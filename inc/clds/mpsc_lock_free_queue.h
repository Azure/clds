// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef MPSC_LOCK_FREE_QUEUE_H
#define MPSC_LOCK_FREE_QUEUE_H

#ifdef __cplusplus
#else
#include <stdbool.h>
#endif

#include "umock_c/umock_c_prod.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MPSC_LOCK_FREE_QUEUE_TAG* MPSC_LOCK_FREE_QUEUE_HANDLE;

typedef struct MPSC_LOCK_FREE_QUEUE_ITEM_TAG
{
    volatile struct MPSC_LOCK_FREE_QUEUE_ITEM_TAG* next;
} MPSC_LOCK_FREE_QUEUE_ITEM;

MOCKABLE_FUNCTION(, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue_create);
MOCKABLE_FUNCTION(, void, mpsc_lock_free_queue_destroy, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue);
MOCKABLE_FUNCTION(, int, mpsc_lock_free_queue_enqueue, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue, MPSC_LOCK_FREE_QUEUE_ITEM*, item);
MOCKABLE_FUNCTION(, MPSC_LOCK_FREE_QUEUE_ITEM*, mpsc_lock_free_queue_dequeue, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue);
MOCKABLE_FUNCTION(, MPSC_LOCK_FREE_QUEUE_ITEM*, mpsc_lock_free_queue_peek, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue);
MOCKABLE_FUNCTION(, int, mpsc_lock_free_queue_is_empty, MPSC_LOCK_FREE_QUEUE_HANDLE, mpsc_lock_free_queue, bool*, is_empty);

#ifdef __cplusplus
}
#endif

#endif /* MPSC_LOCK_FREE_QUEUE_H */
