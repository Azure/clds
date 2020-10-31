// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef LOCK_FREE_SET_H
#define LOCK_FREE_SET_H

#include "umock_c/umock_c_prod.h"
#include "c_pal/interlocked.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LOCK_FREE_SET_TAG* LOCK_FREE_SET_HANDLE;

typedef struct LOCK_FREE_SET_ITEM_TAG
{
    void* volatile_atomic previous;
    void* volatile_atomic next;
} LOCK_FREE_SET_ITEM;

typedef void(*NODE_CLEANUP_FUNC)(void* context, LOCK_FREE_SET_ITEM* item);

MOCKABLE_FUNCTION(, LOCK_FREE_SET_HANDLE, lock_free_set_create);
MOCKABLE_FUNCTION(, void, lock_free_set_destroy, LOCK_FREE_SET_HANDLE, lock_free_set, NODE_CLEANUP_FUNC, node_cleanup_callback, void*, context);
MOCKABLE_FUNCTION(, int, lock_free_set_insert, LOCK_FREE_SET_HANDLE, lock_free_set, LOCK_FREE_SET_ITEM*, item);
MOCKABLE_FUNCTION(, int, lock_free_set_remove, LOCK_FREE_SET_HANDLE, lock_free_set, LOCK_FREE_SET_ITEM*, item);
MOCKABLE_FUNCTION(, int, lock_free_set_purge_not_thread_safe, LOCK_FREE_SET_HANDLE, lock_free_set, NODE_CLEANUP_FUNC, node_cleanup_callback, void*, context);

#ifdef __cplusplus
}
#endif

#endif /* LOCK_FREE_SET_H */
