// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#define lock_free_set_create real_lock_free_set_create
#define lock_free_set_destroy real_lock_free_set_destroy
#define lock_free_set_insert real_lock_free_set_insert
#define lock_free_set_remove real_lock_free_set_remove
#define lock_free_set_purge_not_thread_safe real_lock_free_set_purge_not_thread_safe
