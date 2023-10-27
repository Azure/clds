// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include "windows.h"

#include "macro_utils/macro_utils.h"

#include "c_logging/logger.h"

#include "c_pal/interlocked.h"
#include "c_pal/ps_util.h"

#include "thread_notifications_lackey_dll/thread_notifications_lackey_dll.h"

static volatile_atomic THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC notifications_cb = NULL;

MU_DEFINE_ENUM_STRINGS(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES);

int thread_notifications_lackey_dll_init_callback(THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC thread_notifications_cb)
{
    int result;

    /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_001: [ If thread_notification_cb is NULL, thread_notifications_lackey_set_callback shall fail and return a non-zero value. ]*/
    if (thread_notifications_cb == NULL)
    {
        LogError("Invalid arguments: THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC thread_notifications_cb=%p", thread_notifications_cb);
        result = MU_FAILURE;
    }
    else
    {
        /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_002: [ Otherwise, thread_notifications_lackey_dll_init_callback shall initialize the callback maintained by the module with thread_notification_cb, succeed and return 0. ]*/
        THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC existing_callback = (THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC)interlocked_compare_exchange_pointer((void* volatile_atomic*) & notifications_cb, (void*)thread_notifications_cb, NULL);
        if (existing_callback != NULL)
        {
            /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_003: [ If the callback was already initialized, thread_notifications_lackey_dll_init_callback shall fail and return a non-zero value. ] */
            LogError("User error: callback already set to %p, thread_notifications_cb=%p", existing_callback, thread_notifications_cb);
            result = MU_FAILURE;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

void thread_notifications_lackey_dll_deinit_callback(void)
{
    /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_004: [ thread_notifications_lackey_dll_deinit_callback shall set the callback maintained by the module to NULL. ]*/
    (void)interlocked_exchange_pointer((void* volatile_atomic*)&notifications_cb, NULL);
}

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpvReserved)  // reserved
{
    BOOL result = FALSE;

    (void)hinstDLL;
    (void)lpvReserved;

    switch (fdwReason)
    {
    default:
        /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_017: [ If fdwReason is any other value, DllMain shall terminate the process. ]*/
        ps_util_terminate_process();
        break;

    /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_005: [ If fdwReason is DLL_PROCESS_ATTACH: ]*/
    case DLL_PROCESS_ATTACH:
        /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_006: [ DllMain shall call logger_init to initialize logging for the module. ]*/
        if (logger_init() != 0)
        {
            /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_008: [ If logger_init fails, DllMain shall return FALSE. ]*/
            result = FALSE;
        }
        else
        {
            /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_007: [ Otherwise, DllMain shall initialize the callback maintained by the module with NULL and return TRUE. ]*/
            result = TRUE;
        }
        break;

    /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_011: [ If fdwReason is DLL_THREAD_ATTACH: ]*/
    case DLL_THREAD_ATTACH:
    {
        /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_013: [ If a callback was passed by the user through a thread_notifications_lackey_dll_init_callback call, the callback shall be called with THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH. ]*/
        THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC existing_callback = (THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC)interlocked_compare_exchange_pointer((void* volatile_atomic*) & notifications_cb, NULL, NULL);
        if (existing_callback != NULL)
        {
            existing_callback(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH);
        }

        /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_012: [ DllMain shall return TRUE. ]*/
        result = TRUE;
        break;
    }

    /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_014: [ If fdwReason is DLL_THREAD_DETACH: ]*/
    case DLL_THREAD_DETACH:
    {
        /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_016: [ If a callback was passed by the user through a thread_notifications_lackey_dll_init_callback call, the callback shall be called with THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH. ]*/
        THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC existing_callback = (THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC)interlocked_compare_exchange_pointer((void* volatile_atomic*) & notifications_cb, NULL, NULL);
        if (existing_callback != NULL)
        {
            existing_callback(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH);
        }
        /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_015: [ DllMain shall return TRUE. ]*/
        result = TRUE;
        break;
    }

    /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_009: [ If fdwReason is DLL_PROCESS_DETACH: ]*/
    case DLL_PROCESS_DETACH:
        /* Codes_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_010: [DllMain shall call logger_deinit and return TRUE.]*/
        logger_deinit();
        result = TRUE;
        break;
    }

    return result;
}
