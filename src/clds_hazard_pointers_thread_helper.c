// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include "macro_utils/macro_utils.h"

#include "windows.h" // Thread Local Storage doesn't have a PAL yet

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "clds/clds_hazard_pointers.h"

#include "clds/clds_hazard_pointers_thread_helper.h"

typedef struct CLDS_HAZARD_POINTERS_THREAD_HELPER_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers;
    DWORD tls_slot;
} CLDS_HAZARD_POINTERS_THREAD_HELPER;

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
                LogError("TlsAlloc failed");
            }
            else
            {
                /*Codes_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_004: [ clds_hazard_pointers_thread_helper_create shall succeed and return the helper. ]*/
                result->hazard_pointers = hazard_pointers;

                goto all_ok;
                //(void)TlsFree(result->tls_slot);
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
