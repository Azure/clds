// Copyright (C) Microsoft Corporation. All rights reserved.

#include "windows.h"

#include "thread_notifications_lackey_dll/thread_notifications_lackey_dll.h"

int thread_notifications_lackey_dll_init_callback(THREAD_NOTIFICATION_LACKEY_DLL_CALLBACK_FUNC thread_notification_cb)
{
    (void)thread_notification_cb;

    return MU_FAILURE;
}

void thread_notifications_lackey_dll_deinit_callback(void)
{
}

BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpvReserved)  // reserved
{
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpvReserved;

    return FALSE;
}
