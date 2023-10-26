// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef REAL_THREAD_NOTIFICATIONS_DISPATCHER_H
#define REAL_THREAD_NOTIFICATIONS_DISPATCHER_H

#include "macro_utils/macro_utils.h"

#include "clds/tcall_dispatcher_ll.h"
#include "clds/tcall_dispatcher.h"
#include "thread_notifications_lackey_dll/thread_notifications_lackey_dll.h"

#include "../../inc/clds/thread_notifications_dispatcher.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_THREAD_NOTIFICATIONS_DISPATCHER_GLOBAL_MOCK_HOOK() \
    MU_FOR_EACH_1(R2, \
        thread_notifications_dispatcher_init, \
        thread_notifications_dispatcher_deinit, \
        thread_notifications_dispatcher_get_call_dispatcher, \
    )

#include "umock_c/umock_c_prod.h"

int real_thread_notifications_dispatcher_init(void);
void real_thread_notifications_dispatcher_deinit(void);

TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) real_thread_notifications_dispatcher_get_call_dispatcher(void);

#endif // REAL_THREAD_NOTIFICATIONS_DISPATCHER_H
