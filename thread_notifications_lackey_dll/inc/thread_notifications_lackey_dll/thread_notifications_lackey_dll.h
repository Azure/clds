// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef THREAD_NOTIFICATIONS_LACKEY_DLL_H
#define THREAD_NOTIFICATIONS_LACKEY_DLL_H

#include "windows.h"

#include "macro_utils/macro_utils.h"

#include "umock_c/umock_c_prod.h"

#define THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES \
    THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH, \
    THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH

MU_DEFINE_ENUM(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES)

typedef void (*THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC)(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason);

MOCKABLE_FUNCTION_WITH_RETURNS(, int, thread_notifications_lackey_dll_init_callback, THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC, thread_notifications_cb)(0, MU_FAILURE);
MOCKABLE_FUNCTION(, void, thread_notifications_lackey_dll_deinit_callback);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

#endif // THREAD_NOTIFICATIONS_LACKEY_DLL_H
