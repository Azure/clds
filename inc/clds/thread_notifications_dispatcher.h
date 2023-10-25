// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef THREAD_NOTIFICATIONS_DISPATCHER_H
#define THREAD_NOTIFICATIONS_DISPATCHER_H

#include "macro_utils/macro_utils.h"

#include "c_pal/thandle.h"

#include "thread_notifications_lackey_dll/thread_notifications_lackey_dll.h"

#include "clds/tcall_dispatcher.h"

#include "umock_c/umock_c_prod.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

TCALL_DISPATCHER_DEFINE_CALL_TYPE(THREAD_NOTIFICATION_CALL, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, thread_notification_reason);
THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(THREAD_NOTIFICATION_CALL));
TCALL_DISPATCHER_TYPE_DECLARE(THREAD_NOTIFICATION_CALL, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, thread_notification_reason);

MOCKABLE_FUNCTION(, int, thread_notifications_dispatcher_init);
MOCKABLE_FUNCTION(, void, thread_notifications_dispatcher_deinit);

MOCKABLE_FUNCTION(, TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL), thread_notifications_dispatcher_get_call_dispatcher);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // THREAD_NOTIFICATIONS_DISPATCHER_H
