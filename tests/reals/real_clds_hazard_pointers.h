// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef REAL_CLDS_HAZARD_POINTERS_H
#define REAL_CLDS_HAZARD_POINTERS_H

#include "azure_macro_utils/macro_utils.h"
#include "clds/clds_hazard_pointers.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS() \
    MU_FOR_EACH_1(R2, \
        clds_hazard_pointers_create, \
        clds_hazard_pointers_destroy, \
        clds_hazard_pointers_register_thread, \
        clds_hazard_pointers_unregister_thread, \
        clds_hazard_pointers_acquire, \
        clds_hazard_pointers_release, \
        clds_hazard_pointers_reclaim, \
        clds_hazard_pointers_set_reclaim_threshold \
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
int real_clds_hazard_pointers_set_reclaim_threshold(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, size_t reclaim_threshold);

#ifdef __cplusplus
}
#endif

#endif // REAL_CLDS_HAZARD_POINTERS_H
