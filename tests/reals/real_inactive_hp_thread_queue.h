// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef REAL_INACTIVE_HP_THREAD_QUEUE_H
#define REAL_INACTIVE_HP_THREAD_QUEUE_H

#include "macro_utils/macro_utils.h"

#include "c_util/tqueue_ll.h"
#include "c_util/tqueue.h"

#include "../../inc/clds/inactive_hp_thread_queue.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_INACTIVE_HP_THREAD_QUEUE_GLOBAL_MOCK_HOOK() \
    REGISTER_GLOBAL_MOCK_HOOK(TQUEUE_PUSH(CLDS_HP_INACTIVE_THREAD), TQUEUE_PUSH(real_CLDS_HP_INACTIVE_THREAD)) \
    REGISTER_GLOBAL_MOCK_HOOK(TQUEUE_POP(CLDS_HP_INACTIVE_THREAD), TQUEUE_POP(real_CLDS_HP_INACTIVE_THREAD)) \
    REGISTER_GLOBAL_MOCK_HOOK(TQUEUE_CREATE(CLDS_HP_INACTIVE_THREAD), TQUEUE_CREATE(real_CLDS_HP_INACTIVE_THREAD)) \
    REGISTER_GLOBAL_MOCK_HOOK(TQUEUE_MOVE(CLDS_HP_INACTIVE_THREAD), TQUEUE_MOVE(real_CLDS_HP_INACTIVE_THREAD)) \
    REGISTER_GLOBAL_MOCK_HOOK(TQUEUE_INITIALIZE(CLDS_HP_INACTIVE_THREAD), TQUEUE_INITIALIZE(real_CLDS_HP_INACTIVE_THREAD)) \
    REGISTER_GLOBAL_MOCK_HOOK(TQUEUE_INITIALIZE_MOVE(CLDS_HP_INACTIVE_THREAD), TQUEUE_INITIALIZE_MOVE(real_CLDS_HP_INACTIVE_THREAD)) \
    REGISTER_GLOBAL_MOCK_HOOK(TQUEUE_ASSIGN(CLDS_HP_INACTIVE_THREAD), TQUEUE_ASSIGN(real_CLDS_HP_INACTIVE_THREAD)) \

#include "umock_c/umock_c_prod.h"

TQUEUE_LL_TYPE_DECLARE(real_CLDS_HP_INACTIVE_THREAD, CLDS_HP_INACTIVE_THREAD);

#endif // REAL_INACTIVE_HP_THREAD_QUEUE_H
