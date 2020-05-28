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

MU_DEFINE_ENUM(CLDS_ST_HASH_SET_INSERT_RESULT, CLDS_ST_HASH_SET_INSERT_RESULT_VALUES);

#define CLDS_ST_HASH_SET_FIND_RESULT_VALUES \
    CLDS_ST_HASH_SET_FIND_OK, \
    CLDS_ST_HASH_SET_FIND_ERROR, \
    CLDS_ST_HASH_SET_FIND_NOT_FOUND

MU_DEFINE_ENUM(CLDS_ST_HASH_SET_FIND_RESULT, CLDS_ST_HASH_SET_FIND_RESULT_VALUES);

MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set_create, CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, compute_hash_func, CLDS_ST_HASH_SET_KEY_COMPARE_FUNC, key_compare_func, size_t, bucket_size);
MOCKABLE_FUNCTION(, void, clds_st_hash_set_destroy, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set);
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_INSERT_RESULT, clds_st_hash_set_insert, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_FIND_RESULT, clds_st_hash_set_find, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
```

### clds_st_hash_set_create

```c
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set_create, CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, compute_hash_func, CLDS_ST_HASH_SET_KEY_COMPARE_FUNC, key_compare_func, size_t, bucket_size);
```

**SRS_CLDS_ST_HASH_SET_01_001: [** `clds_st_hash_set_create` shall create a new hash set object and on success it shall return a non-NULL handle to the newly created hash set. **]**

**SRS_CLDS_ST_HASH_SET_01_022: [** `clds_st_hash_set_create` shall allocate memory for the array of buckets used to store the hash set data. **]**

**SRS_CLDS_ST_HASH_SET_01_002: [** If any error happens, `clds_st_hash_set_create` shall fail and return NULL. **]**

**SRS_CLDS_ST_HASH_SET_01_003: [** If `compute_hash_func` is NULL, `clds_st_hash_set_create` shall fail and return NULL. **]**

**SRS_CLDS_ST_HASH_SET_01_004: [** If `key_compare_func` is NULL, `clds_st_hash_set_create` shall fail and return NULL. **]**

**SRS_CLDS_ST_HASH_SET_01_005: [** If `bucket_size` is 0, `clds_st_hash_set_create` shall fail and return NULL. **]**

### clds_st_hash_set_destroy

```c
MOCKABLE_FUNCTION(, void, clds_st_hash_set_destroy, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set);
```

**SRS_CLDS_ST_HASH_SET_01_006: [** `clds_st_hash_set_destroy` shall free all resources associated with the hash set instance. **]**

**SRS_CLDS_ST_HASH_SET_01_007: [** If `clds_st_hash_set` is NULL, `clds_st_hash_set_destroy` shall return. **]**

### clds_st_hash_set_insert

```c
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_INSERT_RESULT, clds_st_hash_set_insert, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
```

**SRS_CLDS_ST_HASH_SET_01_008: [** `clds_st_hash_set_insert` shall insert a key in the hash set. **]**

**SRS_CLDS_ST_HASH_SET_01_009: [** On success `clds_st_hash_set_insert` shall return `CLDS_HASH_TABLE_INSERT_OK`. **]**

**SRS_CLDS_ST_HASH_SET_01_010: [** If `clds_st_hash_set` is NULL, `clds_st_hash_set_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. **]**

**SRS_CLDS_ST_HASH_SET_01_011: [** If `key` is NULL, `clds_st_hash_set_insert` shall fail and return `CLDS_HASH_TABLE_INSERT_ERROR`. **]**

**SRS_CLDS_ST_HASH_SET_01_012: [** `clds_st_hash_set_insert` shall hash the key by calling the `compute_hash` function passed to `clds_st_hash_set_create`. **]**

**S_R_S_CLDS_ST_HASH_SET_01_013: [** `clds_st_hash_set_insert` shall walk through all the items stored in the list at the bucket given by the computed hash. **]**

**S_R_S_CLDS_ST_HASH_SET_01_014: [** If any of the items stored in the list at the bucket given by the computed hash matches `key`, `clds_st_hash_set_insert` shall return `CLDS_ST_HASH_SET_INSERT_KEY_ALREADY_EXISTS`. **]**

### clds_st_hash_set_find

```c
MOCKABLE_FUNCTION(, CLDS_ST_HASH_SET_FIND_RESULT, clds_st_hash_set_find, CLDS_ST_HASH_SET_HANDLE, clds_st_hash_set, void*, key);
```

**S_R_S_CLDS_ST_HASH_SET_01_015: [** `clds_st_hash_set_find` shall inform the user if the given `key` is in the hash set or not. **]**

**S_R_S_CLDS_ST_HASH_SET_01_016: [** If `clds_st_hash_set_find` is NULL, `clds_st_hash_set_find` shall return `CLDS_ST_HASH_SET_FIND_ERROR`. **]**

**S_R_S_CLDS_ST_HASH_SET_01_017: [** If `key` is NULL, `clds_st_hash_set_find` shall return `CLDS_ST_HASH_SET_FIND_ERROR`. **]**

**S_R_S_CLDS_ST_HASH_SET_01_018: [** `clds_st_hash_set_find` shall hash the key by calling the `compute_hash` function passed to `clds_st_hash_set_create` **]**
.
**S_R_S_CLDS_ST_HASH_SET_01_019: [** `clds_st_hash_set_find` shall walk through all the items stored in the list at the bucket given by the computed hash. **]**

**S_R_S_CLDS_ST_HASH_SET_01_020: [** If `key` exists in the hash set, `clds_st_hash_set_find` shall return `CLDS_ST_HASH_SET_FIND_OK`. **]**

**S_R_S_CLDS_ST_HASH_SET_01_021: [** If `key` does not exist in the hash set, `clds_st_hash_set_find` shall return `CLDS_ST_HASH_SET_FIND_NOT_FOUND`. **]**
