// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef REAL_CLDS_HAZARD_POINTERS_THREAD_HELPER_H
#define REAL_CLDS_HAZARD_POINTERS_THREAD_HELPER_H

#include "macro_utils/macro_utils.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hazard_pointers_thread_helper.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_CLDS_HAZARD_POINTERS_THREAD_HELPER_GLOBAL_MOCK_HOOKS() \
    MU_FOR_EACH_1(R2, \
        clds_hazard_pointers_thread_helper_create, \
        clds_hazard_pointers_thread_helper_destroy, \
        clds_hazard_pointers_thread_helper_get_thread \
    )

CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE real_clds_hazard_pointers_thread_helper_create(CLDS_HAZARD_POINTERS_HANDLE hazard_pointers);
void real_clds_hazard_pointers_thread_helper_destroy(CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE hazard_pointers_helper);

CLDS_HAZARD_POINTERS_THREAD_HANDLE real_clds_hazard_pointers_thread_helper_get_thread(CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE hazard_pointers_helper);

#endif // REAL_CLDS_HAZARD_POINTERS_THREAD_HELPER_H
