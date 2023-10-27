// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#define mpsc_lock_free_queue_create real_mpsc_lock_free_queue_create
#define mpsc_lock_free_queue_destroy real_mpsc_lock_free_queue_destroy
#define mpsc_lock_free_queue_enqueue real_mpsc_lock_free_queue_enqueue
#define mpsc_lock_free_queue_dequeue real_mpsc_lock_free_queue_dequeue
#define mpsc_lock_free_queue_is_empty real_mpsc_lock_free_queue_is_empty
#define mpsc_lock_free_queue_peek real_mpsc_lock_free_queue_peek
