// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include "windows.h"

#define TlsAlloc mocked_TlsAlloc
#define TlsFree mocked_TlsFree
#define TlsGetValue mocked_TlsGetValue
#define TlsSetValue mocked_TlsSetValue

extern LPVOID TlsGetValue(
    DWORD dwTlsIndex
);

extern BOOL TlsSetValue(
    DWORD  dwTlsIndex,
    LPVOID lpTlsValue
);

extern DWORD TlsAlloc();

extern BOOL TlsFree(
    DWORD dwTlsIndex
);

#include "../../src/clds_hazard_pointers_thread_helper.c"
