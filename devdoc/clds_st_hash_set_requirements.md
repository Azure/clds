# `clds_st_hash_set` requirements

## Overview

`clds_st_hash_set` is module that implements a hash set that is not thread safe (should be used in single threaded environments).

The module provides the following functionality:
- Inserting items in the hash set
- Delete an item from the hash set by its key

## Exposed API

```c
typedef struct CLDS_ST_HASH_SET_TAG* CLDS_ST_HASH_SET_HANDLE;
typedef uint64_t (*CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC)(void* key);
typedef int(*CLDS_ST_HASH_SET_KEY_COMPARE_FUNC)(void* key_1, void* key_2);

#define CLDS_ST_HASH_SET_INSERT_RESULT_VALUES \
    CLDS_ST_HASH_SET_INSERT_OK, \
    CLDS_ST_HASH_SET_INSERT_ERROR, \
    CLDS_ST_HASH_SET_INSERT_KEY_ALREADY_EXISTS

DEFINE_ENUM(CLDS_ST_HASH_SET_INSERT_RESULT, CLDS_ST_HASH_SET_INSERT_RESULT_VALUES);

#define CLDS_ST_HASH_SET_FIND_RESULT_VALUES \
    CLDS_ST_HASH_SET_FIND_OK, \
    CLDS_ST_HASH_SET_FIND_ERROR, \
    CLDS_ST_HASH_SET_FIND_NOT_FOUND

DEFINE_ENUM(CLDS_ST_HASH_SET_FIND_RESULT, CLDS_ST_HASH_SET_FIND_RESULT_VALUES);

MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set_create, CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, compute_hash, CLDS_ST_HASH_SET_KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size);
MOCKABLE_FUNCTION(, void, clds_st_hash_set_destroy, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set);
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_INSERT_RESULT, clds_st_hash_set_insert, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_FIND_RESULT, clds_st_hash_set_find, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
```

### clds_st_hash_set_create

```c
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set_create, CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, compute_hash, CLDS_ST_HASH_SET_KEY_COMPARE_FUNC, key_compare_func, size_t, initial_bucket_size);
```

**SRS_CLDS_ST_HASH_SET_01_001: [** `clds_st_hash_set_create` shall create a new hash set object and on success it shall return a non-NULL handle to the newly created hash set. **]**

If any error happens, `clds_st_hash_set_create` shall fail and return NULL.

If `compute_hash` is NULL, `clds_st_hash_set_create` shall fail and return NULL.

If `key_compare_func` is NULL, `clds_st_hash_set_create` shall fail and return NULL.

If `initial_bucket_size` is 0, `clds_st_hash_set_create` shall fail and return NULL.

### clds_st_hash_set_destroy

```c
MOCKABLE_FUNCTION(, void, clds_st_hash_set_destroy, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set);
```

`clds_st_hash_set_destroy` shall free all resources associated with the hash set instance.

If `clds_st_hash_set` is NULL, `clds_st_hash_set_destroy` shall return.

### clds_st_hash_set_insert

```c
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_INSERT_RESULT, clds_st_hash_set_insert, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
```

`clds_st_hash_set_insert` shall insert a key in the hash set.

On success `clds_st_hash_set_insert` shall return `CLDS_HASH_TABLE_INSERT_OK`.

If `clds_st_hash_set` is NULL, `clds_st_hash_set_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`.

If `key` is NULL, `clds_st_hash_set_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`.

`clds_st_hash_set_insert` shall hash the key by calling the `compute_hash` function passed to `clds_st_hash_set_create`.

`clds_st_hash_set_insert` shall walk through all the items stored in the list at the bucket given by the computed hash.

If any of the items stored in the list at the bucket given by the computed hash matches `key`, `clds_st_hash_set_insert` shall return `CLDS_ST_HASH_SET_INSERT_KEY_ALREADY_EXISTS`.

### clds_st_hash_set_find

```c
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_FIND_RESULT, clds_st_hash_set_find, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
```

`clds_st_hash_set_find` shall inform the user if the given `key` is in the hash set or not.

If `clds_st_hash_set_find` is NULL, `clds_st_hash_set_find` shall return `CLDS_ST_HASH_SET_FIND_ERROR`.

If `key` is NULL, `clds_st_hash_set_find` shall return `CLDS_ST_HASH_SET_FIND_ERROR`.

`clds_st_hash_set_find` shall hash the key by calling the `compute_hash` function passed to `clds_st_hash_set_create`
.
`clds_st_hash_set_find` shall walk through all the items stored in the list at the bucket given by the computed hash.

If `key` exists in the hash set, `clds_st_hash_set_find` shall return `CLDS_ST_HASH_SET_FIND_OK`.

If `key` does not exist in the hash set, `clds_st_hash_set_find` shall return `CLDS_ST_HASH_SET_FIND_NOT_FOUND`.
