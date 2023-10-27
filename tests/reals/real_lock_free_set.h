// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef REAL_LOCK_FREE_SET_H
#define REAL_LOCK_FREE_SET_H

#include "macro_utils/macro_utils.h"
#include "clds/lock_free_set.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_LOCK_FREE_SET_GLOBAL_MOCK_HOOKS() \
    MU_FOR_EACH_1(R2, \
        lock_free_set_create, \
        lock_free_set_destroy, \
        lock_free_set_insert, \
        lock_free_set_remove, \
        lock_free_set_purge_not_thread_safe \
    )

#include <stddef.h>

LOCK_FREE_SET_HANDLE real_lock_free_set_create(void);
void real_lock_free_set_destroy(LOCK_FREE_SET_HANDLE lock_free_set, NODE_CLEANUP_FUNC node_cleanup_callback, void* context);
int real_lock_free_set_insert(LOCK_FREE_SET_HANDLE lock_free_set, LOCK_FREE_SET_ITEM* item);
int real_lock_free_set_remove(LOCK_FREE_SET_HANDLE lock_free_set, LOCK_FREE_SET_ITEM* item);
int real_lock_free_set_purge_not_thread_safe(LOCK_FREE_SET_HANDLE lock_free_set, NODE_CLEANUP_FUNC node_cleanup_callback, void* context);


#endif // REAL_LOCK_FREE_SET_H
