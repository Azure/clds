// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_HAZARD_POINTERS_H
#define CLDS_HAZARD_POINTERS_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

#include "azure_c_shared_utility/umock_c_prod.h"

typedef struct CLDS_HAZARD_POINTERS_TAG* CLDS_HAZARD_POINTERS_HANDLE;
typedef struct CLDS_HAZARD_POINTERS_THREAD_TAG* CLDS_HAZARD_POINTERS_THREAD_HANDLE;
typedef struct CLDS_HAZARD_POINTER_RECORD_TAG* CLDS_HAZARD_POINTER_RECORD_HANDLE;

typedef void(*RECLAIM_FUNC)(void* node);

MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers_create);
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_destroy, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_register_thread, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_unregister_thread, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
MOCKABLE_FUNCTION(, CLDS_HAZARD_POINTER_RECORD_HANDLE, clds_hazard_pointers_acquire, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, node);
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_release, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, CLDS_HAZARD_POINTER_RECORD_HANDLE, clds_hazard_pointer_record);
MOCKABLE_FUNCTION(, void, clds_hazard_pointers_reclaim, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread, void*, node, RECLAIM_FUNC, reclaim_func);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_HAZARD_POINTERS_H */
