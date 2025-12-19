// Copyright (C) Microsoft Corporation. All rights reserved.
#ifndef CLDS_HAZARD_POINTERS_THREAD_HELPER_H
#define CLDS_HAZARD_POINTERS_THREAD_HELPER_H

#include "macro_utils/macro_utils.h"

#include "clds/clds_hazard_pointers.h"

#include "umock_c/umock_c_prod.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct CLDS_HAZARD_POINTERS_THREAD_HELPER_TAG* CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE;

MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, clds_hazard_pointers_thread_helper_create, CLDS_HAZARD_POINTERS_HANDLE, hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_thread_helper_destroy, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, hazard_pointers_helper);

MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread_helper_get_thread, CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, hazard_pointers_helper);

#ifdef __cplusplus
}
#endif

#endif // CLDS_HAZARD_POINTERS_THREAD_HELPER_H
