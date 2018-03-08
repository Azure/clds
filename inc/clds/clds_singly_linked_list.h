// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_SINGLY_LINKED_LIST_H
#define CLDS_SINGLY_LINKED_LIST_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

#include "azure_c_shared_utility/umock_c_prod.h"

typedef struct CLDS_SINGLY_LINKED_LIST_TAG* CLDS_SINGLY_LINKED_LIST_HANDLE;

MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list_create);
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_destroy, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_SINGLY_LINKED_LIST_H */
