// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include "macro_utils/macro_utils.h"

#include "windows.h" // Thread Local Storage doesn't have a PAL yet

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/log_critical_and_terminate.h"

#include "c_util/thread_notifications_dispatcher.h"

#include "clds/clds_hazard_pointers.h"

#include "clds/clds_hazard_pointers_thread_helper.h"

typedef struct CLDS_HAZARD_POINTERS_THREAD_HELPER_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers;
    DWORD tls_slot;
    TCALL_DISPATCHER_TARGET_HANDLE(THREAD_NOTIFICATION_CALL) thread_notification_target_handle;
    TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) call_dispatcher;
} CLDS_HAZARD_POINTERS_THREAD_HELPER;

MU_DEFINE_ENUM_STRINGS(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES);

static void clds_hazard_pointers_thread_helper_thread_notification(void* context, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason)
{
    if (context == NULL)
    {
        /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_005: [ If context is NULL, clds_hazard_pointers_thread_helper_thread_notification shall terminate the process. ]*/
        LogCriticalAndTerminate("Unexpected reason: void* context=%p, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason=%" PRI_MU_ENUM "",
            context, MU_ENUM_VALUE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, reason));
    }
    else
    {
        switch (reason)
        {
        default:
            /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_012: [ If reason is any other value, clds_hazard_pointers_thread_helper_thread_notification shall return. ]*/
            LogError("Unexpected reason: void* context=%p, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason=%" PRI_MU_ENUM "",
                context, MU_ENUM_VALUE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, reason));
            break;

        case THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH:
            /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_006: [ If reason is THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH, clds_hazard_pointers_thread_helper_thread_notification shall return. ]*/
            LogInfo("Got notification: %" PRI_MU_ENUM "", MU_ENUM_VALUE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, reason));
            break;

        case THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH:
        {
            CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE hazard_pointers_helper = context;
            LogInfo("Got notification: %" PRI_MU_ENUM "", MU_ENUM_VALUE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, reason));

            /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_007: [ If reason is THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH, clds_hazard_pointers_thread_helper_thread_notification shall call TlsGetValue obtain the thread local value for the slot created in the clds_hazard_pointers_thread_create. ]*/
            CLDS_HAZARD_POINTERS_THREAD_HANDLE hp_thread_handle = TlsGetValue(hazard_pointers_helper->tls_slot);

            /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_009: [ If the thread local stored value is not NULL: ]*/
            if (hp_thread_handle != NULL)
            {
                /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_010: [ clds_hazard_pointers_thread_helper_thread_notification shall set the value in the slot to NULL by calling TlsSetValue. ]*/
                if (!TlsSetValue(hazard_pointers_helper->tls_slot, NULL))
                {
                    LogError("TlsSetValue(hazard_pointers_helper->tls_slot=%" PRIu32 ", NULL) failed", hazard_pointers_helper->tls_slot);
                }
                /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_011: [ clds_hazard_pointers_thread_helper_thread_notification shall call clds_hazard_pointers_unregister_thread with the argument being the obtained thread local value. ]*/
                clds_hazard_pointers_unregister_thread(hp_thread_handle);
            }
            else
            {
                /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_008: [ If the thread local stored value is NULL, clds_hazard_pointers_thread_helper_thread_notification shall return. ]*/
            }
            break;
        }
        }
    }
}

IMPLEMENT_MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, clds_hazard_pointers_thread_helper_create, CLDS_HAZARD_POINTERS_HANDLE, hazard_pointers)
{
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE result;

    if (hazard_pointers == NULL)
    {
        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_001: [ If hazard_pointers is NULL then clds_hazard_pointers_thread_helper_create shall fail and return NULL. ]*/
        LogError("Invalid args: CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = %p", hazard_pointers);
        result = NULL;
    }
    else
    {
        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_002: [ clds_hazard_pointers_thread_helper_create shall allocate memory for the helper. ]*/
        result = malloc(sizeof(CLDS_HAZARD_POINTERS_THREAD_HELPER));

        if (result == NULL)
        {
            /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_005: [ If there are any errors then clds_hazard_pointers_thread_helper_create shall fail and return NULL. ]*/
            LogError("malloc CLDS_HAZARD_POINTERS_THREAD_HELPER failed");
        }
        else
        {
            /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_003: [ clds_hazard_pointers_thread_helper_create shall allocate the thread local storage slot for the hazard pointers by calling TlsAlloc. ]*/
            result->tls_slot = TlsAlloc();
            if (result->tls_slot == TLS_OUT_OF_INDEXES)
            {
                /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_005: [ If there are any errors then clds_hazard_pointers_thread_helper_create shall fail and return NULL. ]*/
                LogError("TlsAlloc failed");
            }
            else
            {
                /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_001: [ clds_hazard_pointers_thread_helper_create shall obtain a TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) by calling thread_notifications_dispatcher_get_call_dispatcher. ]*/
                TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) call_dispatcher = thread_notifications_dispatcher_get_call_dispatcher();
                if (call_dispatcher == NULL)
                {
                    /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_005: [ If there are any errors then clds_hazard_pointers_thread_helper_create shall fail and return NULL. ]*/
                    LogError("thread_notifications_dispatcher_get_call_dispatcher failed");
                }
                else
                {
                    TCALL_DISPATCHER_INITIALIZE_MOVE(THREAD_NOTIFICATION_CALL)(&result->call_dispatcher, &call_dispatcher);

                    /* Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_002: [ clds_hazard_pointers_thread_helper_create shall register clds_hazard_pointers_thread_helper_thread_notification call target with the TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL). ]*/
                    result->thread_notification_target_handle = TCALL_DISPATCHER_REGISTER_TARGET(THREAD_NOTIFICATION_CALL)(result->call_dispatcher, clds_hazard_pointers_thread_helper_thread_notification, result);
                    if (result->thread_notification_target_handle == NULL)
                    {
                        LogError("thread_notifications_register_notification failed");
                    }
                    else
                    {
                        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_004: [ clds_hazard_pointers_thread_helper_create shall succeed and return the helper. ]*/
                        result->hazard_pointers = hazard_pointers;

                        goto all_ok;
                        //TCALL_DISPATCHER_UNREGISTER_TARGET(THREAD_NOTIFICATION_CALL)(result->call_dispatcher, result->thread_notification_target_handle);;
                    }

                    TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(&result->call_dispatcher, NULL);
                }

                (void)TlsFree(result->tls_slot);
            }

            free(result);
            result = NULL;
        }
    }
all_ok:
    return result;
}

IMPLEMENT_MOCKABLE_FUNCTION(, void, clds_hazard_pointers_thread_helper_destroy, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, hazard_pointers_helper)
{
    if (hazard_pointers_helper == NULL)
    {
        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_006: [ If hazard_pointers_helper is NULL then clds_hazard_pointers_thread_helper_destroy shall return. ]*/
        LogError("Invalid args: CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE hazard_pointers_helper = %p", hazard_pointers_helper);
    }
    else
    {
        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_004: [ clds_hazard_pointers_thread_helper_destroy shall unregister the call target by calling TCALL_DISPATCHER_UNREGISTER_TARGET(THREAD_NOTIFICATION_CALL). ]*/
        TCALL_DISPATCHER_UNREGISTER_TARGET(THREAD_NOTIFICATION_CALL)(hazard_pointers_helper->call_dispatcher, hazard_pointers_helper->thread_notification_target_handle);

        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_01_003: [ clds_hazard_pointers_thread_helper_destroy shall release its reference to the TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL). ]*/
        TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(&hazard_pointers_helper->call_dispatcher, NULL);

        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_007: [ clds_hazard_pointers_thread_helper_destroy shall free the thread local storage slot by calling TlsFree. ]*/
        (void)TlsFree(hazard_pointers_helper->tls_slot);

        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_008: [ clds_hazard_pointers_thread_helper_destroy shall free the helper. ]*/
        free(hazard_pointers_helper);
    }
}

IMPLEMENT_MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread_helper_get_thread, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, hazard_pointers_helper)
{
    CLDS_HAZARD_POINTERS_THREAD_HANDLE result;

    if (hazard_pointers_helper == NULL)
    {
        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_009: [ If hazard_pointers_helper is NULL then clds_hazard_pointers_thread_helper_get_thread shall fail and return NULL. ]*/
        LogError("Invalid args: CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE hazard_pointers_helper = %p", hazard_pointers_helper);
        result = NULL;
    }
    else
    {
        /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_010: [ clds_hazard_pointers_thread_helper_get_thread shall get the thread local handle by calling TlsGetValue. ]*/
        result = TlsGetValue(hazard_pointers_helper->tls_slot);
        if (result == NULL)
        {
            /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_011: [ If no thread local handle exists then: ]*/

            /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_012: [ clds_hazard_pointers_thread_helper_get_thread shall create one by calling clds_hazard_pointers_register_thread. ]*/
            result = clds_hazard_pointers_register_thread(hazard_pointers_helper->hazard_pointers);
            if (result == NULL)
            {
                /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_015: [ If there are any errors then clds_hazard_pointers_thread_helper_get_thread shall fail and return NULL. ]*/
                LogError("Cannot create clds hazard pointers thread");
            }
            else
            {
                /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_013: [ clds_hazard_pointers_thread_helper_get_thread shall store the new handle by calling TlsSetValue. ]*/
                if (!TlsSetValue(hazard_pointers_helper->tls_slot, result))
                {
                    /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_015: [ If there are any errors then clds_hazard_pointers_thread_helper_get_thread shall fail and return NULL. ]*/
                    LogError("Cannot set Tls slot value");
                }
                else
                {
                    /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_014: [ clds_hazard_pointers_thread_helper_get_thread shall return the thread local handle. ]*/
                    goto all_ok;
                }

                clds_hazard_pointers_unregister_thread(result);
                result = NULL;
            }
        }
    }
all_ok:
    return result;
}
