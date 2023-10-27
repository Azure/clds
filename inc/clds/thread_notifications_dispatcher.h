// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef THREAD_NOTIFICATIONS_DISPATCHER_H
#define THREAD_NOTIFICATIONS_DISPATCHER_H

#include "macro_utils/macro_utils.h"

#include "c_pal/thandle.h"

#include "thread_notifications_lackey_dll/thread_notifications_lackey_dll.h"

#include "clds/tcall_dispatcher.h"
#include "clds/tcall_dispatcher_thread_notification_call.h"

#include "umock_c/umock_c_prod.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

MOCKABLE_FUNCTION(, int, thread_notifications_dispatcher_init);
MOCKABLE_FUNCTION(, void, thread_notifications_dispatcher_deinit);

MOCKABLE_FUNCTION(, TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL), thread_notifications_dispatcher_get_call_dispatcher);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // THREAD_NOTIFICATIONS_DISPATCHER_H
