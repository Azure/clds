# clds AI Coding Instructions

## Project Overview
clds (C Lockless Data Structures) is a library implementing high-performance, lock-free data structures for multi-threaded applications. It provides hazard pointer-based memory management for safe concurrent access to shared data structures without traditional locking mechanisms.

## External Dependencies and Standards
All code must follow the comprehensive coding standards defined in `deps/c-build-tools/.github/general_coding_instructions.md`. For detailed patterns and conventions, refer to dependency-specific instructions:

- **Build Infrastructure**: `deps/c-build-tools/.github/copilot-instructions.md`
- **Utilities & Collections**: `deps/c-util/.github/copilot-instructions.md` (THANDLE patterns, async operations, generic programming)
- **Platform Abstraction**: `deps/c-pal/.github/copilot-instructions.md` (memory management, threading primitives)
- **Logging Framework**: `deps/c-logging/.github/copilot-instructions.md`
- **Test Framework**: `deps/ctest/.github/copilot-instructions.md`
- **Mocking Framework**: `deps/umock-c/.github/copilot-instructions.md`

## Core Architecture

### Hazard Pointers Foundation
**Hazard Pointers** (`clds_hazard_pointers.h`) form the memory management foundation for all lockless data structures:
- **Purpose**: Safe memory reclamation in lockless environments where nodes can be accessed concurrently
- **Thread Registration**: Each thread must register with `clds_hazard_pointers_register_thread()` before accessing shared data structures
- **Protection Protocol**: Use `clds_hazard_pointers_acquire()` to protect nodes from deletion, `clds_hazard_pointers_release()` when done
- **Reclamation**: Use `clds_hazard_pointers_reclaim()` to safely delete nodes when no threads hold hazard pointers to them
- **Based on**: Maged M. Michael's paper on hazard pointers (IBM Research)

### Data Structure Categories

#### Core Lockless Structures
- **`clds_singly_linked_list.h`**: Foundation singly-linked list with hazard pointer integration
- **`clds_sorted_list.h`**: Ordered list supporting lockless insertion, deletion, and search operations
- **`clds_hash_table.h`**: Lockless hash table built on top of sorted lists with configurable hash and compare functions

#### Specialized Collections
- **`clds_st_hash_set.h`**: Single-threaded hash set (no hazard pointers required)
- **`lock_free_set.h`**: Lockless set implementation for unique element storage
- **`mpsc_lock_free_queue.h`**: Multiple-producer, single-consumer queue
- **`lru_cache.h`**: Lockless LRU cache implementation

#### Platform-Specific Components
- **`clds_hazard_pointers_thread_helper.h`**: Windows-only thread-local storage helper (conditional compilation with `#if WIN32`)
- **`inactive_hp_thread_queue.h`**: Management of inactive hazard pointer threads

## Development Workflows

### Memory Allocator Integration
clds integrates with configurable memory allocators through the c-pal layer:
- **jemalloc**: Use `cmake .. -DGBALLOC_LL_TYPE=JEMALLOC` (requires vcpkg on Windows)
- **mimalloc**: Use `cmake .. -DGBALLOC_LL_TYPE=MIMALLOC` 
- **Default**: System allocator via `GBALLOC_LL_TYPE=PASSTHROUGH`

### Build Configuration Patterns
```powershell
# Standard build with unit tests
cmake .. -Drun_unittests:bool=ON

# Performance testing build
cmake .. -Drun_perf_tests:bool=ON

# Integration testing with specific allocator
cmake .. -Drun_int_tests:bool=ON -DGBALLOC_LL_TYPE=JEMALLOC

# Traceability checking (Windows only, default ON)
cmake .. -Drun_traceability:bool=ON
```

### Test Organization
- **Unit Tests**: `*_ut/` directories test individual components in isolation using umock-c
- **Integration Tests**: `*_int/` directories test cross-component interactions with real allocators  
- **Performance Tests**: `*_perf/` directories measure throughput and latency characteristics
- **Reals Testing**: `tests/reals/` provides non-mocked implementations for integration scenarios

## Code Conventions

### Lockless Programming Patterns
```c
// Hazard pointer protection pattern
CLDS_HAZARD_POINTER_RECORD_HANDLE hp_record = clds_hazard_pointers_acquire(hp_thread, node);
if (hp_record != NULL)
{
    // Node is protected from deletion
    // Perform operations on node
    clds_hazard_pointers_release(hp_thread, hp_record);
}

// Safe reclamation pattern
clds_hazard_pointers_reclaim(hp_thread, node_to_delete, reclaim_function);
```

### Data Structure Declaration Macros
clds uses macro-based generic programming for type-safe data structures:
```c
// Hash table node declaration
DECLARE_HASH_TABLE_NODE_TYPE(my_record_type);

// Sorted list node declaration  
DECLARE_SORTED_LIST_NODE_TYPE(my_item_type);

// Custom compare and hash functions
static int my_key_compare(void* key1, void* key2);
static uint64_t my_hash_function(void* key);
```

### Thread Safety Requirements
- **Hazard Pointer Registration**: Every thread accessing shared data structures must register with hazard pointers first
- **Cleanup Order**: Unregister threads before destroying hazard pointer instances
- **Reclamation Threshold**: Configure with `clds_hazard_pointers_set_reclaim_threshold()` to control when reclamation occurs

## Integration Points

### Memory Management Integration
- All allocations go through `gballoc_hl.h` for consistent memory management
- Hazard pointer reclamation integrates with configurable allocators (jemalloc, mimalloc)

### Callback and Context Patterns
```c
// Item cleanup callbacks for automatic resource management
typedef void(*HASH_TABLE_ITEM_CLEANUP_CB)(void* context, struct CLDS_HASH_TABLE_ITEM_TAG* item);

// Sequence number tracking for ordered operations
typedef void(*HASH_TABLE_SKIPPED_SEQ_NO_CB)(void* context, int64_t skipped_sequence_no);
```

### Cross-Platform Considerations
- Core lockless algorithms work on both Windows and Linux
- Thread-local storage helpers (`clds_hazard_pointers_thread_helper.h`) are Windows-specific
- Use conditional compilation `#if WIN32` for platform-specific features
- Link with `thread_notifications_lackey_dll` on Windows for

## Common Usage Patterns

### Hash Table Operations
```c
// Create hash table with custom functions
CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(compute_hash, key_compare, 64, hp, &seq_no, cleanup_cb, context);

// Thread-safe insertion
CLDS_HASH_TABLE_INSERT_RESULT result = clds_hash_table_insert(hash_table, hp_thread, key, item, &seq_no);

// Find with hazard pointer protection
CLDS_HASH_TABLE_ITEM* found_item = clds_hash_table_find(hash_table, hp_thread, key);
```

### Sorted List Operations
```c
// Create sorted list
CLDS_SORTED_LIST_HANDLE sorted_list = clds_sorted_list_create(hp, get_item_key, compare_keys, cleanup_cb, context);

// Insert item maintaining order
CLDS_SORTED_LIST_INSERT_RESULT result = clds_sorted_list_insert(sorted_list, hp_thread, item, &seq_no);

// Remove by key
CLDS_SORTED_LIST_REMOVE_RESULT result = clds_sorted_list_remove_key(sorted_list, hp_thread, key, &seq_no);
```

## Performance Considerations

### Hazard Pointer Tuning
- Configure reclamation threshold based on memory pressure: higher thresholds reduce reclamation overhead but increase memory usage
- Consider thread count when sizing hazard pointer pools
- Monitor performance tests in `*_perf/` directories for optimization guidance

### Data Structure Selection
- **Hash Table**: Best for key-value lookups with known hash function
- **Sorted List**: Optimal for ordered iteration and range queries
- **Lock-Free Set**: Efficient for unique element testing
- **MPSC Queue**: Producer-heavy scenarios with single consumer
- **LRU Cache**: Bounded memory with access-pattern-based eviction

### Memory Allocator Impact
- jemalloc typically provides best performance for concurrent workloads
- mimalloc offers good performance with lower memory overhead
- Performance tests demonstrate allocator impact on specific workloads

## Requirements Traceability
Each data structure has comprehensive requirements documentation in `devdoc/`:
- Requirements use SRS pattern: `SRS_CLDS_HASH_TABLE_01_001`
- Implementation tagged with `/*Codes_SRS_*` comments
- Unit tests tagged with `/*Tests_SRS_*` comments  
- Traceability tool validates complete coverage (Windows builds only)
