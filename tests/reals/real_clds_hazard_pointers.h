// Copyright (c) Microsoft. All rights reserved.

#ifndef REAL_CLDS_HAZARD_POINTERS_H
#define REAL_CLDS_HAZARD_POINTERS_H

#include "macro_utils.h"
#include "clds/clds_hazard_pointers.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS() \
    FOR_EACH_1(R2, \
        clds_hazard_pointers_create, \
        clds_hazard_pointers_destroy, \
        clds_hazard_pointers_register_thread, \
        clds_hazard_pointers_unregister_thread, \
        clds_hazard_pointers_acquire, \
        clds_hazard_pointers_release, \
        clds_hazard_pointers_reclaim \
    )

#ifdef __cplusplus
#include <cstddef>
extern "C"
{
#else
#include <stddef.h>
#endif

CLDS_HAZARD_POINTERS_HANDLE real_clds_hazard_pointers_create(void);
void real_clds_hazard_pointers_destroy(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers);
CLDS_HAZARD_POINTERS_THREAD_HANDLE real_clds_hazard_pointers_register_thread(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers);
void real_clds_hazard_pointers_unregister_thread(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread);
CLDS_HAZARD_POINTER_RECORD_HANDLE real_clds_hazard_pointers_acquire(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* node);
void real_clds_hazard_pointers_release(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointer_record);
void real_clds_hazard_pointers_reclaim(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* node, RECLAIM_FUNC reclaim_func);

#ifdef __cplusplus
}
#endif

#endif // REAL_CLDS_HAZARD_POINTERS_H
