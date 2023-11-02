// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>

#include "macro_utils/macro_utils.h"

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "clds/tcall_dispatcher_thread_notification_call.h"

#include "clds/thread_notifications_dispatcher.h"

#define THREAD_NOTIFICATIONS_DISPATCHER_STATE_VALUES \
    THREAD_NOTIFICATIONS_DISPATCHER_NOT_INITIALIZED, \
    THREAD_NOTIFICATIONS_DISPATCHER_INITIALIZED

MU_DEFINE_ENUM(THREAD_NOTIFICATIONS_DISPATCHER_STATE, THREAD_NOTIFICATIONS_DISPATCHER_STATE_VALUES)
MU_DEFINE_ENUM_STRINGS(THREAD_NOTIFICATIONS_DISPATCHER_STATE, THREAD_NOTIFICATIONS_DISPATCHER_STATE_VALUES);

typedef struct THREAD_NOTIFICATIONS_DISPATCHER_TAG
{
    THREAD_NOTIFICATIONS_DISPATCHER_STATE state;
    TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) thread_notifications_call_dispatcher;
} THREAD_NOTIFICATIONS_DISPATCHER;

static THREAD_NOTIFICATIONS_DISPATCHER thread_notifications_dispatcher =
{
    .state = THREAD_NOTIFICATIONS_DISPATCHER_NOT_INITIALIZED,
    .thread_notifications_call_dispatcher = NULL
};

static void thread_notifications_lackey_dll_cb(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason)
{
    /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_013: [ thread_notifications_lackey_dll_cb shall dispatch the call by calling TCALL_DISPATCHER_DISPATCH_CALL(THREAD_NOTIFICATION_CALL) with reason as argument. ]*/
    TCALL_DISPATCHER_DISPATCH_CALL(THREAD_NOTIFICATION_CALL)(thread_notifications_dispatcher.thread_notifications_call_dispatcher, reason);
}

int thread_notifications_dispatcher_init(void)
{
    int result;

    if (thread_notifications_dispatcher.state != THREAD_NOTIFICATIONS_DISPATCHER_NOT_INITIALIZED)
    {
        /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_001: [ If the module is already initialized, thread_notifications_dispatcher_init shall fail and return a non-zero value. ]*/
        LogError("Invalid state: %" PRI_MU_ENUM "",
            MU_ENUM_VALUE(THREAD_NOTIFICATIONS_DISPATCHER_STATE, thread_notifications_dispatcher.state));
        result = MU_FAILURE;
    }
    else
    {
        /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_003: [ thread_notifications_dispatcher_init shall create a TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL). ]*/
        TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) call_dispatcher = TCALL_DISPATCHER_CREATE(THREAD_NOTIFICATION_CALL)();
        if (call_dispatcher == NULL)
        {
            /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_007: [ If any error occurrs, thread_notifications_dispatcher_init shall fail and return a non-zero value. ]*/
            LogError("TCALL_DISPATCHER_CREATE(THREAD_NOTIFICATION_CALL)() failed");
            result = MU_FAILURE;
        }
        else
        {
            /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_004: [ thread_notifications_dispatcher_init shall store in a global variable the TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL). ]*/
            TCALL_DISPATCHER_INITIALIZE_MOVE(THREAD_NOTIFICATION_CALL)(&thread_notifications_dispatcher.thread_notifications_call_dispatcher, &call_dispatcher);

            /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_005: [ thread_notifications_dispatcher_init shall call thread_notifications_lackey_dll_init_callback to register thread_notifications_lackey_dll_cb as the thread notifications callback. ]*/
            if (thread_notifications_lackey_dll_init_callback(thread_notifications_lackey_dll_cb) != 0)
            {
                /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_007: [ If any error occurrs, thread_notifications_dispatcher_init shall fail and return a non-zero value. ]*/
                LogError("thread_notifications_lackey_dll_init_callback failed");
                result = MU_FAILURE;
            }
            else
            {
                /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_002: [ Otherwise, thread_notifications_dispatcher_init shall initialize the module: ]*/
                thread_notifications_dispatcher.state = THREAD_NOTIFICATIONS_DISPATCHER_INITIALIZED;

                /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_006: [ thread_notifications_dispatcher_init shall succeed and return 0. ]*/
                result = 0;
                goto all_ok;
            }

            TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(&thread_notifications_dispatcher.thread_notifications_call_dispatcher, NULL);
        }
    }

all_ok:
    return result;
}

void thread_notifications_dispatcher_deinit(void)
{
    if (thread_notifications_dispatcher.state != THREAD_NOTIFICATIONS_DISPATCHER_INITIALIZED)
    {
        /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_008: [ If the module is not initialized, thread_notifications_dispatcher_deinit shall return. ]*/
        LogError("Invalid state: %" PRI_MU_ENUM "",
            MU_ENUM_VALUE(THREAD_NOTIFICATIONS_DISPATCHER_STATE, thread_notifications_dispatcher.state));
    }
    else
    {
        /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_009: [ Otherwise, thread_notifications_dispatcher_init shall call thread_notifications_lackey_dll_deinit_callback to clear the thread notifications callback. ]*/
        thread_notifications_lackey_dll_deinit_callback();

        /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_010: [ thread_notifications_dispatcher_deinit shall release the reference to the TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL). ]*/
        TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(&thread_notifications_dispatcher.thread_notifications_call_dispatcher, NULL);

        /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_011: [ thread_notifications_dispatcher_deinit shall de-initialize the module. ]*/
        thread_notifications_dispatcher.state = THREAD_NOTIFICATIONS_DISPATCHER_NOT_INITIALIZED;
    }
}

TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) thread_notifications_dispatcher_get_call_dispatcher(void)
{
    TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) result = NULL;
    /* Codes_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_012: [ TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) shall return the TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) created in thread_notifications_dispatcher_init. ]*/
    TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(&result, thread_notifications_dispatcher.thread_notifications_call_dispatcher);
    return result;
}
