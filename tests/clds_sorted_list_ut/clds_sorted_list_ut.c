// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstdint>
#else
#include <stdlib.h>
#include <stdint.h>
#endif

#include "azure_macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

void* real_malloc(size_t size)
{
    return malloc(size);
}

void real_free(void* ptr)
{
    free(ptr);
}

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_stdint.h"

#define ENABLE_MOCKS

#include "azure_c_util/gballoc.h"
#include "clds/clds_st_hash_set.h"
#include "clds/clds_hazard_pointers.h"

#undef ENABLE_MOCKS

#include "clds/clds_sorted_list.h"
#include "../reals/real_clds_st_hash_set.h"
#include "../reals/real_clds_hazard_pointers.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;

TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_RESULT_VALUES);

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

MOCK_FUNCTION_WITH_CODE(, void, test_item_cleanup_func, void*, context, CLDS_SORTED_LIST_ITEM*, item)
MOCK_FUNCTION_END()
MOCK_FUNCTION_WITH_CODE(, void, test_skipped_seq_no_cb, void*, context, int64_t, skipped_seq_no)
MOCK_FUNCTION_END()

typedef struct TEST_ITEM_TAG
{
    uint32_t key;
} TEST_ITEM;

DECLARE_SORTED_LIST_NODE_TYPE(TEST_ITEM)

static void* test_get_item_key(void* context, struct CLDS_SORTED_LIST_ITEM_TAG* item)
{
    TEST_ITEM* test_item = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    (void)context;
    return (void*)(uintptr_t)test_item->key;
}

static int test_key_compare(void* context, void* key1, void* key2)
{
    int result;

    (void)context;
    if ((int64_t)key1 < (int64_t)key2)
    {
        result = -1;
    }
    else if ((int64_t)key1 > (int64_t)key2)
    {
        result = 1;
    }
    else
    {
        result = 0;
    }

    return result;
}

BEGIN_TEST_SUITE(clds_sorted_list_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    test_serialize_mutex = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(test_serialize_mutex);

    result = umock_c_init(on_umock_c_error);
    ASSERT_ARE_EQUAL(int, 0, result, "umock_c_init failed");

    result = umocktypes_stdint_register_types();
    ASSERT_ARE_EQUAL(int, 0, result, "umocktypes_stdint_register_types failed");

    REGISTER_CLDS_ST_HASH_SET_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, real_free);

    REGISTER_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT);
    REGISTER_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT);
    REGISTER_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT);
    REGISTER_TYPE(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_RESULT);
    REGISTER_TYPE(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_RESULT);

    REGISTER_UMOCK_ALIAS_TYPE(RECLAIM_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTER_RECORD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_THREAD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_KEY_COMPARE_FUNC, void*);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    TEST_MUTEX_DESTROY(test_serialize_mutex);
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

/* clds_sorted_list_create */

/* Tests_SRS_CLDS_SORTED_LIST_01_001: [ clds_sorted_list_create shall create a new sorted list object and on success it shall return a non-NULL handle to the newly created list. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_059: [ start_sequence_number shall be allowed to be NULL, in which case no order shall be provided for the operations. ]*/
TEST_FUNCTION(clds_sorted_list_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = 45;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_001: [ clds_sorted_list_create shall create a new sorted list object and on success it shall return a non-NULL handle to the newly created list. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_059: [ start_sequence_number shall be allowed to be NULL, in which case no order shall be provided for the operations. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_076: [ skipped_seq_no_cb shall be allowed to be NULL. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_077: [ skipped_seq_no_cb_context shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_sorted_list_create_succeeds_with_NULL_skipped_seq_no_cb_and_context)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = 44;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_001: [ clds_sorted_list_create shall create a new sorted list object and on success it shall return a non-NULL handle to the newly created list. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_059: [ start_sequence_number shall be allowed to be NULL, in which case no order shall be provided for the operations. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_076: [ skipped_seq_no_cb shall be allowed to be NULL. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_077: [ skipped_seq_no_cb_context shall be allowed to be NULL. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_078: [ If start_sequence_number is NULL, then skipped_seq_no_cb must also be NULL, otherwise clds_sorted_list_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_create_succeeds_with_NULL_start_sequence_no)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_078: [ If start_sequence_number is NULL, then skipped_seq_no_cb must also be NULL, otherwise clds_sorted_list_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_create_with_NULL_sequence_no_and_non_NULL_skipped_seq_no_cb_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, test_skipped_seq_no_cb, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_002: [ If any error happens, clds_sorted_list_create shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_fails_clds_sorted_list_create_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG))
        .SetReturn(NULL);

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_003: [ If clds_hazard_pointers is NULL, clds_sorted_list_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_create_with_NULL_clds_hazard_pointers_fails)
{
    // arrange
    CLDS_SORTED_LIST_HANDLE list;

    // act
    list = clds_sorted_list_create(NULL, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_045: [ If get_item_key_cb is NULL, clds_sorted_list_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_create_with_NULL_get_item_key_cb_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    // act
    list = clds_sorted_list_create(hazard_pointers, NULL, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_049: [ get_item_key_cb_context shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_sorted_list_create_with_NULL_get_item_key_cb_context_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, NULL, test_key_compare, (void*)0x4243, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_046: [ If key_compare_cb is NULL, clds_sorted_list_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_create_with_NULL_key_compare_cb_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, NULL, (void*)0x4243, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_050: [ key_compare_cb_context shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_sorted_list_create_with_NULL_key_compare_cb_context_context_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, NULL, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_058: [ start_sequence_number shall be used by the sorted list to compute the sequence number of each operation. ]*/
TEST_FUNCTION(clds_sorted_list_create_with_non_NULL_sequence_number_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_destroy */

/* Tests_SRS_CLDS_SORTED_LIST_01_004: [ clds_sorted_list_destroy shall free all resources associated with the sorted list instance. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_frees_the_allocated_list_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_sorted_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_004: [ clds_sorted_list_destroy shall free all resources associated with the sorted list instance. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_frees_the_allocated_list_resources_for_a_list_with_sequence_no)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_sorted_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_005: [ If clds_sorted_list is NULL, clds_sorted_list_destroy shall return. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_with_NULL_handle_returns)
{
    // arrange

    // act
    clds_sorted_list_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_CLDS_SORTED_LIST_01_039: [ Any items still present in the list shall be freed. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_040: [ For each item that is freed, the callback item_cleanup_callback passed to clds_sorted_list_node_create shall be called, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_with_1_item_in_the_list_frees_the_item_and_triggers_user_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_sorted_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_039: [ Any items still present in the list shall be freed. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_040: [ For each item that is freed, the callback item_cleanup_callback passed to clds_sorted_list_node_create shall be called, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_with_2_items_in_the_list_frees_the_item_and_triggers_user_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_1));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_sorted_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_041: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the freed items. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_with_NULL_item_cleanup_callback_does_not_trigger_any_callback_for_the_freed_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_sorted_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_insert */

/* Tests_SRS_CLDS_SORTED_LIST_01_010: [ On success clds_sorted_list_insert shall return CLDS_SORTED_LIST_INSERT_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_069: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
TEST_FUNCTION(clds_sorted_list_insert_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_011: [ If clds_sorted_list is NULL, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_NULL_singly_linked_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_INSERT_RESULT result;
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_insert(NULL, hazard_pointers_thread, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_ERROR, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_012: [ If item is NULL, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_NULL_item_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_insert(list, hazard_pointers_thread, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_013: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_NULL_hazard_pointers_thread_handle_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_insert(list, NULL, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_010: [ On success clds_sorted_list_insert shall return CLDS_SORTED_LIST_INSERT_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_047: [ clds_sorted_list_insert shall insert the item at its correct location making sure that items in the list are sorted according to the order given by item keys. ]*/
TEST_FUNCTION(clds_sorted_list_insert_2_items_in_order_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result_1;
    CLDS_SORTED_LIST_INSERT_RESULT result_2;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result_1 = clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    result_2 = clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_010: [ On success clds_sorted_list_insert shall return CLDS_SORTED_LIST_INSERT_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_047: [ clds_sorted_list_insert shall insert the item at its correct location making sure that items in the list are sorted according to the order given by item keys. ]*/
TEST_FUNCTION(clds_sorted_list_insert_2_items_in_reverse_order_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result_1;
    CLDS_SORTED_LIST_INSERT_RESULT result_2;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x43;
    item_2_payload->key = 0x42;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result_1 = clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    result_2 = clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_048: [ If the item with the given key already exists in the list, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS. ]*/
TEST_FUNCTION(clds_sorted_list_insert_for_a_key_that_already_exists_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_2);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_079: [** If sequence numbers are generated and a skipped sequence number callback was provided to clds_sorted_list_create, when the item is indicated as already existing, the generated sequence number shall be indicated as skipped. ]*/
TEST_FUNCTION(clds_sorted_list_insert_for_a_key_that_already_exists_indicates_the_seq_no_as_skipped)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    volatile int64_t sequence_number = 42;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_skipped_seq_no_cb((void*)0x4243, 44)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_2);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_060: [ For each insert the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
TEST_FUNCTION(clds_sorted_list_insert_provides_sequence_numbers)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result_1;
    CLDS_SORTED_LIST_INSERT_RESULT result_2;
    volatile int64_t sequence_number = 0x42;
    int64_t insert_seq_no_1 = 0;
    int64_t insert_seq_no_2 = 0;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result_1 = clds_sorted_list_insert(list, hazard_pointers_thread, item_1, &insert_seq_no_1);
    result_2 = clds_sorted_list_insert(list, hazard_pointers_thread, item_2, &insert_seq_no_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_2);
    ASSERT_ARE_EQUAL(int64_t, 0x43, insert_seq_no_1);
    ASSERT_ARE_EQUAL(int64_t, 0x44, insert_seq_no_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_061: [ If the sequence_number argument passed to clds_sorted_list_insert is NULL, the computed sequence number for the insert shall still be computed but it shall not be provided to the user. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_NULL_sequence_number_still_computes_sequence_number)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result_1;
    CLDS_SORTED_LIST_INSERT_RESULT result_2;
    volatile int64_t sequence_number = 0x42;
    int64_t insert_seq_no_2 = 0;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result_1 = clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    result_2 = clds_sorted_list_insert(list, hazard_pointers_thread, item_2, &insert_seq_no_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_2);
    ASSERT_ARE_EQUAL(int64_t, 0x44, insert_seq_no_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_062: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_insert shall fail and return CLDS_SORTED_LIST_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_non_NULL_sequence_number_but_no_start_sequence_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    int64_t insert_seq_no = 0;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_1_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_insert(list, hazard_pointers_thread, item_1, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_ERROR, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_1);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_delete_item */

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ clds_sorted_list_delete_item shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_070: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
TEST_FUNCTION(clds_sorted_list_delete_item_deletes_the_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item));
    STRICT_EXPECTED_CALL(free(item));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ clds_sorted_list_delete_item shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_2nd_item_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ clds_sorted_list_delete_item shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_oldest_out_of_2_items_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_1));
    STRICT_EXPECTED_CALL(free(item_1));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ clds_sorted_list_delete_item shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_2nd_out_of_3_items_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ clds_sorted_list_delete_item shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_deletes_the_item_NULL_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ clds_sorted_list_delete_item shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_2nd_item_succeeds_NULL_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ clds_sorted_list_delete_item shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_oldest_out_of_2_items_succeeds_NULL_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_1));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ clds_sorted_list_delete_item shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, clds_sorted_list_delete_item shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling clds_hazard_pointers_reclaim. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_2nd_out_of_3_items_succeeds_NULL_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_015: [ If clds_sorted_list is NULL, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_delete_with_NULL_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(NULL, hazard_pointers_thread, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_016: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_delete_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(list, NULL, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_017: [ If item is NULL, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_delete_with_NULL_item_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_018: [ If the item does not exist in the list, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_sorted_list_delete_on_an_empty_lists_returns_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_018: [ If the item does not exist in the list, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_sorted_list_delete_when_the_item_is_not_in_the_list_returns_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_2);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_018: [ If the item does not exist in the list, clds_sorted_list_delete_item shall fail and return CLDS_SORTED_LIST_DELETE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_sorted_list_delete_twice_on_the_same_item_returns_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)CLDS_SORTED_LIST_NODE_INC_REF(TEST_ITEM, item);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_063: [ For each delete the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
TEST_FUNCTION(clds_sorted_list_delete_stamps_the_order)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 42;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result_1;
    CLDS_SORTED_LIST_DELETE_RESULT result_2;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    int64_t delete_seq_no_1 = 0;
    int64_t delete_seq_no_2 = 0;
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_1));

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result_1 = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_1, &delete_seq_no_1);
    result_2 = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2, &delete_seq_no_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result_2);
    ASSERT_ARE_EQUAL(int64_t, 45, delete_seq_no_1);
    ASSERT_ARE_EQUAL(int64_t, 46, delete_seq_no_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_064: [ If the sequence_number argument passed to clds_sorted_list_delete is NULL, the computed sequence number for the delete shall still be computed but it shall not be provided to the user. ]*/
TEST_FUNCTION(clds_sorted_list_delete_with_no_start_seq_no_stamps_the_order)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 42;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result_1;
    CLDS_SORTED_LIST_DELETE_RESULT result_2;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    int64_t delete_seq_no_2 = 0;
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_1));

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result_1 = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_1, NULL);
    result_2 = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2, &delete_seq_no_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result_2);
    ASSERT_ARE_EQUAL(int64_t, 46, delete_seq_no_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_065: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_delete shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_delete_with_non_NULL_sequence_no_and_no_start_sequence_no_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    int64_t delete_seq_no = 0;
    item_1_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_1, &delete_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_delete_key */

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ clds_sorted_list_delete_key shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, clds_sorted_list_delete_key shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_071: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item));
    STRICT_EXPECTED_CALL(free(item));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ clds_sorted_list_delete_key shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, clds_sorted_list_delete_key shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If item_cleanup_callback is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_succeeds_with_NULL_item_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_020: [ If clds_sorted_list is NULL, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_with_NULL_clds_sorted_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_key(NULL, hazard_pointers_thread, (void*)0x42, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_021: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_with_NULL_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_key(list, NULL, (void*)0x42, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_022: [ If key is NULL, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ clds_sorted_list_delete_key shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, clds_sorted_list_delete_key shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_deletes_the_second_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x43, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ clds_sorted_list_delete_key shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, clds_sorted_list_delete_key shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_deletes_the_oldest_out_of_2_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_1));
    STRICT_EXPECTED_CALL(free(item_1));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ clds_sorted_list_delete_key shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, clds_sorted_list_delete_key shall return CLDS_SORTED_LIST_DELETE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_deletes_the_2nd_out_of_3_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x43, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_024: [ If the key is not found, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_on_an_empty_list_yields_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_024: [ If the key is not found, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_after_the_item_was_deleted_yields_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x43, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_066: [ For each delete key the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_stamps_the_sequence_no)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 42;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4243);
    CLDS_SORTED_LIST_DELETE_RESULT result_1;
    CLDS_SORTED_LIST_DELETE_RESULT result_2;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    int64_t delete_key_seq_no_1 = 0;
    int64_t delete_key_seq_no_2 = 0;
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_1));
    STRICT_EXPECTED_CALL(free(item_1));

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4243, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result_1 = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42, &delete_key_seq_no_1);
    result_2 = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x43, &delete_key_seq_no_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result_2);
    ASSERT_ARE_EQUAL(int64_t, 45, delete_key_seq_no_1);
    ASSERT_ARE_EQUAL(int64_t, 46, delete_key_seq_no_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_067: [ If the sequence_number argument passed to clds_sorted_list_delete_key is NULL, the computed sequence number for the delete shall still be computed but it shall not be provided to the user. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_with_NULL_sequence_no_and_non_NULL_start_Sequence_no_still_computes_the_sequence_numbers)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 42;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4243);
    CLDS_SORTED_LIST_DELETE_RESULT result_1;
    CLDS_SORTED_LIST_DELETE_RESULT result_2;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    int64_t delete_key_seq_no_2 = 0;
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_1));
    STRICT_EXPECTED_CALL(free(item_1));

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4243, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result_1 = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42, NULL);
    result_2 = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x43, &delete_key_seq_no_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result_2);
    ASSERT_ARE_EQUAL(int64_t, 46, delete_key_seq_no_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_068: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_delete_key shall fail and return CLDS_SORTED_LIST_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_with_non_NULL_sequence_no_and_no_start_sequence_no_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    int64_t delete_key_seq_no = 0;
    item_1_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42, &delete_key_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_remove_key */

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ clds_sorted_list_remove_key shall delete an item by its key and return the pointer to the deleted item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, clds_sorted_list_remove_key shall return CLDS_SORTED_LIST_REMOVE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the item argument. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_073: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ clds_sorted_list_remove_key shall delete an item by its key and return the pointer to the deleted item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, clds_sorted_list_remove_key shall return CLDS_SORTED_LIST_REMOVE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the item argument. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_succeeds_with_NULL_item_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_053: [ If clds_sorted_list is NULL, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_with_NULL_clds_sorted_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_remove_key(NULL, hazard_pointers_thread, (void*)0x42, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_055: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_with_NULL_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_remove_key(list, NULL, (void*)0x42, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_056: [ If key is NULL, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, NULL, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ clds_sorted_list_remove_key shall delete an item by its key and return the pointer to the deleted item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, clds_sorted_list_remove_key shall return CLDS_SORTED_LIST_REMOVE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the item argument. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_deletes_the_second_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x43, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ clds_sorted_list_remove_key shall delete an item by its key and return the pointer to the deleted item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, clds_sorted_list_remove_key shall return CLDS_SORTED_LIST_REMOVE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the item argument. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_deletes_the_oldest_out_of_2_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ clds_sorted_list_remove_key shall delete an item by its key and return the pointer to the deleted item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, clds_sorted_list_remove_key shall return CLDS_SORTED_LIST_REMOVE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the item argument. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to clds_hazard_pointers_reclaim shall call the user callback item_cleanup_callback that was passed to clds_sorted_list_node_create, while passing item_cleanup_callback_context and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_deletes_the_2nd_out_of_3_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x43, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_057: [ If the key is not found, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_on_an_empty_list_yields_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    CLDS_SORTED_LIST_ITEM* removed_item;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_NOT_FOUND, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_057: [ If the key is not found, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_after_the_item_was_deleted_yields_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x43, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_NOT_FOUND, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_072: [ For each remove key the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_stamps_the_sequence_numbers)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 42;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item_1;
    CLDS_SORTED_LIST_ITEM* removed_item_2;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    CLDS_SORTED_LIST_REMOVE_RESULT result_1;
    CLDS_SORTED_LIST_REMOVE_RESULT result_2;
    int64_t remove_seq_no_1 = 0;
    int64_t remove_seq_no_2 = 0;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));

    // act
    result_1 = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item_1, &remove_seq_no_1);
    result_2 = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x43, &removed_item_2, &remove_seq_no_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result_2);
    ASSERT_ARE_EQUAL(int64_t, 45, remove_seq_no_1);
    ASSERT_ARE_EQUAL(int64_t, 46, remove_seq_no_2);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item_1);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item_2);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_074: [ If the sequence_number argument passed to clds_sorted_list_remove_key is NULL, the computed sequence number for the remove shall still be computed but it shall not be provided to the user. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_with_NULL_sequence_number_and_non_NULL_start_sequence_number_still_computes_the_sequence_numbers)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 42;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item_1;
    CLDS_SORTED_LIST_ITEM* removed_item_2;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    CLDS_SORTED_LIST_REMOVE_RESULT result_1;
    CLDS_SORTED_LIST_REMOVE_RESULT result_2;
    int64_t remove_seq_no_2 = 0;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));

    // act
    result_1 = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item_1, NULL);
    result_2 = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x43, &removed_item_2, &remove_seq_no_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result_2);
    ASSERT_ARE_EQUAL(int64_t, 46, remove_seq_no_2);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item_1);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item_2);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_075: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_remove_key shall fail and return CLDS_SORTED_LIST_REMOVE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_with_non_NULL_sequence_number_and_NULL_start_sequence_number_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_1_payload->key = 0x42;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    int64_t remove_seq_no = 0;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item, &remove_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_find_key */

/* Tests_SRS_CLDS_SORTED_LIST_01_027: [ clds_sorted_list_find_key shall find in the list the first item that matches the criteria given by a user compare function. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_029: [ On success clds_sorted_list_find_key shall return a non-NULL pointer to the found linked list item. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_succeeds_in_finding_an_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, item, result);

    // cleanup
    clds_sorted_list_destroy(list);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_027: [ clds_sorted_list_find_key shall find in the list the first item that matches the criteria given by a user compare function. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_029: [ On success clds_sorted_list_find_key shall return a non-NULL pointer to the found linked list item. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_finds_the_2nd_out_of_3_added_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x43);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, item_2, result);

    // cleanup
    clds_sorted_list_destroy(list);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_027: [ clds_sorted_list_find_key shall find in the list the first item that matches the criteria given by a user compare function. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_029: [ On success clds_sorted_list_find_key shall return a non-NULL pointer to the found linked list item. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_finds_the_last_added_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x44);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, item_3, result);

    // cleanup
    clds_sorted_list_destroy(list);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_028: [ If clds_sorted_list is NULL, clds_sorted_list_find_key shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_with_NULL_clds_sorted_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_find_key(NULL, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_030: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_find_key shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_with_NULL_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_find_key(list, NULL, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_031: [ If key is NULL, clds_sorted_list_find_key shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, clds_sorted_list_find_key shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_on_an_empty_list_returns_NULL)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, clds_sorted_list_find_key shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_after_the_item_was_deleted_returns_NULL)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, clds_sorted_list_find_key shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_when_the_item_is_not_in_the_list_returns_NULL)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_1);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_034: [ clds_sorted_list_find_key shall return a pointer to the item with the reference count already incremented so that it can be safely used by the caller. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_result_has_the_ref_count_incremented)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));

    // no item cleanup and free should happen here

    // act
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_sorted_list_destroy(list);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_lock_writes */

/* Tests_SRS_CLDS_SORTED_LIST_42_030: [ If clds_sorted_list is NULL then clds_sorted_list_lock_writes shall return. ]*/
TEST_FUNCTION(clds_sorted_list_lock_writes_with_NULL_clds_sorted_list_returns)
{
    // act
    clds_sorted_list_lock_writes(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* clds_sorted_list_unlock_writes */

/* Tests_SRS_CLDS_SORTED_LIST_42_033: [ If clds_sorted_list is NULL then clds_sorted_list_unlock_writes shall return. ]*/
TEST_FUNCTION(clds_sorted_list_unlock_writes_with_NULL_clds_sorted_list_returns)
{
    // act
    clds_sorted_list_unlock_writes(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_CLDS_SORTED_LIST_42_031: [ clds_sorted_list_lock_writes shall increment a counter to lock the list for writes. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_42_034: [ clds_sorted_list_lock_writes shall decrement a counter to unlock the list for writes. ]*/
TEST_FUNCTION(clds_sorted_list_lock_then_unlock_works)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);

    umock_c_reset_all_calls();

    // act
    clds_sorted_list_lock_writes(list);
    clds_sorted_list_unlock_writes(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_42_031: [ clds_sorted_list_lock_writes shall increment a counter to lock the list for writes. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_42_034: [ clds_sorted_list_lock_writes shall decrement a counter to unlock the list for writes. ]*/
TEST_FUNCTION(clds_sorted_list_lock_nests)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);

    umock_c_reset_all_calls();

    // act
    clds_sorted_list_lock_writes(list); // 1
    clds_sorted_list_lock_writes(list); // 2
    clds_sorted_list_lock_writes(list); // 3

    clds_sorted_list_unlock_writes(list); // 2

    clds_sorted_list_lock_writes(list); // 3
    clds_sorted_list_unlock_writes(list); // 2

    clds_sorted_list_unlock_writes(list); // 1
    clds_sorted_list_unlock_writes(list); // 0

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // Insert works after the unlock
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_42_031: [ clds_sorted_list_lock_writes shall increment a counter to lock the list for writes. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_42_034: [ clds_sorted_list_lock_writes shall decrement a counter to unlock the list for writes. ]*/
TEST_FUNCTION(clds_sorted_list_lock_does_not_block_clds_sorted_list_find_key)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);
    umock_c_reset_all_calls();

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, item, result);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_get_count */

/* Tests_SRS_CLDS_SORTED_LIST_42_035: [ If clds_sorted_list is NULL then clds_sorted_list_get_count shall fail and return CLDS_SORTED_LIST_GET_COUNT_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_get_count_with_NULL_clds_sorted_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    uint64_t item_count;

    // act
    CLDS_SORTED_LIST_GET_COUNT_RESULT result = clds_sorted_list_get_count(NULL, hazard_pointers_thread, &item_count);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_ERROR, result);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_42_036: [ If clds_hazard_pointers_thread is NULL then clds_sorted_list_get_count shall fail and return CLDS_SORTED_LIST_GET_COUNT_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_get_count_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count;

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_COUNT_RESULT result = clds_sorted_list_get_count(list, NULL, &item_count);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_ERROR, result);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_42_037: [ If item_count is NULL then clds_sorted_list_get_count shall fail and return CLDS_SORTED_LIST_GET_COUNT_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_get_count_with_NULL_item_count_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_COUNT_RESULT result = clds_sorted_list_get_count(list, hazard_pointers_thread, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_ERROR, result);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_42_038: [ If the counter to lock the list for writes is 0 then clds_sorted_list_get_count shall fail and return CLDS_SORTED_LIST_GET_COUNT_NOT_LOCKED. ]*/
TEST_FUNCTION(clds_sorted_list_get_count_without_lock_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count;

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_COUNT_RESULT result = clds_sorted_list_get_count(list, hazard_pointers_thread, &item_count);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_NOT_LOCKED, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_42_039: [ clds_sorted_list_get_count shall iterate over the items in the list and count them in item_count. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_42_040: [ clds_sorted_list_get_count shall succeed and return CLDS_SORTED_LIST_GET_COUNT_OK. ]*/
TEST_FUNCTION(clds_sorted_list_get_count_with_no_items_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count;

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_COUNT_RESULT result = clds_sorted_list_get_count(list, hazard_pointers_thread, &item_count);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_OK, result);
    ASSERT_ARE_EQUAL(uint64_t, 0, item_count);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_42_039: [ clds_sorted_list_get_count shall iterate over the items in the list and count them in item_count. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_42_040: [ clds_sorted_list_get_count shall succeed and return CLDS_SORTED_LIST_GET_COUNT_OK. ]*/
TEST_FUNCTION(clds_sorted_list_get_count_with_1_item_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_COUNT_RESULT result = clds_sorted_list_get_count(list, hazard_pointers_thread, &item_count);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_OK, result);
    ASSERT_ARE_EQUAL(uint64_t, 1, item_count);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_42_039: [ clds_sorted_list_get_count shall iterate over the items in the list and count them in item_count. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_42_040: [ clds_sorted_list_get_count shall succeed and return CLDS_SORTED_LIST_GET_COUNT_OK. ]*/
TEST_FUNCTION(clds_sorted_list_get_count_with_3_items_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count;

    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_1 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_payload_1->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);

    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_2 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_payload_2->key = 0x43;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);

    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_3 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_payload_3->key = 0x40;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_COUNT_RESULT result = clds_sorted_list_get_count(list, hazard_pointers_thread, &item_count);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_COUNT_RESULT, CLDS_SORTED_LIST_GET_COUNT_OK, result);
    ASSERT_ARE_EQUAL(uint64_t, 3, item_count);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_get_all */

/*Tests_SRS_CLDS_SORTED_LIST_42_041: [ If clds_sorted_list is NULL then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_get_all_with_NULL_clds_sorted_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    uint64_t item_count = 1;
    CLDS_SORTED_LIST_ITEM* items[1];

    // act
    CLDS_SORTED_LIST_GET_ALL_RESULT result = clds_sorted_list_get_all(NULL, hazard_pointers_thread, item_count, items);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_ERROR, result);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_042: [ If clds_hazard_pointers_thread is NULL then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_get_all_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count = 1;
    CLDS_SORTED_LIST_ITEM* items[1];

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_ALL_RESULT result = clds_sorted_list_get_all(list, NULL, item_count, items);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_ERROR, result);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_043: [ If item_count is 0 then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_get_all_with_0_item_count_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count = 0;
    CLDS_SORTED_LIST_ITEM* items[1];

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_ALL_RESULT result = clds_sorted_list_get_all(list, hazard_pointers_thread, item_count, items);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_ERROR, result);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_044: [ If items is NULL then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_get_all_with_NULL_items_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count = 1;

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_ALL_RESULT result = clds_sorted_list_get_all(list, hazard_pointers_thread, item_count, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_ERROR, result);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_045: [ If the counter to lock the list for writes is 0 then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_NOT_LOCKED. ]*/
TEST_FUNCTION(clds_sorted_list_get_all_without_lock_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count = 1;
    CLDS_SORTED_LIST_ITEM* items[1];

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_ALL_RESULT result = clds_sorted_list_get_all(list, hazard_pointers_thread, item_count, items);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_NOT_LOCKED, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_046: [ For each item in the list: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_047: [ clds_sorted_list_get_all shall increment the ref count. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_048: [ clds_sorted_list_get_all shall store the pointer in items. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_050: [ clds_sorted_list_get_all shall succeed and return CLDS_SORTED_LIST_GET_ALL_OK. ]*/
TEST_FUNCTION(clds_sorted_list_get_all_with_1_item_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count = 1;
    CLDS_SORTED_LIST_ITEM* items[1];
    items[0] = NULL;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item, NULL);

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_ALL_RESULT result = clds_sorted_list_get_all(list, hazard_pointers_thread, item_count, items);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_OK, result);

    ASSERT_IS_NOT_NULL(items[0]);
    TEST_ITEM* result_item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, items[0]);
    ASSERT_ARE_EQUAL(uint32_t, 0x42, result_item_payload->key);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, items[0]);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_046: [ For each item in the list: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_047: [ clds_sorted_list_get_all shall increment the ref count. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_048: [ clds_sorted_list_get_all shall store the pointer in items. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_050: [ clds_sorted_list_get_all shall succeed and return CLDS_SORTED_LIST_GET_ALL_OK. ]*/
TEST_FUNCTION(clds_sorted_list_get_all_with_3_items_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count = 3;
    CLDS_SORTED_LIST_ITEM* items[3];
    items[0] = NULL;
    items[1] = NULL;
    items[2] = NULL;

    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_1 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_payload_1->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);

    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_2 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_payload_2->key = 0x43;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);

    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_3 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_payload_3->key = 0x40;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_ALL_RESULT result = clds_sorted_list_get_all(list, hazard_pointers_thread, item_count, items);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_OK, result);

    ASSERT_IS_NOT_NULL(items[0]);
    TEST_ITEM* result_item_payload_1 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, items[0]);
    ASSERT_ARE_EQUAL(uint32_t, 0x40, result_item_payload_1->key);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, items[0]);

    ASSERT_IS_NOT_NULL(items[1]);
    TEST_ITEM* result_item_payload_2 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, items[1]);
    ASSERT_ARE_EQUAL(uint32_t, 0x42, result_item_payload_2->key);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, items[1]);

    ASSERT_IS_NOT_NULL(items[2]);
    TEST_ITEM* result_item_payload_3 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, items[2]);
    ASSERT_ARE_EQUAL(uint32_t, 0x43, result_item_payload_3->key);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, items[2]);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_049: [ If item_count does not match the number of items in the list then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_WRONG_SIZE. ]*/
TEST_FUNCTION(clds_sorted_list_get_all_with_3_items_but_item_count_1_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count = 1;
    CLDS_SORTED_LIST_ITEM* items[1];

    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_1 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_payload_1->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);

    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_2 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_payload_2->key = 0x43;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);

    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_3 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_payload_3->key = 0x40;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3, NULL);

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_ALL_RESULT result = clds_sorted_list_get_all(list, hazard_pointers_thread, item_count, items);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_WRONG_SIZE, result);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_049: [ If item_count does not match the number of items in the list then clds_sorted_list_get_all shall fail and return CLDS_SORTED_LIST_GET_ALL_WRONG_SIZE. ]*/
TEST_FUNCTION(clds_sorted_list_get_all_with_1_item_but_item_count_2_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = real_clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = real_clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    uint64_t item_count = 2;
    CLDS_SORTED_LIST_ITEM* items[2];

    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload_1 = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_payload_1->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);

    clds_sorted_list_lock_writes(list);

    umock_c_reset_all_calls();

    // act
    CLDS_SORTED_LIST_GET_ALL_RESULT result = clds_sorted_list_get_all(list, hazard_pointers_thread, item_count, items);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_GET_ALL_RESULT, CLDS_SORTED_LIST_GET_ALL_WRONG_SIZE, result);

    // cleanup
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_node_create */

/* Tests_SRS_CLDS_SORTED_LIST_01_036: [ item_cleanup_callback shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_sorted_list_node_create_with_NULL_item_cleanup_callback_succeeds)
{
    // arrange
    CLDS_SORTED_LIST_ITEM* item;

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_037: [ item_cleanup_callback_context shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_sorted_list_node_create_with_NULL_item_cleanup_callback_context_succeeds)
{
    // arrange
    CLDS_SORTED_LIST_ITEM* item;

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
}

/* clds_sorted_list_set_value */

/* Tests_SRS_CLDS_SORTED_LIST_01_081: [ If clds_sorted_list is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_with_NULL_clds_sorted_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_1_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_set_value(NULL, hazard_pointers_thread, (void*)0x42, item_1, &old_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_ERROR, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_1);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_082: [ If clds_hazard_pointers_thread is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    volatile int64_t sequence_number = 45;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_1_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_set_value(list, NULL, (void*)0x42, item_1, &old_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_ERROR, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_1);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_083: [ If key is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_1_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, NULL, item_1, &old_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_ERROR, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_1);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_084: [ If new_item is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_with_NULL_new_item_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x42, NULL, &old_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_085: [ If old_item is NULL, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_with_NULL_old_item_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    item_1_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x42, item_1, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_ERROR, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_1);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_086: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_sorted_list_create, clds_sorted_list_set_value shall fail and return CLDS_SORTED_LIST_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_with_NULL_sequence_number_when_no_start_sequence_number_was_specified_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    int64_t set_value_seq_no;
    item_1_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x42, item_1, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_ERROR, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_1);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_087: [ If the key entry does not exist in the list, new_item shall be inserted at the key position. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_090: [ For each set value the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_when_the_key_is_not_in_the_list_inserts_the_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    volatile int64_t sequence_number = 45;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_ITEM* result_item_1;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    int64_t set_value_seq_no;
    item_1_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x42, item_1, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, result);
    ASSERT_ARE_EQUAL(int64_t, 46, set_value_seq_no);
    result_item_1 = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NOT_NULL(result_item_1);
    ASSERT_IS_NULL(old_item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result_item_1);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_088: [ If the key entry exists in the list, its value shall be replaced with new_item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_090: [ For each set value the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_when_the_key_is_in_the_list_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    volatile int64_t sequence_number = 45;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_ITEM* result_item_2;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    int64_t set_value_seq_no;
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x42, item_2, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, result);
    ASSERT_ARE_EQUAL(int64_t, 47, set_value_seq_no);
    result_item_2 = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NOT_NULL(result_item_2);
    ASSERT_ARE_EQUAL(void_ptr, item_1, old_item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, old_item);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result_item_2);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_088: [ If the key entry exists in the list, its value shall be replaced with new_item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_090: [ For each set value the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_when_the_key_is_in_the_list_for_the_2nd_item_in_the_list_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    volatile int64_t sequence_number = 45;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4244);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_ITEM* result_item_3;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    int64_t set_value_seq_no;
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x43, item_3, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, result);
    ASSERT_ARE_EQUAL(int64_t, 48, set_value_seq_no);
    result_item_3 = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x43);
    ASSERT_IS_NOT_NULL(result_item_3);
    ASSERT_ARE_EQUAL(void_ptr, item_2, old_item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, old_item);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result_item_3);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_091: [ If no start sequence number was provided in clds_sorted_list_create and sequence_number is NULL, no sequence number computations shall be done. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_when_the_key_is_in_the_list_for_the_2nd_item_in_the_list_succeeds_no_seq_computation)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, NULL, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4244);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_ITEM* result_item_3;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x43, item_3, &old_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, result);
    result_item_3 = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x43);
    ASSERT_IS_NOT_NULL(result_item_3);
    ASSERT_ARE_EQUAL(void_ptr, item_2, old_item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, old_item);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result_item_3);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_092: [ If the sequence_number argument passed to clds_sorted_list_set_value is NULL, the computed sequence number for the remove shall still be computed but it shall not be provided to the user. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_with_NULL_sequence_number_still_increments_the_seq_no)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    volatile int64_t sequence_number = 45;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4244);
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_ITEM* result_item_2;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    int64_t set_value_seq_no;
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL);
    (void)clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x43, item_3, &old_item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_3, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x43, item_2, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, result);
    ASSERT_ARE_EQUAL(int64_t, 48, set_value_seq_no);
    result_item_2 = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x43);
    ASSERT_IS_NOT_NULL(result_item_2);
    ASSERT_ARE_EQUAL(void_ptr, item_3, old_item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, old_item);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, result_item_2);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_080: [ clds_sorted_list_set_value shall replace in the list the item that matches the criteria given by the compare function passed to clds_sorted_list_create with new_item and on success it shall return CLDS_SORTED_LIST_SET_VALUE_OK. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_088: [ If the key entry exists in the list, its value shall be replaced with new_item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_089: [ The previous value shall be returned in old_item. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_090: [ For each set value the order of the operation shall be computed based on the start sequence number passed to clds_sorted_list_create. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_with_the_same_item_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, NULL, NULL);
    ASSERT_IS_NOT_NULL(list);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    ASSERT_IS_NOT_NULL(item_1);
    CLDS_SORTED_LIST_ITEM* old_item = NULL;
    CLDS_SORTED_LIST_SET_VALUE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    int64_t set_value_seq_no;
    item_1_payload->key = 0x42;
    ASSERT_ARE_EQUAL(int, 0, clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1));
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, clds_sorted_list_insert(list, hazard_pointers_thread, item_1, NULL));
    // unfortunately the semantics should have been here to have clds inc ref all the items passed to it
    // another rainy day project to fix that
    CLDS_SORTED_LIST_NODE_INC_REF(TEST_ITEM, item_1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_set_value(list, hazard_pointers_thread, (void*)0x42, item_1, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, result);
    ASSERT_ARE_EQUAL(int64_t, 47, set_value_seq_no);
    ASSERT_IS_NOT_NULL(old_item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, old_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(clds_sorted_list_unittests)
