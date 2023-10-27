// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#define TlsAlloc mocked_TlsAlloc
#define TlsFree mocked_TlsFree
#define TlsGetValue mocked_TlsGetValue
#define TlsSetValue mocked_TlsSetValue

#include "../../src/clds_hazard_pointers_thread_helper.c"
