# Nonblocking hash-table snapshot design

## Status and scope

This is the proposed architecture and top-level specification for
[task 39570726](https://msazure.visualstudio.com/One/_workitems/edit/39570726),
under [PBI 5766647](https://msazure.visualstudio.com/One/_workitems/edit/5766647).
It does not change production behavior. The existing
[hash-table requirements](clds_hash_table_requirements.md) remain the
specification of the implemented API until the final integration PR.

The objective is to replace the write suspension in
`clds_hash_table_snapshot` with a transient copy-before-change journal and
hazard-safe enumeration. Preserve the existing public API, result values,
node ownership, sequence-number behavior, and per-table snapshot semantics.
Do not introduce a second hash-table implementation or require caller API
changes.

**Progress decision, September 2026:** a snapshot may wait indefinitely for a
writer-quiescent cut. It must not close writer admission to obtain that cut.
After the cut, writers continue during enumeration and merging. Cancellation
is supported; a new `BUSY` result and an automatic fallback to the old write
lock are not.

This decision replaces the earlier proposal to advance a phase while old-phase
writers are still executing. The distinction is necessary for correctness,
not just performance.

## Current implementation

The following observations were checked against CLDS commit
`bed894edbd40c0eddd5b1835775d415b23a550b0`.

| Surface | Relevant behavior |
|---|---|
| `src/clds_hash_table.c`: `check_lock_and_begin_write_operation`, `internal_lock_writes`, `clds_hash_table_snapshot` | Snapshot closes writer admission, drains writes, and holds the gate throughout enumeration. |
| `src/clds_hash_table.c`: `clds_hash_table_find` | Finds do not enter the snapshot write gate. |
| `src/clds_hash_table.c`: `get_first_bucket_array` | Resize prepends a bucket array; existing nodes are not migrated and old arrays survive until table destruction. |
| `src/clds_hash_table.c`: insert and set | `pending_insert_count` and older-level searches prevent duplicate keys across resize levels. |
| `src/clds_sorted_list.c`: insert, `internal_delete`, `internal_remove`, set | Successful incoming-link CAS publishes, unlinks, or replaces a node. A marked `next` prevents conflicting structural changes. |
| `src/clds_sorted_list.c`: `clds_sorted_list_find_key` | A validated hazard pointer protects acquisition of a durable item reference. |
| `src/clds_sorted_list.c`: `clds_sorted_list_get_all` | Enumeration assumes stable structure; it does not protect every node with hazard pointers. |
| `src/clds_sorted_list.c`: `reclaim_list_node`, `internal_node_destroy` | HP reclamation drops the table's item reference; the final reference invokes cleanup and frees the node. |
| `inc/clds/clds_hash_table.h` | Nodes are intrusive, and keys are raw caller-provided pointers. Neither keys nor payloads are cloned. |

Sequence numbers are reserved before publication and may be skipped. They are
not commit timestamps. Concurrent `item_count` values are neither snapshot
versions nor safe fixed output capacities.

An item returned by find, remove, replacement, or snapshot can outlive its
membership in the table. An item reference preserves allocation lifetime; it
does not freeze payload bytes or make its old `next` a safe traversal source.

### Why advancing a phase and draining afterward is incorrect

Consider an initially empty table:

1. Writer A registers in generation G, intending to insert X, then pauses.
2. Snapshot S advances to G+1 without waiting for A.
3. Writer B inserts Y in G+1 and returns.
4. A subsequent find of X returns not-found.
5. A inserts X in G and returns.
6. S drains G and filters out G+1 items, returning X but not Y.

The find forces X's insertion after the observation that followed Y's
insertion. A snapshot containing X but not Y cannot be linearized in that
history. Adding more rotating counters, or journaling only according to a
writer's registration phase, does not fix it.

The corrected design advances the generation only in an atomic state with
zero registered writers. No old-generation publication can cross that CAS.

## Top-level proposed requirements

`SNAP-*` identifiers are design-level requirements, not active `SRS_*` tags.
The module-specification PRs will allocate traceable SRS identifiers. The
integration PR will retire superseded requirements rather than rewriting their
meaning or pretending unimplemented requirements have code/test coverage.

| ID | Requirement |
|---|---|
| SNAP-001 | A successful snapshot shall return the complete key-to-item mapping at one instant between its invocation and response. |
| SNAP-002 | The result shall contain at most one item for each logical key. |
| SNAP-003 | The result shall own one reference to each returned item. |
| SNAP-004 | A successful empty result shall set `items` to `NULL` and `item_count` to zero. |
| SNAP-005 | The public snapshot signature and existing result enumeration shall remain unchanged. |
| SNAP-006 | Snapshot execution shall not close admission of hash-table writers. |
| SNAP-007 | Snapshot bookkeeping failure shall not by itself fail an otherwise valid mutation. |
| SNAP-008 | A failed or abandoned snapshot shall not publish a partial successful result. |
| SNAP-009 | Snapshot order shall remain unspecified. |
| SNAP-010 | Snapshot consistency shall concern membership and item identity, not historical copies of mutable payload bytes. |
| SNAP-011 | Snapshot generation shall be independent of optional operation sequence numbers. |
| SNAP-012 | Cancellation shall be checked while waiting for snapshot leadership or a cut. |
| SNAP-013 | Cancellation shall be checked during enumeration and merge. |
| SNAP-014 | Snapshot cleanup shall release every reference not transferred to the caller. |
| SNAP-015 | Concurrent snapshot callers shall be serialized without introducing `BUSY`. |
| SNAP-016 | A successful cut shall exclude every subsequently published membership. |
| SNAP-017 | An item removed or replaced after the cut shall remain available to that snapshot. |
| SNAP-018 | Resize after the cut shall not hide a cut-visible item. |
| SNAP-019 | Same-item set shall preserve the publication generation of unchanged membership. |
| SNAP-020 | Snapshot generation shall not wrap or be reused after a successful cut. |
| SNAP-021 | No successor shall be followed through an invalidated or marked predecessor. |
| SNAP-022 | Cancellation shall not bypass safe retirement of journal users. |
| SNAP-023 | A snapshot shall not invoke item cleanup while retaining snapshot leadership. |
| SNAP-024 | Existing resize-level duplicate prevention shall remain intact. |

The guarantee is per table. It does not provide one atomic cut across several
hash tables or make concurrent table destruction legal.

## Architecture

Four independently testable pieces remain unused by the production hash table
until the integration PR:

| Piece | Responsibility | Boundary |
|---|---|---|
| Snapshot generation domain | Writer entry/exit, atomic quiescent cut, snapshot leadership, counter exhaustion | No list traversal or ownership of item references |
| Transient snapshot journal | Stable registration slot, append ownership, poisoning, close/drain, detached cleanup | No mutation or key lookup |
| Concurrent sorted-list enumeration | Validated HP traversal and reference delivery | No claim of point-in-time consistency by itself |
| Snapshot collector | Dynamic storage, deduplication, journal merge, output transfer | No coordination of writers |

The hash-table integration owns publication-generation metadata and connects
destructive sorted-list publication sites to the journal. Merely adding a
callback around a hash-table API call is insufficient: the old item must be
retained before its mark/unlink/replacement inside the sorted-list algorithm.

### Generation domain

Use one atomic 64-bit word encoding a nonnegative 31-bit generation and a
32-bit active-writer count; leave the sign bit unused. Pseudocode below uses
field notation, not C arithmetic on packed bitfields. Use existing c-pal
interlocked operations, with checked field arithmetic.

```text
writer_enter:
    repeat:
        old = atomic_load(domain)
        if old.count is exhausted:
            log and return the operation's existing ERROR before side effects
        if CAS(domain, old, {old.generation, old.count + 1}) succeeds:
            return old.generation

writer_leave:
    CAS-loop decrement of count, preserving generation

try_cut(G):
    return CAS(domain, {G, 0}, {G + 1, 0})
```

Every insert, delete, delete-key-value, remove, and set enters before any
membership/resize mutation and leaves on every exit path, after publication,
count maintenance, and sequence-number bookkeeping. Conditional failure,
allocation failure, and CAS retries do not leak participants. The outer hash
operation registers once, not once per bucket attempt. Find remains ungated.

A writer that loaded `{G, 0}` but loses to the cut retries registration in G+1.
A writer that wins admission first makes the cut CAS fail. There is no
check-zero-then-increment-generation interval using separate atomics.

Only the snapshot caller backs off when the cut fails. It may use finite waits
to avoid spinning, but cannot depend on a missed wakeup or an infinite wait
that prevents cancellation checks. Writer completion is not conditional on
snapshot completion.

At generation exhaustion, log and fail future snapshots before arming a new
cut. Normal mutations remain possible in the last generation. Do not reset the
generation while the table is alive. Count exhaustion is a checked structural
limit, not the handling for journal allocation failure.

### Publication metadata and node lifetime

An actual insertion or replacement initializes the incoming node's internal
`publication_generation` before its linking CAS. Failed publication transfers
no item ownership. An unchanged same-item set must not rewrite its generation
as G+1: doing so would hide a cut-visible node.

Move generation/key preparation to paths that distinguish a fresh publication
from same-item set; do not unconditionally overwrite a linked node's internal
metadata in the hash-table wrapper before the sorted-list call. Preserve the
existing special-case reference behavior for same-item set.

The proof requires a membership incarnation to retain a stable key and
generation while any traversal or snapshot can observe it. Holding a reference
does not make arbitrary reinsertion of the same intrusive node safe:

- A fresh node may be inserted for the same logical key after deletion.
- The old snapshot then returns the old node; current find returns the new one.
- Same-item set of unchanged membership remains supported.
- A removed physical node may be republished only after previous traversals,
  HP retirement ownership, and all other references to its old incarnation
  have ended, with the republishing caller retaining exclusive ownership.
  Otherwise rewriting `next`, key, or generation can cause ABA, stale-link
  validation, or loss of historical identity.
- Raw key storage must remain valid and comparison-stable while its retained
  item is used by the snapshot. Copying a `void*` into a journal does not clone
  the key or preserve externally freed memory.

**Compatibility gate:** the current public requirements do not fully specify
these reuse/lifetime rules. Module/integration review must make them explicit
and audit supported consumers. Do not silently
classify a supported reuse pattern as invalid to make the proof work. If a
consumer requires republishing a node while its old incarnation is retained,
revise the representation to use immutable internal membership records before
activation. That is additional scope, not something a captured raw pointer
solves. Caller adoption remains dependency-only unless that audit finds a concrete
compatibility issue requiring separate approval.

Adding internal metadata to macro-defined nodes changes their layout. Preserve
source-level API compatibility and rebuild consumers with the dependency.
Do not claim binary compatibility between differently compiled node layouts.

### Stable journal registration slot

Keep the registration control word embedded in the table-owned journal handle,
alive until table destruction. Do not load a removable descriptor pointer and
then increment its refcount: it might already be freed.

The control word has checked fields:

```text
{epoch: 31 bits, state: 2 bits, failed: 1 bit, users: 29 bits}
```

The sign bit is unused. States are `CLOSED`, `OPEN`, and `CLOSING`, with one
reserved encoding. `failed` is orthogonal to state so poisoning cannot reopen
registration. `epoch` is the post-cut writer generation G+1. The separate
journal payload is readable only after a successful registration.

The snapshot leader initializes payload storage while the slot is closed with
zero users, then release-publishes `OPEN(G+1, false, 0)` **before** attempting
the generation cut. Before a successful cut, all writers still belong to G and
therefore ignore the armed slot. After the cut, every eligible writer sees an
already initialized slot.

A writer joins by CAS on the whole control word only if state is OPEN and epoch
matches its writer generation. It preserves state/epoch/failed when changing
the user count. A stale CAS cannot join a later successful cut. An attempt
canceled before cutting can reuse its proposed epoch because no writer could
have registered in that epoch.

On CLOSING/CLOSED, the writer proceeds without journaling. Normal closure starts
only after traversal is finished; premature closure means the snapshot has
already been abandoned or failed. Neither case requires later unlinks in its
result.

The embedded slot also allows poisoning through a stamped CAS without touching
the payload if journal-user capacity is exhausted. If closure wins that CAS,
no new journal obligation remains. Do not fail the mutation or dereference
unprotected payload storage on that path.

### Copy before destructive change

For a writer in G+1 and an old node published at or before G:

1. Obtain and validate normal HP protection for the old node.
2. Join the matching OPEN journal slot.
3. Take an extra reference and capture `{item, key, publication_generation}`.
4. Publish a fully initialized journal entry.
5. Leave journal registration.
6. Only then attempt the node mark and incoming-link unlink/replacement CAS.

If the node is post-cut, no cut-visible membership needs retaining. If the
mutation loses a later CAS, the conservative entry is harmless: it still
describes a node that existed at the cut. Multiple attempts may log it.
Never claim "already journaled" before an owning entry is published; another
writer must not unlink while the first logger is paused with an unpublished
reservation.

The extra reference is acquired while the writer has valid HP protection.
Journal registration can end before the eventual unlink because the published
entry already preserves the old item. The normal mutation retains its own
protection independently.

Use append-only segments/lists with fully initialized entry publication.
Segment allocation and append ownership belong to the journal module, not to
undocumented fields in the HP implementation. Per-thread or sharded segments
should avoid a single global append bottleneck; their exact allocation policy
and memory budget belong in that module's specification and perf qualification.
Thread registration/unregistration and segment teardown require tests.

On allocation, capacity, or reference-acquisition failure:

1. Atomically set failed on the matching slot before an unjournaled mutation.
2. Release any untransferred bookkeeping ownership safely.
3. Continue the normal mutation.
4. Return snapshot ERROR after closing and draining its users.

The journal must not wait for the snapshot to free space. A full journal poisons
the snapshot instead. All size arithmetic is checked; no overflowing allocation
or silent entry loss is allowed.

### Concurrent enumeration

Leave `clds_sorted_list_get_all` and its existing callers unchanged until
integration. The new enumerator needs the following protocol:

1. Start at the bucket head.
2. Load the incoming pointer, retaining the predecessor's HP when applicable.
3. Treat a marked incoming link as invalid, including a marked null tail.
4. Acquire an HP for a non-null candidate.
5. Re-read the full incoming link and require the same unmarked value.
6. Only then access candidate metadata and acquire an eligible item reference.
7. To advance, keep the current node protected, load its unmarked `next`,
   protect the successor, and revalidate that incoming link before use.
8. On a mark or validation failure, release traversal HPs and restart at head.
9. Validate an unmarked null tail before declaring that pass complete.

Do not just mask off the deletion bit and follow a dead node's successor.
Keeping the dead node alive with a refcount does not keep the successor alive.
The predecessor remains HP-protected until successor protection/validation is
complete. Unlinked nodes must not have their links rewritten through premature
reuse, as specified above.

A finite completed pass is sufficient for a bucket. Nodes missed because of
post-cut unlink/replacement are covered by the journal. Repeated observations
are allowed and deduplicated. Continuous interference may starve a pass; check
cancellation on restart and inside long buckets, not only between buckets.

Transfer acquired references into the collector or its deferred-cleanup list.
Do not release a potentially final application reference while holding snapshot
leadership. Ineligible nodes can be examined under HP protection without taking
a durable reference.

Unwinding must not allocate merely to remember a reference that needs releasing.
Reserve collector ownership storage before acquisition where possible. If a
handoff fails after acquisition, retain that reference in an operation-local
pending slot and abort through the common detach/cleanup path.

### Snapshot and merge sequence

```text
acquire snapshot leadership (cancelable; writers do not participate)
initialize payload and arm OPEN slot for G+1
retry CAS {G, 0} -> {G+1, 0}, checking cancellation
capture current bucket-array root
enumerate every bucket in that root chain
close slot OPEN -> CLOSING, preserving failed and users
drain registered users
if successful, merge journal entries and collected observations
detach result and all cleanup ownership; mark slot CLOSED
release snapshot leadership
release detached non-result references and storage
return OK / ERROR / ABANDONED
```

The successful generation CAS is the snapshot linearization point. Capturing
the root after that CAS includes all pre-cut levels. Extra levels published in
the intervening interval contain only post-cut publications and are harmless.
Arrays prepended afterward cannot hide old nodes. Do not skip buckets or levels
based on changing `item_count`.

The collector filters on publication generation at or before G and merges by
the table's logical key comparison, not raw key-pointer equality. Different key
objects may compare equal. Snapshot deduplication must hold the owning item
reference that keeps each comparison key valid.

Under the lifetime rules and quiescent cut, all eligible observations for a
logical key identify the same cut-visible item. A journal copy takes precedence
over a duplicate direct observation; release the duplicate reference later.
If two distinct eligible item identities compare equal, report an invariant
failure and fail the snapshot rather than selecting an arbitrary winner.

Do not assume `clds_st_hash_set` is usable without examining its equality
contract. A pointer-identity set could deduplicate retained item pointers only
after proving the one-item-per-cut-key invariant; it is not a substitute for
arbitrary caller key equality.

Use checked dynamic growth. Live counts may be sizing hints, never correctness
conditions. On success detach exactly one reference per unique result and the
allocated pointer array into the existing caller contract. On error do not
write successful output values; retain the existing rule that outputs are
consumed only on OK.

### Closure, failure, cancellation, and reentrancy

Close registration with a CAS on the stamped state/user word. An already joined
writer either publishes its entry or poisons the descriptor before leaving.
Drain users with acquire ordering, then inspect failed and consume the journal.
No registration may reopen the slot.

The fail flag is authoritative even if most of the output has been built.
No partial success, silent truncation, write-lock fallback, or retry disguised
as success is allowed. Diagnostic paths distinguish cut exhaustion, allocation,
HP acquisition, size overflow, and invariant failures.

Cancellation before the cut closes the armed slot without advancing the
generation. Cancellation after the cut does not roll the generation back.
Both paths detach all references and close/drain before reclamation.
Cancellation cannot force reclamation beneath a stalled registered journal
writer; post-cut cleanup therefore has no hard latency bound.

Checking cancellation earlier than the legacy per-bucket check is intentional.
A token observed while waiting for a cut may produce ABANDONED even if the
table would later be found empty. Success can race a cancellation not yet
observed. No new result value is introduced.

Detach cleanup lists and release snapshot leadership before dropping references
that could invoke application cleanup. This applies on success, failure, and
cancellation, and includes duplicate and temporary collector ownership. Cleanup
may reenter ordinary table operations without depending on the old journal
payload. Do not use that payload after leadership is released.

As with the current blocking implementation, callbacks invoked inside an
unfinished mutation must not synchronously request a snapshot of the same table
or destroy it: the caller would be waiting for its own writer participation.
Do not claim this design makes arbitrary recursive callbacks safe.

### Memory ordering and lifetime

Initially use the repository's interlocked primitives with full ordering;
weakening to acquire/release is separate reviewed work. Required edges are:

| Publication | Required observer |
|---|---|
| Payload initialization before OPEN publication | Successfully registered journal writer |
| Node key/generation initialization before linking CAS | Validated enumerator or subsequent mutation |
| Journal entry initialization/ref ownership before append publication | Snapshot after user drain |
| Append or failed flag before destructive mutation | Snapshot decision and retained old-item lifetime |
| Final mutation bookkeeping before writer count decrement | Successful zero-writer cut |
| User completion before decrement | Closing snapshot observing zero users |

Only registered writers dereference an OPEN slot's payload. The stable control
word outlives all operations. HPs protect list dereferences; ordinary references
protect retained result/journal items. These are different lifetime mechanisms.
Destruction still requires external exclusion of all table operations.

## Correctness argument

Let T be the successful CAS from `{G, 0}` to `{G+1, 0}`.

**Cut:** every G writer has finished before T, and no later writer can register
in G. In particular, no delayed old-generation insert or resize can publish
after T. Existing mutation/find ordering is not replaced by registration order.

**Soundness:** every accepted node was published no later than G. A validated
post-T observation of that incarnation, or a journal entry made before its
post-T removal, therefore refers to membership present at T. Pre-T removals
are no longer reachable through validated live links and were not journaled
for this cut. Post-T publications are filtered out.

**Completeness:** take an item present at T. If it remains linked through the
completed pass over its bucket, validated traversal observes it. If it becomes
unreachable before it is observed, the destructive operation published an
owning journal entry first, or set failed. Thus a successful result cannot lose
it between table and journal. Resize neither migrates nor destroys its level.

**Uniqueness:** a logical key has one membership at T. Repeated traversal,
conservative logging, and lost mutation CAS attempts produce extra references
to that membership, not additional cut-visible values. Key deduplication leaves
one caller-owned reference.

**Closure:** every writer that could remove an unobserved cut-visible node
before traversal ended either registered and is drained, or lost to closure
after the completed pass. Later removals cannot invalidate already retained
direct observations. Abort closure never claims success.

This proof depends on the existing map's uniqueness guarantees, the specified
node/key lifetime contract, validated finite passes, and the ordering edges
above. It is not a proof that unsafe node reuse or arbitrary payload mutation
becomes safe.

## Progress and cost

No snapshot-created state prevents a writer from registering or publishing.
Writers incur registration CAS contention and, while a snapshot is active,
additional bookkeeping plus journal allocation/append work. Allocators, callbacks,
existing marked-node retries, and resize-level pending-insert waits still have
their existing progress properties; do not call the whole library wait-free or
claim to fix all existing stalled-thread dependencies.

A snapshot can starve before T under continuously overlapping writers, or
during traversal under sustained interference. A stalled writer can prevent
the cut; a stalled journal user can delay cleanup. This is the accepted tradeoff.
No finite snapshot latency or successful completion under perpetual writes is
promised. Measure cut wait separately from materialization.

Steady-state additions are a packed participant word, a stable per-table journal
slot/leadership state, and per-node generation metadata. There are no permanent
per-key MVCC chains or history scans. Snapshot memory is output storage plus
deduplication and journal entries; repeated failed mutation attempts can increase
journal volume. Capacity limits must fail the snapshot, not block writers.

## Compatibility and implementation surfaces

Keep the public snapshot declaration and OK/ERROR/ABANDONED values. Preserve
insert transfer, find/remove/set returned ownership, skipped-sequence callbacks,
same-item set behavior, existing count maintenance, and resize duplicate checks.

Integration changes include hash-table writer entry/exit; insert and replacement
metadata preparation; destructive sorted-list hooks; new enumeration; snapshot
materialization and teardown; the public node layout; reals/mocking surfaces;
and corresponding requirements and tests. Existing generic sorted-list callers
must not pay for hash-table journaling or have `get_all` semantics changed.

At integration, retire the active write-gate requirements, including snapshot
requirements `SRS_CLDS_HASH_TABLE_42_017`, `SRS_CLDS_HASH_TABLE_42_018`, and
`SRS_CLDS_HASH_TABLE_42_030`, plus the mutator admission/wait requirements.
Replace stable-count enumeration requirements such as
`SRS_CLDS_HASH_TABLE_01_114` and `SRS_CLDS_HASH_TABLE_42_026` with new identifiers.
Preserve public validation/output requirements where still applicable. Update
the source and tests with matching traceability text in that same PR.

List operations already call this API. The intended EBS
production change is a dependency update only. Retain the caller's checkpoint
`index_lock`: it coordinates multiple tables, committed state, and maximum
address, not merely one hash-table enumeration. Combined local/offload results
remain separate per-table cuts. Shared-domain snapshots and checkpoint-lock
removal are not included.

## Validation and acceptance

Tests use deterministic synchronization points, not sleeps to infer ordering.
The model oracle records successful publication/unlink events, invocation and
response ordering, find observations, and the successful cut. It must compare
against the actual map at T, not merely replay generation tags from the algorithm
being tested.

| Scenario | Required outcome |
|---|---|
| Old writer paused before publication; another writer/find interleave | No cut until old writer leaves; reject the phase-only counterexample |
| Writer loads old packed state; cut wins its CAS | Writer retries in the new generation |
| Writer admission wins before cut | Cut retries without delaying writer |
| Continuous overlapping writers | Writers keep progressing; cancelable snapshot is allowed to remain pending |
| Armed snapshot canceled before cut | No epoch advance, no registered future-epoch users, no leaked payload |
| Post-cut insert/set-absent | New membership is absent from the result |
| Delete/remove/replace before scanner reaches old item | Old item is retained in journal and returned |
| Lost mark/unlink CAS after append | No duplicate output or lost reference |
| Same-item set after cut | Existing membership remains visible; generation is not retagged |
| Same key deleted and reinserted using a new item | Snapshot sees old item, find sees current item |
| Physical-node reuse and externally owned keys | Lifetime contract is validated; supported consumers are not silently broken |
| Current node or successor removed; marked null tail | Restart safely; no stale-successor dereference or false completed pass |
| Collector observes a node repeatedly | One result reference; all duplicates accounted for |
| New bucket levels before/after T | All cut-visible memberships included exactly once |
| Writer paused after registration or append | Close/drain remains safe; append-before-unlink invariant holds |
| Journal allocation/capacity failure | Mutation proceeds, snapshot fails, all acquired refs eventually released |
| Collector/HP failure and cancellation at each stage | No partial success, no leaked refs/users/leadership |
| Back-to-back snapshots and delayed registration CAS | No stale descriptor access or epoch ABA |
| Generation/count boundary values | No wrap, carry into another field, or silent overflow |
| Final reference invokes reentrant cleanup | Leadership released and payload detached before callback |
| Threshold-1 reclamation and thread unregister | No UAF, double release, or retained orphan journal segments |

Before integration, baseline tests must pass against the legacy implementation.
Scenarios that specifically require nonblocking materialization become active
with integration; do not check in disabled tests or permanently failing future
expectations.

Retain the existing 1,500,000-item snapshot performance test's under-1,000-ms
requirement for its quiescent workload. Record writer throughput and
P50/P99/P99.9 latency, cut wait, materialization, cancellation latency, traversal
restarts, journal bytes, and retained-node high-water marks with 1/8/32/64 writers.
Hold a snapshot deliberately inside enumeration and require concurrent mutations
to complete before releasing it; total throughput alone is not proof of progress.

Measure the no-snapshot regression separately. Performance tolerances and journal
budgets need recorded baseline data and reviewer agreement before activation;
this document does not invent an unmeasured percentage or default cap. Run the
normal leak checks, sanitizer/verification configurations, sequence-number tests,
resize chaos, and downstream caller regressions. VLD is part of definition-of-done.

## PR-sized delivery plan

| ADO task | Deliverable |
|---|---|
| 39570726 | This architecture and proposed top-level specification |
| 39570727 / 39570728 | Generation-domain specs, then code/tests; use the atomic quiescent cut, not the superseded three-phase drain |
| 39570729 / 39570730 | Journal specs, then code/tests, including stable registration and detached cleanup |
| 39570731 / 39570733 | Concurrent enumeration specs, then code/tests; existing callers unchanged |
| 39570734 / 39570735 | Collector specs, then code/tests |
| 39570736 | Legacy correctness/stress/performance baselines and reusable scheduling infrastructure |
| 39570738 | Integrate all pieces, update live SRS/code/tests together, and activate the complete protocol |
| 39570739 | EBS write workload with key-list, block-list, and mixed list storms; capture legacy baseline |
| 39570740 | Update EBS dependency and rerun caller/copy/checkpoint/list and perf qualification |

Module specs follow architecture approval. Their implementations can land
independently while unused. Baselines can proceed in parallel. Integration
depends on all four module implementations and the CLDS baseline. EBS adoption
depends on integrated/released CLDS and its list-storm baseline.

The old ADO generation-domain and integration descriptions mention advancing
before old-phase drain. This design supersedes that mechanism; update those task
descriptions when taking them up rather than implementing the obsolete wording.

Every preceding PR keeps master shippable without a partially enabled protocol.
No opt-in caller API migration or v2 table is planned. If representation or
compatibility review invalidates these boundaries, revise the design before
changing production behavior. Reverting final integration restores the old
snapshot without removing the already-unused standalone modules.

## Review gates and remaining specification work

Architecture approval must include the accepted starvation tradeoff and the
node/key-lifetime compatibility gate. Module specs must make exact APIs, capacity
policy, allocation ownership, counter arithmetic, and ordering testable. Before
production activation, reviewers require:

- Consumer evidence for lifetime/reuse assumptions, including same-item set.
- Deterministic and model-based evidence for the cut and snapshot contents.
- Safe descriptor closure/reentrancy and HP traversal under aggressive reclamation.
- Preserved sequence/resize behavior and measured no-snapshot overhead.
- Documented budgets/performance acceptance and baseline comparisons.

The earlier 8-12 engineer-week implementation/qualification estimate plus
2-4 calendar weeks of soak is provisional, not a result of this design PR.
Snapshot starvation under representative caller workloads must be measured. If it is
unacceptable, revisit the progress contract: writer pausing, helping descriptors,
or persistent versioned structures are different designs, not an optimization
that can be added to the cut without a new correctness argument.
