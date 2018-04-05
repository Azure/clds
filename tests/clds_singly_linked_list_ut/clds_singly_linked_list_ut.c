// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif

#include "testrunnerswitcher.h"

void* real_malloc(size_t size)
{
    return malloc(size);
}

void real_free(void* ptr)
{
    free(ptr);
}

#include "umock_c.h"
#include "umocktypes_stdint.h"

#define ENABLE_MOCKS

#include "azure_c_shared_utility/gballoc.h"
#include "clds/clds_hazard_pointers.h"

#undef ENABLE_MOCKS

#include "clds/clds_singly_linked_list.h"
#include "../reals/real_clds_hazard_pointers.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;
static TEST_MUTEX_HANDLE g_dllByDll;

TEST_DEFINE_ENUM_TYPE(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_RESULT_VALUES);

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

MOCK_FUNCTION_WITH_CODE(, void, test_item_cleanup_func, void*, context, CLDS_SINGLY_LINKED_LIST_ITEM*, item)
MOCK_FUNCTION_END()

MOCK_FUNCTION_WITH_CODE(, uint64_t, test_compute_hash, void*, key)
    (void)key;
MOCK_FUNCTION_END(0)

typedef struct TEST_ITEM_TAG
{
    int dummy;
} TEST_ITEM;

DECLARE_SINGLY_LINKED_LIST_NODE_TYPE(TEST_ITEM)

BEGIN_TEST_SUITE(clds_singly_linked_list_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);

    test_serialize_mutex = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(test_serialize_mutex);

    result = umock_c_init(on_umock_c_error);
    ASSERT_ARE_EQUAL(int, 0, result, "umock_c_init failed");

    result = umocktypes_stdint_register_types();
    ASSERT_ARE_EQUAL(int, 0, result, "umocktypes_stdint_register_types failed");

    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, real_free);

    REGISTER_UMOCK_ALIAS_TYPE(RECLAIM_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTER_RECORD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_THREAD_HANDLE, void*);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    TEST_MUTEX_DESTROY(test_serialize_mutex);
    TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    if (TEST_MUTEX_ACQUIRE(test_serialize_mutex))
    {
        ASSERT_FAIL("Could not acquire test serialization mutex.");
    }

    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    TEST_MUTEX_RELEASE(test_serialize_mutex);
}

/* clds_singly_linked_list_create */

/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_001: [ `clds_singly_linked_list_create` shall create a new singly linked list object and on success it shall return a non-NULL handle to the newly created list. ]*/
TEST_FUNCTION(clds_hash_table_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SINGLY_LINKED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_singly_linked_list_create(hazard_pointers, test_item_cleanup_func, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_singly_linked_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_002: [ If any error happens, `clds_singly_linked_list_create` shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_fails_clds_hash_table_create_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SINGLY_LINKED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG))
        .SetReturn(NULL);

    // act
    list = clds_singly_linked_list_create(hazard_pointers, test_item_cleanup_func, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_003: [ If `clds_hazard_pointers` is NULL, `clds_singly_linked_list_create` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_clds_hazard_pointers_fails)
{
    // arrange
    CLDS_SINGLY_LINKED_LIST_HANDLE list;

    // act
    list = clds_singly_linked_list_create(NULL, test_item_cleanup_func, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);
}

/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_036: [ `item_cleanup_callback` shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_item_cleanup_callback_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SINGLY_LINKED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_singly_linked_list_create(hazard_pointers, NULL, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_singly_linked_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_037: [ `item_cleanup_callback_context` shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_item_cleanup_callback_context_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SINGLY_LINKED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_singly_linked_list_create(hazard_pointers, test_item_cleanup_func, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_singly_linked_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_singly_linked_list_destroy */

/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_004: [ `clds_singly_linked_list_destroy` shall free all resources associated with the hazard pointers instance. ]*/
TEST_FUNCTION(clds_singly_linked_list_destroy_frees_the_allocated_list_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SINGLY_LINKED_LIST_HANDLE list;
    list = clds_singly_linked_list_create(hazard_pointers, test_item_cleanup_func, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_singly_linked_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_005: [ If `clds_singly_linked_list` is NULL, `clds_singly_linked_list_destroy` shall return. ]*/
TEST_FUNCTION(clds_singly_linked_list_destroy_with_NULL_handle_returns)
{
    // arrange

    // act
    clds_singly_linked_list_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_039: [ Any items still present in the list shall be freed. ]*/
/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_040: [ For each item that is freed, the callback `item_cleanup_callback` passed to `clds_singly_linked_list_create` shall be called, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_singly_linked_list_destroy_with_1_item_in_the_list_frees_the_item_and_triggers_user_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SINGLY_LINKED_LIST_HANDLE list;
    list = clds_singly_linked_list_create(hazard_pointers, test_item_cleanup_func, (void*)0x4242);
    CLDS_SINGLY_LINKED_LIST_ITEM* item = CLDS_SINGLY_LINKED_LIST_NODE_CREATE(TEST_ITEM);
    (void)clds_singly_linked_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_singly_linked_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_039: [ Any items still present in the list shall be freed. ]*/
/* Tests_SRS_CLDS_SINGLY_LINKED_LIST_01_040: [ For each item that is freed, the callback `item_cleanup_callback` passed to `clds_singly_linked_list_create` shall be called, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_singly_linked_list_destroy_with_2_items_in_the_list_frees_the_item_and_triggers_user_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SINGLY_LINKED_LIST_HANDLE list;
    list = clds_singly_linked_list_create(hazard_pointers, test_item_cleanup_func, (void*)0x4242);
    CLDS_SINGLY_LINKED_LIST_ITEM* item_1 = CLDS_SINGLY_LINKED_LIST_NODE_CREATE(TEST_ITEM);
    CLDS_SINGLY_LINKED_LIST_ITEM* item_2 = CLDS_SINGLY_LINKED_LIST_NODE_CREATE(TEST_ITEM);
    (void)clds_singly_linked_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_singly_linked_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_1));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_singly_linked_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(clds_singly_linked_list_unittests)
