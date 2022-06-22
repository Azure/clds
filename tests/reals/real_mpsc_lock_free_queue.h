// Copyright (c) Microsoft. All rights reserved.

#ifndef REAL_MPSC_LOCK_FREE_QUEUE_H
#define REAL_MPSC_LOCK_FREE_QUEUE_H

#include "macro_utils/macro_utils.h"
#include "clds/mpsc_lock_free_queue.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_MPSC_LOCK_FREE_QUEUE_GLOBAL_MOCK_HOOKS() \
    MU_FOR_EACH_1(R2, \
        mpsc_lock_free_queue_create, \
        mpsc_lock_free_queue_destroy, \
        mpsc_lock_free_queue_enqueue, \
        mpsc_lock_free_queue_dequeue, \
        mpsc_lock_free_queue_is_empty, \
        mpsc_lock_free_queue_peek \
    )

#include <stddef.h>
#include <stdbool.h>

MPSC_LOCK_FREE_QUEUE_HANDLE real_mpsc_lock_free_queue_create(void);
void real_mpsc_lock_free_queue_destroy(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue);
int real_mpsc_lock_free_queue_enqueue(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue, MPSC_LOCK_FREE_QUEUE_ITEM* item);
MPSC_LOCK_FREE_QUEUE_ITEM* real_mpsc_lock_free_queue_dequeue(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue);
int real_mpsc_lock_free_queue_is_empty(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue, bool* is_empty);
MPSC_LOCK_FREE_QUEUE_ITEM* real_mpsc_lock_free_queue_peek(MPSC_LOCK_FREE_QUEUE_HANDLE mpsc_lock_free_queue);


#endif // REAL_MPSC_LOCK_FREE_QUEUE_H
