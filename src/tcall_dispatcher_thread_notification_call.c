// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "thread_notifications_lackey_dll/thread_notifications_lackey_dll.h"
#include "clds/tcall_dispatcher_thread_notification_call.h"

THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(THREAD_NOTIFICATION_CALL));
TCALL_DISPATCHER_TYPE_DEFINE(THREAD_NOTIFICATION_CALL, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, thread_notification_reason);
