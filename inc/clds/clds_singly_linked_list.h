// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef CLDS_SINGLY_LINKED_LIST_H
#define CLDS_SINGLY_LINKED_LIST_H

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

#include "windows.h"
#include "azure_c_shared_utility/umock_c_prod.h"
#include "macro_utils.h"

typedef struct CLDS_SINGLY_LINKED_LIST_TAG* CLDS_SINGLY_LINKED_LIST_HANDLE;
typedef struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG
{
    size_t record_offset;
    volatile LONG ref_count;
    volatile struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG* next;
} CLDS_SINGLY_LINKED_LIST_ITEM;

#define DECLARE_SINGLY_LINKED_LIST_NODE_TYPE(record_type) \
typedef struct C3(SINGLY_LINKED_LIST_NODE_,record_type,_TAG) \
{ \
    CLDS_SINGLY_LINKED_LIST_ITEM item; \
    record_type record; \
} C2(SINGLY_LINKED_LIST_NODE_,record_type); \

#define CLDS_SINGLY_LINKED_LIST_NODE_CREATE(record_type) \
clds_singly_linked_list_node_create(sizeof(C2(SINGLY_LINKED_LIST_NODE_,record_type)), offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), item), offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), record));

#define CLDS_SINGLY_LINKED_LIST_NODE_DESTROY(record_type, ptr) \
(record_type*)clds_singly_linked_list_node_destroy(ptr, offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), record));

#define CLDS_SINGLY_LINKED_LIST_GET_VALUE(record_type, ptr) \
((record_type*)((unsigned char*)ptr + offsetof(C2(SINGLY_LINKED_LIST_NODE_,record_type), record)))

MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list_create);
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_destroy, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list);

MOCKABLE_FUNCTION(, int, clds_singly_linked_list_insert, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_SINGLY_LINKED_LIST_ITEM*, item);
MOCKABLE_FUNCTION(, int, clds_singly_linked_list_delete, CLDS_SINGLY_LINKED_LIST_HANDLE, clds_singly_linked_list, CLDS_SINGLY_LINKED_LIST_ITEM*, item);

MOCKABLE_FUNCTION(, CLDS_SINGLY_LINKED_LIST_ITEM*, clds_singly_linked_list_node_create, size_t, node_size, size_t, item_offset, size_t, record_offset);
MOCKABLE_FUNCTION(, void, clds_singly_linked_list_node_destroy, CLDS_SINGLY_LINKED_LIST_ITEM*, item);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_SINGLY_LINKED_LIST_H */
