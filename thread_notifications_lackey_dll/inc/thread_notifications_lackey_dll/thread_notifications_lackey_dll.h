// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef THREAD_NOTIFICATIONS_LACKEY_DLL_H
#define THREAD_NOTIFICATIONS_LACKEY_DLL_H

#include "macro_utils/macro_utils.h"

#include "umock_c/umock_c_prod.h"

#include "windows.h"

#define THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES \
    THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_ATTACH, \
    THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_DETACH

MU_DEFINE_ENUM(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES)

typedef void (*THREAD_NOTIFICATION_LACKEY_DLL_CALLBACK_FUNC)(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON reason);

MOCKABLE_FUNCTION(, int, thread_notifications_lackey_dll_init_callback, THREAD_NOTIFICATION_LACKEY_DLL_CALLBACK_FUNC, thread_notification_cb);
MOCKABLE_FUNCTION(, void, thread_notifications_lackey_dll_deinit_callback);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

#endif // THREAD_NOTIFICATIONS_LACKEY_DLL_H
