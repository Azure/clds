// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef REAL_CLDS_SINGLY_LINKED_LIST_H
#define REAL_CLDS_SINGLY_LINKED_LIST_H

#include "azure_macro_utils/macro_utils.h"
#include "clds/clds_singly_linked_list.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_CLDS_SINGLY_LINKED_LIST_GLOBAL_MOCK_HOOKS() \
    MU_FOR_EACH_1(R2, \
        clds_singly_linked_list_create, \
        clds_singly_linked_list_destroy, \
        clds_singly_linked_list_insert, \
        clds_singly_linked_list_delete, \
        clds_singly_linked_list_delete_if, \
        clds_singly_linked_list_find, \
        clds_singly_linked_list_node_create, \
        clds_singly_linked_list_node_inc_ref, \
        clds_singly_linked_list_node_release \
    )

#ifdef __cplusplus
#include <cstddef>
extern "C"
{
#else
#include <stddef.h>
#endif

CLDS_SINGLY_LINKED_LIST_HANDLE real_clds_singly_linked_list_create(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers);
void real_clds_singly_linked_list_destroy(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list);

int real_clds_singly_linked_list_insert(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM* item);
CLDS_SINGLY_LINKED_LIST_DELETE_RESULT real_clds_singly_linked_list_delete(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SINGLY_LINKED_LIST_ITEM* item);
CLDS_SINGLY_LINKED_LIST_DELETE_RESULT real_clds_singly_linked_list_delete_if(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_callback_context);
CLDS_SINGLY_LINKED_LIST_ITEM* real_clds_singly_linked_list_find(CLDS_SINGLY_LINKED_LIST_HANDLE clds_singly_linked_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SINGLY_LINKED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_callback_context);

CLDS_SINGLY_LINKED_LIST_ITEM* real_clds_singly_linked_list_node_create(size_t node_size, SINGLY_LINKED_LIST_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context);
int real_clds_singly_linked_list_node_inc_ref(CLDS_SINGLY_LINKED_LIST_ITEM* item);
void real_clds_singly_linked_list_node_release(CLDS_SINGLY_LINKED_LIST_ITEM* item);

#ifdef __cplusplus
}
#endif

#endif // REAL_CLDS_SINGLY_LINKED_LIST_H
