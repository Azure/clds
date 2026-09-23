# Nonblocking hash-table snapshot design

## Scope

This document specifies nonblocking snapshots for `clds_hash_table`.
The [hash-table requirements](clds_hash_table_requirements.md) describe the
existing public API.

The objective is to replace the write suspension in
`clds_hash_table_snapshot` with a transient copy-before-change journal and
hazard-safe enumeration. Preserve the existing public API, result values,
node ownership, sequence-number behavior, and per-table snapshot semantics.
Do not introduce a second hash-table implementation or require caller API
changes.

**Progress contract:** a snapshot may wait indefinitely for a
writer-quiescent cut. It must not close writer admission to obtain that cut.
After the cut, writers continue during enumeration and merging. Cancellation
is supported; a `BUSY` result and an automatic fallback to a write
lock are not.

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

## Requirements

- A successful snapshot shall return the complete key-to-item mapping at one instant between its invocation and response.
- The result shall contain at most one item for each logical key.
- The result shall own one reference to each returned item.
- A successful empty result shall set `items` to `NULL` and `item_count` to zero.
- The public snapshot signature and existing result enumeration shall remain unchanged.
- Snapshot execution shall not close admission of hash-table writers.
- Snapshot bookkeeping failure shall not by itself fail an otherwise valid mutation.
- A failed or abandoned snapshot shall not publish a partial successful result.
- Snapshot order shall remain unspecified.
- Snapshot consistency shall concern membership and item identity, not historical copies of mutable payload bytes.
- Snapshot epochs shall be independent of optional operation sequence numbers.
- Cancellation shall be checked while waiting for snapshot leadership or a cut.
- Cancellation shall be checked during enumeration and merge.
- Snapshot cleanup shall release every reference not transferred to the caller.
- Concurrent snapshot callers shall be serialized without introducing `BUSY`.
- A successful cut shall exclude every subsequently published membership.
- An item removed or replaced after the cut shall remain available to that snapshot.
- Resize after the cut shall not hide a cut-visible item.
- Same-item set shall preserve the publication epoch of unchanged membership.
- An epoch identity shall not be reused while referenced by a node or operation.
- No successor shall be followed through an invalidated or marked predecessor.
- Cancellation shall not bypass safe retirement of journal users.
- A snapshot shall not invoke item cleanup while retaining snapshot leadership.
- Existing resize-level duplicate prevention shall remain intact.

The guarantee is per table. It does not provide one atomic cut across several
hash tables or make concurrent table destruction legal.

## Architecture

Four components coordinate the snapshot boundary, preserve removed items, and
assemble the result:

| Piece | Responsibility | Boundary |
|---|---|---|
| Snapshot epoch domain | An admission word counts writers and selects one of two owning epoch-reference slots; switching slots at zero writers establishes the cut | Controls membership epochs, not journal append registration |
| Transient snapshot journal | A table-lifetime registration control word protects a per-snapshot payload containing the target epoch and retained old items | Controls who can append and when that payload can be consumed or freed |
| Concurrent sorted-list enumeration | Validated HP traversal and reference delivery | No claim of point-in-time consistency by itself |
| Snapshot collector | Dynamic storage, deduplication, journal merge, output transfer | No coordination of writers |

The hash table coordinates these components. Snapshot leadership serializes
snapshot callers only; it is not a lock acquired by mutators. An epoch identifies
a period of publication. Nodes retain their publication epoch so the collector
can distinguish items present at the cut from later insertions.

The snapshot lifecycle is:

1. The snapshot caller acquires leadership, prepares a fresh epoch N in the
   inactive epoch slot, and opens a journal payload for N.
2. At a writer-free instant, the epoch domain atomically switches the selected
   slot to N. This is the cut. Writers continue attempting admission throughout;
   writers admitted afterward publish nodes tagged N.
3. The snapshot captures the bucket-array root and enumerates it. Concurrent
   writers copy cut-visible old items into the journal before removing or
   replacing them. The enumerator supplies references to cut-visible items
   still linked in the table; it excludes new items tagged N.
4. After traversal, the snapshot closes journal append registration and drains
   writers already using that payload. The collector combines direct
   observations and journal entries into one result.
5. The snapshot detaches result and cleanup ownership, releases leadership, and
   releases non-result references.

The two epoch-reference slots belong to writer admission. The journal's
registration control word is separate: it protects append/payload lifetime,
not the table's writer count. Closing it does not stop mutations.

### Epoch domain and quiescent cut

An epoch is a reference-counted record whose address supplies identity. It has
no increasing generation number. Every published node owns a reference to its
`publication_epoch`. A snapshot introduces a fresh epoch N; publications made
after its cut use N. While that snapshot owns leadership, no other cut occurs,
so a node is cut-visible exactly when its publication epoch differs from N
and its membership is observed directly or retained in the journal.

The table owns two epoch-reference slots and one atomic 64-bit admission word:

```text
epoch_slots[2]: owning references; the selected slot is always initialized
admission: {selected_slot: 1 bit, active_writers: 32 bits}
```

Unused bits, including the sign bit, remain zero. The selector identifies the
current slot; it is not an epoch identity stored on nodes. It alternates on
every successful cut without exhausting a lifetime snapshot counter.

A writer first registers through the admission word and only then reads the
selected epoch reference. Its active-writer participation prevents a cut and
keeps that selected slot unchanged until it leaves. The writer borrows the
slot's epoch reference for its operation; any node it publishes acquires its
own reference before the linking CAS.

```text
writer_enter:
    repeat:
        old = atomic_load(admission)
        if old.count is exhausted:
            log and return the operation's existing ERROR before side effects
        if CAS(admission, old, {old.slot, old.count + 1}) succeeds:
            return {slot = old.slot, epoch = epoch_slots[old.slot]}

writer_leave:
    CAS-loop decrement of count, preserving selected_slot

prepare_cut:  // snapshot leader only
    S = atomic_load(admission).slot
    allocate fresh epoch N
    replace epoch_slots[1-S] with an owning reference to N
    initialize and open the journal for target_slot = 1-S and epoch N

try_cut(S):
    return CAS(admission, {S, 0}, {1-S, 0})
```

Use existing c-pal interlocked operations and checked field arithmetic.
Publishing the new slot reference precedes the cut CAS. A writer that registers
after that CAS therefore reads the initialized N. Only the snapshot leader
replaces slots, and it writes only the inactive slot.

Every insert, delete, delete-key-value, remove, and set enters before any
membership/resize mutation and leaves on every exit path, after publication,
count maintenance, and sequence-number bookkeeping. Conditional failure,
allocation failure, and CAS retries do not leak participants. The outer hash
operation registers once, not once per bucket attempt. Find remains ungated.

A writer that loses admission to the cut retries against the current word.
A writer that wins admission first makes the cut CAS fail. There is no
check-zero-then-switch-slot interval using separate atomics.

A thread paused before admission may see the same selector/count after two or
more cuts. That ABA is harmless: its CAS registers against the current state,
and it then loads the current slot reference. It must not cache an epoch pointer
before successful registration. Once registered, a writer prevents another cut,
so neither its slot nor its borrowed epoch can change.

Only the snapshot caller backs off when the cut fails. It may use finite waits
to avoid spinning, but cannot depend on a missed wakeup or an infinite wait
that prevents cancellation checks. Writer completion is not conditional on
snapshot completion.

#### Epoch ownership and reclamation

The current slot owns its epoch. The inactive slot can hold a previous epoch
until the leader replaces it; replacement releases that slot's old reference.
Nodes retain their publication epoch through removal and through all retained
snapshot/item references. Node destruction releases the epoch after its last
use. Journal payloads own their target epoch until detached cleanup finishes.
These references use checked reference-count arithmetic.

An epoch record is freed when its final reference is released. A newly allocated
record may reuse its address only then: no node or active operation can still
compare against the old identity. Epoch release frees only internal metadata,
not application items, and invokes no application callback.

There is no scan to retag long-lived nodes and no eventual "snapshots disabled"
state. An old node keeps its old epoch alive and differs from every fresh N.
Records are retained only by the two slots, an active or detached snapshot
payload, and live or externally retained nodes, not by the total number of cuts.
Repeated snapshots without publications do not accumulate epoch history.

If epoch allocation fails, the snapshot returns ERROR without changing the
selected slot; a later snapshot can retry. Cancellation before the cut clears
the prepared inactive slot and releases its N references after journal closure.
After a successful cut, N remains the current epoch even if the snapshot fails.
Table destruction releases both slot references; retained result nodes continue
to own any epochs they need.

Active-writer count exhaustion is a checked simultaneous-participant limit.
Unlike a cumulative generation count, it becomes available again when writers
leave and cannot permanently disable snapshots through repeated use.

#### Why the cut requires zero writers

The cut switches the selected epoch only in an atomic state with zero registered
writers. No old-epoch publication can cross that CAS. Switching first
and draining old writers afterward would permit the following history:

1. Writer A registers in epoch E, intending to insert X, then pauses.
2. Snapshot S switches to N without waiting for A.
3. Writer B inserts Y in N and returns.
4. A subsequent find of X returns not-found.
5. A inserts X in E and returns.
6. S drains E and filters out N items, returning X but not Y.

The find forces X's insertion after the observation that followed Y's
insertion. A snapshot containing X but not Y cannot be linearized in that
history. Registering writers in phases cannot substitute for a publication
boundary at the cut.

### Publication metadata and node lifetime

Node creation initializes `publication_epoch` to `NULL`. Before an actual
insertion or replacement linking CAS, the unpublished node acquires an owning
reference to its writer's epoch, releasing any previous unpublished epoch
reference. Failed publication transfers no item ownership: that epoch reference
stays with the caller-owned node until another attempt replaces it or node
destruction releases it. CAS retries within one operation retain the same epoch.
An unchanged same-item set must not retag its epoch as N: doing so would hide a
cut-visible node.

Epoch/key preparation distinguishes a fresh publication from same-item set.
It does not overwrite a linked node's internal metadata in the hash-table
wrapper before the sorted-list call. The special-case reference behavior for
same-item set is unchanged.

The proof requires a membership incarnation to retain a stable key and
epoch while any traversal or snapshot can observe it. Holding a reference
does not make arbitrary reinsertion of the same intrusive node safe:

- A fresh node may be inserted for the same logical key after deletion.
- The old snapshot then returns the old node; current find returns the new one.
- Same-item set of unchanged membership remains supported.
- A removed physical node may be republished only after previous traversals,
  HP retirement ownership, and all other references to its old incarnation
  have ended, with the republishing caller retaining exclusive ownership.
  Otherwise rewriting `next`, key, or epoch can cause ABA, stale-link
  validation, or loss of historical identity.
- Raw key storage must remain valid and comparison-stable while its retained
  item is used by the snapshot. Copying a `void*` into a journal does not clone
  the key or preserve externally freed memory.

The current public requirements do not fully specify reuse/lifetime rules.
Compatibility with supported callers requires that the rules above hold; this
representation does not make premature reuse safe. Supporting republication
while the old incarnation is retained requires separate immutable membership
records. Capturing a raw key pointer cannot provide that separation.

### Journal registration and payload

The journal has a table-lifetime registration control word and a separate
per-snapshot payload. The payload owns the target epoch and appended item
references. Writers register through the control word before accessing that
payload; closure prevents new registrations and waits for existing users.

Embedding the control word in the table-owned journal handle keeps registration
safe even when no payload is active. Loading a removable descriptor pointer
and then incrementing its refcount would instead race descriptor reclamation.

The control word has checked fields:

```text
{target_slot: 1 bit, state: 2 bits, failed: 1 bit, users: 32 bits}
```

Unused bits, including the sign bit, remain zero.
States are `CLOSED`, `OPEN`, and `CLOSING`, with one
reserved encoding. `failed` is orthogonal to state so poisoning cannot reopen
registration. `target_slot` is the slot selected by this snapshot's cut.
The separate journal payload owns N and is readable only after a successful
registration.

The snapshot leader initializes payload storage while the slot is closed with
zero users, then release-publishes OPEN with target slot `1-S` **before**
attempting the cut. Before a successful cut, all writers hold slot S and
therefore ignore the armed journal. After the cut, every eligible writer sees an
already initialized slot.

A writer joins by CAS on the whole control word only if state is OPEN and its
target slot matches the writer's registered slot. It preserves state, target,
and failed when changing the user count. It loads no payload pointer before
joining and finishes all payload access before leaving.

Writer participation keeps the selected slot fixed. During that participation,
the matching journal can close, but the next snapshot can only arm the opposite
slot; it cannot cut. Thus a stale matching registration CAS cannot join a
different snapshot. The single target bit is sufficient for registration,
whereas node visibility uses retained epoch identities, not this bit.
Before-cut cancellations may reuse the inactive selector but cannot have
matching registered writers.

On CLOSING/CLOSED, the writer proceeds without journaling. Normal closure starts
only after traversal is finished; premature closure means the snapshot has
already been abandoned or failed. Neither case requires later unlinks in its
result.

The embedded slot also allows poisoning through a target-checked CAS without touching
the payload if journal-user capacity is exhausted. If closure wins that CAS,
no new journal obligation remains. Do not fail the mutation or dereference
unprotected payload storage on that path.

### Copy before destructive change

For a writer in N and an old node whose publication epoch differs from N:

1. Obtain and validate normal HP protection for the old node.
2. Join the matching OPEN journal.
3. Take an extra reference and capture `{item, key, publication_epoch}`.
4. Publish a fully initialized journal entry.
5. Leave journal registration.
6. Only then attempt the node mark and incoming-link unlink/replacement CAS.

The sorted-list mutation invokes the journal hook while it has validated HP
protection for the old node, before marking or changing its incoming link.
That placement keeps the old item available throughout the transition from
linked membership to journal ownership.

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
avoid a single global append bottleneck. The allocation policy and memory budget
control the number of retained entries; exhaustion poisons the snapshot.
Segment lifetime includes writers joining or unregistering while a snapshot
is active.

On allocation, capacity, or reference-acquisition failure:

1. Atomically set failed on the matching slot before an unjournaled mutation.
2. Release any untransferred bookkeeping ownership safely.
3. Continue the normal mutation.
4. Return snapshot ERROR after closing and draining its users.

The journal must not wait for the snapshot to free space. A full journal poisons
the snapshot instead. All size arithmetic is checked; no overflowing allocation
or silent entry loss is allowed.

### Concurrent enumeration

The enumerator walks mutable bucket lists with validated hazard-pointer
protection:

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
prepare fresh epoch N in inactive slot 1-S and arm its journal
retry CAS admission {S, 0} -> {1-S, 0}, checking cancellation
capture current bucket-array root
enumerate every bucket in that root chain
close journal OPEN -> CLOSING, preserving failed and users
drain registered users
if successful, merge journal entries and collected observations
detach result and all cleanup ownership; mark journal CLOSED
release snapshot leadership
release detached non-result references and storage
return OK / ERROR / ABANDONED
```

The successful admission CAS is the snapshot linearization point. Capturing
the root after that CAS includes all pre-cut levels. Extra levels published in
the intervening interval contain only post-cut publications and are harmless.
Arrays prepended afterward cannot hide old nodes. Do not skip buckets or levels
based on changing `item_count`.

The collector excludes nodes whose publication epoch is N and merges by
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

Close registration with a CAS on the target/state/user word. An already joined
writer either publishes its entry or poisons the descriptor before leaving.
Drain users with acquire ordering, then inspect failed and consume the journal.
No registration may reopen the slot.

The fail flag is authoritative even if most of the output has been built.
No partial success, silent truncation, write-lock fallback, or retry disguised
as success is allowed. Diagnostic paths distinguish epoch allocation,
HP acquisition, size overflow, and invariant failures.

Cancellation before the cut closes the armed journal without changing the
selected epoch. Cancellation after the cut does not switch back to the old epoch.
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

The control words use the repository's interlocked primitives with full
ordering. The required visibility edges are:

| Publication | Required observer |
|---|---|
| Payload initialization before OPEN publication | Successfully registered journal writer |
| Inactive epoch-slot initialization before the cut CAS | Writer registering in the newly selected slot |
| Successful writer admission before loading its epoch reference | Slot remains pinned for that writer |
| Node key/epoch initialization before linking CAS | Validated enumerator or subsequent mutation |
| Journal entry initialization/ref ownership before append publication | Snapshot after user drain |
| Append or failed flag before destructive mutation | Snapshot decision and retained old-item lifetime |
| Final mutation bookkeeping before writer count decrement | Successful zero-writer cut |
| User completion before decrement | Closing snapshot observing zero users |

Only registered writers dereference an OPEN slot's payload. The stable control
word outlives all operations. HPs protect list dereferences; ordinary references
protect retained result/journal items. These are different lifetime mechanisms.
Destruction still requires external exclusion of all table operations.

## Correctness argument

Let T be the successful admission CAS from `{S, 0}` to `{1-S, 0}`, selecting N.

**Cut:** every writer in the preceding epoch has finished before T. Every
subsequent writer loads N after admission, including a thread whose admission
word read was delayed across multiple selector cycles. No delayed old-epoch
insert or resize can publish after T. Existing mutation/find ordering is not
replaced by registration order.

**Soundness:** every accepted node has a publication epoch other than N. No node
could have owned the newly allocated N before T, and every publication after T
uses N until snapshot leadership is released. Retained epoch references prevent
address reuse from confusing these identities. A validated
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
the cut; a stalled journal user can delay cleanup.
No finite snapshot latency or successful completion under perpetual writes is
promised. Measure cut wait separately from materialization.

Steady-state additions are a packed participant word, two epoch-reference slots,
a journal control word and leadership state, and one owning epoch reference
per node. Publication and node destruction acquire/release epoch references.
A snapshot attempt allocates one fresh epoch; epochs are reclaimed when no slot,
payload, or node retains them. There are no per-key MVCC chains or history scans.
Snapshot memory is output storage plus
deduplication and journal entries; repeated failed mutation attempts can increase
journal volume. Capacity limits must fail the snapshot, not block writers.

## Compatibility

Keep the public snapshot declaration and OK/ERROR/ABANDONED values. Preserve
insert transfer, find/remove/set returned ownership, skipped-sequence callbacks,
same-item set behavior, existing count maintenance, and resize duplicate checks.

Hash-table mutations use epoch participation and the destructive-change journal
hooks. Generic sorted-list callers do not incur hash-table journal work, and
`get_all` retains its stable-list contract.

Snapshot consistency covers one hash table. Coordination across tables or with
application state outside the table remains the caller's responsibility.

## Correctness and performance scenarios

Tests use deterministic synchronization points, not sleeps to infer ordering.
The model oracle records successful publication/unlink events, invocation and
response ordering, find observations, and the successful cut. It must compare
against the actual map at T, not merely replay epoch tags from the algorithm
being tested.

| Scenario | Required outcome |
|---|---|
| Old writer paused before publication; another writer/find interleave | No cut until old writer leaves; reject the phase-only counterexample |
| Writer loads old packed state; cut wins its CAS | Writer retries and loads the selected epoch after admission |
| Writer paused before admission across repeated selector cycles | Even if its CAS succeeds on a reused bit pattern, it loads the current epoch |
| Writer admission wins before cut | Cut retries without delaying writer |
| Continuous overlapping writers | Writers keep progressing; cancelable snapshot is allowed to remain pending |
| Armed snapshot canceled before cut | No epoch advance, no registered future-epoch users, no leaked payload |
| Post-cut insert/set-absent | New membership is absent from the result |
| Delete/remove/replace before scanner reaches old item | Old item is retained in journal and returned |
| Lost mark/unlink CAS after append | No duplicate output or lost reference |
| Same-item set after cut | Existing membership remains visible; epoch is not retagged |
| Same key deleted and reinserted using a new item | Snapshot sees old item, find sees current item |
| Physical-node reuse and externally owned keys | Lifetime contract is validated; supported consumers are not silently broken |
| Current node or successor removed; marked null tail | Restart safely; no stale-successor dereference or false completed pass |
| Collector observes a node repeatedly | One result reference; all duplicates accounted for |
| New bucket levels before/after T | All cut-visible memberships included exactly once |
| Writer paused after registration or append | Close/drain remains safe; append-before-unlink invariant holds |
| Journal allocation/capacity failure | Mutation proceeds, snapshot fails, all acquired refs eventually released |
| Collector/HP failure and cancellation at each stage | No partial success, no leaked refs/users/leadership |
| Back-to-back snapshots and delayed registration CAS | No stale descriptor access or epoch ABA |
| Repeated selector cycles with a long-lived node | Old node stays visible; epoch identity remains pinned without counter exhaustion |
| Epoch address reuse after final release | No node, writer, or snapshot still compares the retired identity |
| Epoch allocation failure or before-cut cancellation | Selected epoch unchanged; later snapshot attempts can succeed |
| Writer/user/reference count boundary values | No carry into another field or silent overflow |
| Final reference invokes reentrant cleanup | Leadership released and payload detached before callback |
| Threshold-1 reclamation and thread unregister | No UAF, double release, or retained orphan journal segments |

Retain the existing 1,500,000-item snapshot performance test's under-1,000-ms
requirement for its quiescent workload. Record writer throughput and
P50/P99/P99.9 latency, cut wait, materialization, cancellation latency, traversal
restarts, journal bytes, and retained-node high-water marks with 1/8/32/64 writers.
Hold a snapshot deliberately inside enumeration and require concurrent mutations
to complete before releasing it; total throughput alone is not proof of progress.

Measure the no-snapshot cost separately from snapshot costs. Journal budgets
balance retained memory against snapshot failure rate. Leak checks,
sanitizer/verification configurations, sequence-number tests, and resize chaos
exercise ownership and existing operation semantics as well as snapshot contents.

## Integration boundary

The concurrent enumerator can be added independently of
`clds_sorted_list_get_all`, leaving that API and its callers unchanged.
Switching the hash-table snapshot path requires the epoch domain, journal,
concurrent enumerator, and collector together.
