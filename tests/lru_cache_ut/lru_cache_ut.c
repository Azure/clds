// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "real_gballoc_ll.h"

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_stdint.h"
#include "umock_c/umocktypes_bool.h"
#include "umock_c/umock_c_negative_tests.h"

#include "c_pal/interlocked.h"

#define ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/srw_lock_ll.h"

#include "c_util/doublylinkedlist.h"

#include "clds/clds_hash_table.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hazard_pointers_thread_helper.h"

#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"
#include "real_srw_lock_ll.h"

#include "../reals/real_clds_hazard_pointers.h"
#include "../reals/real_clds_hash_table.h"


#include "clds/lru_cache.h"

TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_CONDITION_CHECK_RESULT, CLDS_CONDITION_CHECK_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_CONDITION_CHECK_RESULT, CLDS_CONDITION_CHECK_RESULT_VALUES);

TEST_DEFINE_ENUM_TYPE(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_RESULT_VALUES);

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)


static CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE g_clds_hazard_pointers_thread_handle = (CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE)0x1301;

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

static CLDS_HASH_TABLE_HANDLE my_clds_hash_table_create(COMPUTE_HASH_FUNC compute_hash, KEY_COMPARE_FUNC key_compare_func, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, volatile_atomic int64_t* start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb, void* skipped_seq_no_cb_context)
{
    return real_clds_hash_table_create(compute_hash, key_compare_func, initial_bucket_size, clds_hazard_pointers, start_sequence_number, skipped_seq_no_cb, skipped_seq_no_cb_context);
}

MOCK_FUNCTION_WITH_CODE(, void, test_item_cleanup_func, void*, context, struct CLDS_HASH_TABLE_ITEM_TAG*, item)
MOCK_FUNCTION_END()

MOCK_FUNCTION_WITH_CODE(, uint64_t, test_compute_hash, void*, key)
    (void)key;
MOCK_FUNCTION_END((uint64_t)key)

MOCK_FUNCTION_WITH_CODE(, void, test_skipped_seq_no_cb, void*, context, int64_t, skipped_seq_no)
MOCK_FUNCTION_END()

static CLDS_CONDITION_CHECK_RESULT g_condition_check_result = CLDS_CONDITION_CHECK_OK;
MOCK_FUNCTION_WITH_CODE(, CLDS_CONDITION_CHECK_RESULT, test_item_condition_check, void*, context, void*, new_key, void*, old_key)
MOCK_FUNCTION_END(g_condition_check_result)

static int test_key_compare_func(void* key_1, void* key_2)
{
    int result;
    if (key_1 < key_2)
    {
        result = -1;
    }
    else if (key_1 > key_2)
    {
        result = 1;
    }
    else
    {
        result = 0;
    }

    return result;
}

typedef struct TEST_ITEM_TAG
{
    int dummy;
} TEST_ITEM;

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM)

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, real_gballoc_hl_init(NULL, NULL));

    ASSERT_ARE_EQUAL(int, 0, umock_c_init(on_umock_c_error), "umock_c_init failed");

    ASSERT_ARE_EQUAL(int, 0, umocktypes_stdint_register_types(), "umocktypes_stdint_register_types failed");
    ASSERT_ARE_EQUAL(int, 0, umocktypes_bool_register_types(), "umocktypes_bool_register_types failed");

    REGISTER_GBALLOC_HL_GLOBAL_MOCK_HOOK();
    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_HASH_TABLE_GLOBAL_MOCK_HOOKS();
    REGISTER_SRW_LOCK_LL_GLOBAL_MOCK_HOOK();


    REGISTER_GLOBAL_MOCK_FAIL_RETURN(clds_sorted_list_get_count, CLDS_SORTED_LIST_GET_COUNT_ERROR);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(clds_sorted_list_get_all, CLDS_SORTED_LIST_GET_ALL_ERROR);

    REGISTER_GLOBAL_MOCK_HOOK(clds_hash_table_create, my_clds_hash_table_create);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(clds_hash_table_create, NULL);

    REGISTER_GLOBAL_MOCK_RETURNS(clds_hazard_pointers_thread_helper_create, g_clds_hazard_pointers_thread_handle, NULL);

    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LRU_CACHE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(COMPUTE_HASH_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(KEY_COMPARE_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(HASH_TABLE_SKIPPED_SEQ_NO_CB, void*);
    REGISTER_UMOCK_ALIAS_TYPE(PDLIST_ENTRY, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HASH_TABLE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, void*);

    REGISTER_TYPE(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_RESULT);


    ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_negative_tests_deinit();
    umock_c_deinit();

    real_gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    g_condition_check_result = CLDS_CONDITION_CHECK_OK;
    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

typedef struct CLDS_HASH_TABLE_TEST_CONTEXT_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread;
    volatile_atomic int64_t start_seq_no;
} CLDS_HASH_TABLE_TEST_CONTEXT;

static void setup_test_context(CLDS_HASH_TABLE_TEST_CONTEXT* test_context)
{
    test_context->hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(test_context->hazard_pointers);
    test_context->hazard_pointers_thread = clds_hazard_pointers_register_thread(test_context->hazard_pointers);
    ASSERT_IS_NOT_NULL(test_context->hazard_pointers_thread);

    (void)interlocked_exchange_64(&test_context->start_seq_no, 0);

    umock_c_reset_all_calls();
}

static void destroy_test_context(CLDS_HASH_TABLE_TEST_CONTEXT* test_context)
{
    clds_hazard_pointers_destroy(test_context->hazard_pointers);
}

static void set_lru_create_expectations(size_t bucket_size, CLDS_HAZARD_POINTERS_HANDLE hazard_pointers)
{
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_create(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_create(IGNORED_ARG, IGNORED_ARG, bucket_size, hazard_pointers, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_init(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_ARG));
}
/* lru_cache_create */

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ clds_hash_table_create shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_027: [ The hash table shall maintain a list of arrays of buckets, so that it can be resized as needed. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_058: [ start_sequence_number shall be allowed to be NULL, in which case no sequence number computations shall be performed. ]*/
TEST_FUNCTION(lru_cache_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    size_t bucket_size = 1024;

    umock_c_reset_all_calls();

    set_lru_create_expectations(bucket_size, hazard_pointers);

    // act
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, hazard_pointers, capacity);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(lru_cache);

    // cleanup
    lru_cache_destroy(lru_cache);
    clds_hazard_pointers_destroy(hazard_pointers);
}


TEST_FUNCTION(lru_cache_create_with_null_compute_hash_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    size_t bucket_size = 1024;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(NULL, test_key_compare_func, bucket_size, hazard_pointers, capacity);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(lru_cache_create_with_null_key_compare_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    size_t bucket_size = 1024;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(test_compute_hash, NULL, bucket_size, hazard_pointers, capacity);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(lru_cache_create_with_bucket_size_0_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, 0, hazard_pointers, capacity);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(lru_cache_create_with_null_hazard_pointers_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    size_t bucket_size = 1024;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, NULL, capacity);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
}


TEST_FUNCTION(lru_cache_create_with_capacity_0_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    LRU_CACHE_HANDLE lru_cache;
    size_t bucket_size = 1024;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, hazard_pointers, 0);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(when_underlying_calls_fail_lru_cache_create_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    size_t bucket_size = 1024;

    umock_c_reset_all_calls();

    set_lru_create_expectations(bucket_size, hazard_pointers);

    umock_c_negative_tests_snapshot();

    for (int i = 0; i < umock_c_negative_tests_call_count(); i++)
    {

        if (umock_c_negative_tests_can_call_fail(i))
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, hazard_pointers, capacity);

            // assert
            ASSERT_IS_NULL(lru_cache, "On failed call %u", i);
        }
    }

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* lru_cache_destroy */

TEST_FUNCTION(lru_cache_destroy_frees_the_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE hazard_pointers_thread;
    CLDS_HASH_TABLE_HANDLE hash_table;
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    size_t bucket_size = 1024;

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_create(IGNORED_ARG)).CaptureReturn(&hazard_pointers_thread);
    STRICT_EXPECTED_CALL(clds_hash_table_create(IGNORED_ARG, IGNORED_ARG, bucket_size, hazard_pointers, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG)).CaptureReturn(&hash_table);
    STRICT_EXPECTED_CALL(srw_lock_ll_init(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_ARG));

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, hazard_pointers, capacity);
    ASSERT_IS_NOT_NULL(lru_cache);

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(srw_lock_ll_deinit(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_destroy(hash_table));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_destroy(hazard_pointers_thread));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    lru_cache_destroy(lru_cache);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_BSI_LOCAL_01_007: [ If bsi_local is NULL, bsi_local_destroy shall return. ]*/
TEST_FUNCTION(lru_cache_destroy_with_NULL_handle_returns)
{
    // arrange

    // act
    lru_cache_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}



END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
