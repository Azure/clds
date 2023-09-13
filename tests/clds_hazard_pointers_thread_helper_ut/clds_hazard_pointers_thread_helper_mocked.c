// Copyright (c) Microsoft. All rights reserved.

#define TlsAlloc mocked_TlsAlloc
#define TlsFree mocked_TlsFree
#define TlsGetValue mocked_TlsGetValue
#define TlsSetValue mocked_TlsSetValue

#include "../../src/clds_hazard_pointers_thread_helper.c"
