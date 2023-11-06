// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <inttypes.h>
#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "real_gballoc_ll.h"

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_stdint.h"
#include "umock_c/umock_c_negative_tests.h"

#define ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/interlocked.h"
#include "c_pal/srw_lock_ll.h"

#include "c_util/doublylinkedlist.h"

#include "clds/clds_hash_table.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hazard_pointers_thread_helper.h"

#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"
#include "real_srw_lock_ll.h"
#include "real_doublylinkedlist.h"

#include "../reals/real_interlocked.h"
#include "../reals/real_clds_hash_table.h"
#include "../reals/real_clds_hazard_pointers.h"
#include "../reals/real_clds_hazard_pointers_thread_helper.h"

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
TEST_DEFINE_ENUM_TYPE(LRU_CACHE_EVICT_RESULT, LRU_CACHE_EVICT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(LRU_CACHE_EVICT_RESULT, LRU_CACHE_EVICT_RESULT_VALUES);

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static CLDS_HAZARD_POINTERS_HANDLE test_clds_hazard_pointers;
static CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE test_clds_hazard_pointers_thread_helper;

static LRU_CACHE_EVICT_CALLBACK_FUNC  g_test_hl_complete_cb;
static void* g_test_hl_complete_cb_context;

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

static CLDS_HASH_TABLE_HANDLE my_clds_hash_table_create(COMPUTE_HASH_FUNC compute_hash, KEY_COMPARE_FUNC key_compare_func, size_t initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, volatile_atomic int64_t* start_sequence_number, HASH_TABLE_SKIPPED_SEQ_NO_CB skipped_seq_no_cb, void* skipped_seq_no_cb_context)
{
    return real_clds_hash_table_create(compute_hash, key_compare_func, initial_bucket_size, clds_hazard_pointers, start_sequence_number, skipped_seq_no_cb, skipped_seq_no_cb_context);
}

MOCK_FUNCTION_WITH_CODE(, uint64_t, test_compute_hash, void*, key)
    (void)key;
MOCK_FUNCTION_END((uint64_t)key)

MOCK_FUNCTION_WITH_CODE(, void, test_eviction_callback, void*, context, LRU_CACHE_EVICT_RESULT, cache_evict_status, void*, evicted_value)
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


MOCK_FUNCTION_WITH_CODE(, void, test_on_error, void*, context)
MOCK_FUNCTION_END()
static void* test_error_context = (void*)13;


BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, real_gballoc_hl_init(NULL, NULL));

    ASSERT_ARE_EQUAL(int, 0, umock_c_init(on_umock_c_error), "umock_c_init failed");

    ASSERT_ARE_EQUAL(int, 0, umocktypes_stdint_register_types(), "umocktypes_stdint_register_types failed");

    REGISTER_GBALLOC_HL_GLOBAL_MOCK_HOOK();
    REGISTER_CLDS_HASH_TABLE_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_HAZARD_POINTERS_THREAD_HELPER_GLOBAL_MOCK_HOOKS();
    REGISTER_SRW_LOCK_LL_GLOBAL_MOCK_HOOK();
    REGISTER_DOUBLYLINKEDLIST_GLOBAL_MOCK_HOOKS();
    REGISTER_INTERLOCKED_GLOBAL_MOCK_HOOK();

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(clds_sorted_list_get_count, CLDS_SORTED_LIST_GET_COUNT_ERROR);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(clds_sorted_list_get_all, CLDS_SORTED_LIST_GET_ALL_ERROR);

    REGISTER_GLOBAL_MOCK_HOOK(clds_hash_table_create, my_clds_hash_table_create);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(clds_hash_table_create, NULL);

    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LRU_CACHE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(COMPUTE_HASH_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(KEY_COMPARE_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(HASH_TABLE_SKIPPED_SEQ_NO_CB, void*);
    REGISTER_UMOCK_ALIAS_TYPE(HASH_TABLE_ITEM_CLEANUP_CB, void*);
    REGISTER_UMOCK_ALIAS_TYPE(PDLIST_ENTRY, void*);
    REGISTER_UMOCK_ALIAS_TYPE(const PDLIST_ENTRY, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HASH_TABLE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_THREAD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTER_RECORD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(RECLAIM_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CONDITION_CHECK_CB, void*);

    REGISTER_TYPE(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_RESULT);
    REGISTER_TYPE(LRU_CACHE_EVICT_RESULT, LRU_CACHE_EVICT_RESULT);
    REGISTER_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT);
    REGISTER_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT);
    REGISTER_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT);

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
    test_clds_hazard_pointers = real_clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(test_clds_hazard_pointers, "Cannot create hazard pointers");

    test_clds_hazard_pointers_thread_helper = real_clds_hazard_pointers_thread_helper_create(test_clds_hazard_pointers);
    ASSERT_IS_NOT_NULL(test_clds_hazard_pointers, "Cannot create hazard pointers");

    g_condition_check_result = CLDS_CONDITION_CHECK_OK;
    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    real_clds_hazard_pointers_thread_helper_destroy(test_clds_hazard_pointers_thread_helper);
    real_clds_hazard_pointers_destroy(test_clds_hazard_pointers);
}


static void setup_ignore_hazard_pointers_calls(void)
{
    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_ARG, IGNORED_ARG))
        .IgnoreAllCalls()
        .CallCannotFail();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_ARG, IGNORED_ARG))
        .IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG))
        .IgnoreAllCalls();
}

static void setup_lock_and_inserttail(int64_t size)
{
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, size));
    STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_node_release(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_ARG, IGNORED_ARG));
}

static void set_lru_create_expectations(uint32_t bucket_size, CLDS_HAZARD_POINTERS_HANDLE hazard_pointers)
{
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_create(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_create(IGNORED_ARG, IGNORED_ARG, bucket_size, hazard_pointers, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_init(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_ARG));
}

static void set_lru_put_insert_expectations(void* key, CLDS_HASH_TABLE_ITEM** hash_table_item, int64_t size)
{
    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_get_thread(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(clds_hash_table_node_create(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG))
        .CaptureReturn(hash_table_item);
    STRICT_EXPECTED_CALL(clds_hash_table_set_value(IGNORED_ARG, IGNORED_ARG, key, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_compute_hash(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, size));
    STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));
}

static void set_lru_put_set_value_expectations(void* key, int64_t old_size, int64_t new_size)
{
    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_get_thread(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(clds_hash_table_node_create(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_set_value(IGNORED_ARG, IGNORED_ARG, key, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_compute_hash(IGNORED_ARG));
    setup_lock_and_inserttail(-old_size + new_size);
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));
}

static void set_lru_put_evict_expectations(void* key, int64_t current_size, int64_t old_node_size)
{
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(DList_IsListEmpty(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, current_size - old_node_size, current_size));
    STRICT_EXPECTED_CALL(clds_hash_table_remove(IGNORED_ARG, IGNORED_ARG, key, IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_compute_hash(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_eviction_callback(IGNORED_ARG, LRU_CACHE_EVICT_OK, IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_node_release(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));
}

static void set_lru_put_nothing_to_evict_expectations()
{
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));
}

static void set_lru_get_value_expectations(void* key)
{
    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_get_thread(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_find(IGNORED_ARG, IGNORED_ARG, key));
    STRICT_EXPECTED_CALL(test_compute_hash(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_node_release(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));
}

/* lru_cache_create */

/*Tests_SRS_LRU_CACHE_13_011: [ lru_cache_create shall allocate memory for LRU_CACHE_HANDLE. ]*/
/*Tests_SRS_LRU_CACHE_13_013: [ lru_cache_create shall assign clds_hazard_pointers to LRU_CACHE_HANDLE. ]*/
/*Tests_SRS_LRU_CACHE_13_014: [ lru_cache_create shall allocate CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE by calling clds_hazard_pointers_thread_helper_create. ]*/
/*Tests_SRS_LRU_CACHE_13_015: [ lru_cache_create shall allocate clds_hash_table by calling clds_hash_table_create. ]*/
/*Tests_SRS_LRU_CACHE_13_016: [ lru_cache_create shall initialize SRW_LOCK_LL by calling srw_lock_ll_init. ]*/
/*Tests_SRS_LRU_CACHE_13_017: [ lru_cache_create shall initialize the doubly linked list by calling DList_InitializeListHead. ]*/
/*Tests_SRS_LRU_CACHE_13_018: [ lru_cache_create shall assign 0 to current_size and the capacity to capacity. ]*/
/*Tests_SRS_LRU_CACHE_13_019: [ On success, lru_cache_create shall return LRU_CACHE_HANDLE. ]*/
TEST_FUNCTION(lru_cache_create_succeeds)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;

    umock_c_reset_all_calls();

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    // act
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);

    // assert
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_001: [ If compute_hash is NULL, lru_cache_create shall fail and return NULL. ]*/
TEST_FUNCTION(lru_cache_create_with_null_compute_hash_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(NULL, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
}

/*Tests_SRS_LRU_CACHE_13_002: [ If key_compare_func is NULL, lru_cache_create shall fail and return NULL. ]*/
TEST_FUNCTION(lru_cache_create_with_null_key_compare_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(test_compute_hash, NULL, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
}

/*Tests_SRS_LRU_CACHE_13_003: [ If initial_bucket_size is 0, lru_cache_create shall fail and return NULL. ]*/
TEST_FUNCTION(lru_cache_create_with_bucket_size_0_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, 0, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
}

/*Tests_SRS_LRU_CACHE_13_004: [ If clds_hazard_pointers is NULL, lru_cache_create shall fail and return NULL. ]*/
TEST_FUNCTION(lru_cache_create_with_null_hazard_pointers_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, NULL, capacity, test_on_error, test_error_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
}

/*Tests_SRS_LRU_CACHE_13_010: [ If capacity is less than or equals 0, then lru_cache_create shall fail and return NULL. ]*/
TEST_FUNCTION(lru_cache_create_with_capacity_0_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    uint32_t bucket_size = 1024;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, 0, test_on_error, test_error_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
}

/*Tests_SRS_LRU_CACHE_13_079: [ If on_error_callback is NULL then lru_cache_create shall fail and return NULL. ]*/
TEST_FUNCTION(lru_cache_create_with_on_error_callback_NULL_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;

    umock_c_reset_all_calls();

    // act
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, NULL, test_error_context);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(lru_cache);

    // cleanup
}

/*Tests_SRS_LRU_CACHE_13_020: [ If there are any failures then lru_cache_create shall fail and return NULL. ]*/
TEST_FUNCTION(when_underlying_calls_fail_lru_cache_create_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;

    umock_c_reset_all_calls();

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {

        if (umock_c_negative_tests_can_call_fail(i))
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);

            // assert
            ASSERT_IS_NULL(lru_cache, "On failed call %zu", i);
        }
    }

    // cleanup
}

/* lru_cache_destroy */

/*Tests_SRS_LRU_CACHE_13_022: [ lru_cache_destroy shall free all resources associated with the LRU_CACHE_HANDLE. ]*/
TEST_FUNCTION(lru_cache_destroy_frees_the_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE hazard_pointers_thread;
    CLDS_HASH_TABLE_HANDLE hash_table;
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_create(IGNORED_ARG)).CaptureReturn(&hazard_pointers_thread);
    STRICT_EXPECTED_CALL(clds_hash_table_create(IGNORED_ARG, IGNORED_ARG, bucket_size, hazard_pointers, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG)).CaptureReturn(&hash_table);
    STRICT_EXPECTED_CALL(srw_lock_ll_init(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_ARG));

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

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

/*Tests_SRS_LRU_CACHE_13_021: [ If lru_cache is NULL, then lru_cache_destroy shall fail. ]*/
TEST_FUNCTION(lru_cache_destroy_with_NULL_handle_returns)
{
    // arrange

    // act
    lru_cache_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* lru_cache_put */

/*Tests_SRS_LRU_CACHE_13_023: [ If lru_handle is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
TEST_FUNCTION(lru_cache_put_with_null_handle_fails)
{
    // arrange
    int key1 = 10, value = 1000, size1 = 2;

    LRU_CACHE_PUT_RESULT result = lru_cache_put(NULL, &key1, &value, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    //cleanup
}

/*Tests_SRS_LRU_CACHE_13_024: [ If key is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
TEST_FUNCTION(lru_cache_put_with_null_key_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int value = 1000, size1 = 2;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();


    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, NULL, &value, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    //cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_025: [ If value is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
TEST_FUNCTION(lru_cache_put_with_null_value_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key1 = 1000, size1 = 2;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();


    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key1, NULL, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    //cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_026: [ If size is less than or equals to 0, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
TEST_FUNCTION(lru_cache_put_with_0_size_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key1 = 1, value = 1000;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();


    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key1, &value, 0, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    //cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_075: [ If evict_callback is NULL, then lru_cache_put shall fail and return LRU_CACHE_PUT_ERROR. ]*/
TEST_FUNCTION(lru_cache_put_with_NULL_callback_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key1 = 1, value = 1000, size1 = 2;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();


    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key1, &value, size1, NULL, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    //cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_027: [ If size is greater than capacity of lru cache, then lru_cache_put shall fail and return LRU_CACHE_PUT_VALUE_INVALID_SIZE. ]*/
TEST_FUNCTION(lru_cache_put_with_size_bigger_than_capacity_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    int64_t size1 = capacity + 1;
    uint32_t bucket_size = 1024;
    int key1 = 1, value = 1000;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);
    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key1, &value, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_VALUE_INVALID_SIZE, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    //cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_076: [ context may be NULL. ]*/
/*Tests_SRS_LRU_CACHE_13_028: [ lru_cache_put shall get CLDS_HAZARD_POINTERS_THREAD_HANDLE by calling clds_hazard_pointers_thread_helper_get_thread. ]*/
/*Tests_SRS_LRU_CACHE_13_071: [ Otherwise, if the key is not found: ]*/
/*Tests_SRS_LRU_CACHE_13_062: [ lru_cache_put shall add the item size to the current_size. ]*/
/*Tests_SRS_LRU_CACHE_13_049: [ On success, lru_cache_put shall return LRU_CACHE_PUT_OK. ]*/
TEST_FUNCTION(lru_cache_put_succeeds)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, size = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item, size);
    set_lru_put_nothing_to_evict_expectations();

    // act
    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_076: [ context may be NULL. ]*/
/*Tests_SRS_LRU_CACHE_13_028: [ lru_cache_put shall get CLDS_HAZARD_POINTERS_THREAD_HANDLE by calling clds_hazard_pointers_thread_helper_get_thread. ]*/
/*Tests_SRS_LRU_CACHE_13_064: [ lru_cache_put shall create LRU Node item to be updated in the hash table. ]*/
/*Tests_SRS_LRU_CACHE_13_065: [ lru_cache_put shall update the LRU Node item in the hash table by calling clds_hash_table_set_value. ]*/
/*Tests_SRS_LRU_CACHE_13_033: [ lru_cache_put shall acquire the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_077: [ lru_cache_put shall remove the old node from the list by calling DList_RemoveEntryList. ]*/
/*Tests_SRS_LRU_CACHE_13_066: [ lru_cache_put shall append the updated node to the tail to maintain the order. ]*/
/*Tests_SRS_LRU_CACHE_13_036: [ lru_cache_put shall release the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_070: [ lru_cache_put shall update the current_size with the new size and removes the old value size. ]*/
/*Tests_SRS_LRU_CACHE_13_067: [ lru_cache_put shall free the old value. ]*/
/*Tests_SRS_LRU_CACHE_13_068: [ lru_cache_put shall return with LRU_CACHE_PUT_OK. ]*/
TEST_FUNCTION(lru_cache_put_twice_succeeds)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, size = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item, size);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    set_lru_put_set_value_expectations(&key, size, size);
    set_lru_put_nothing_to_evict_expectations();

    // act
    result = lru_cache_put(lru_cache, &key, &value, size, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_080: [ If current_size with size exceeds INT64_MAX, then lru_cache_put shall fail and return LRU_CACHE_PUT_VALUE_INVALID_SIZE. ]*/
TEST_FUNCTION(lru_cache_put_overflows_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = INT64_MAX - 10;
    uint32_t bucket_size = 1024;
    int64_t key = 10, key2 = 11, value = 1000, size = INT64_MAX / 2;
    CLDS_HASH_TABLE_ITEM* hash_table_item;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item, size);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_get_thread(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));


    // act
    result = lru_cache_put(lru_cache, &key2, &value, size + 100, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_VALUE_INVALID_SIZE, result);

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_037: [ While the current size of the cache exceeds capacity: ]*/
/*Tests_SRS_LRU_CACHE_13_040: [ lru_cache_put shall acquire the lock in exclusive. ]*/
/*Tests_SRS_LRU_CACHE_13_038: [ lru_cache_put shall get the least used node which is Flink of head node. ]*/
/*Tests_SRS_LRU_CACHE_13_042: [ lru_cache_put shall release the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_072: [ lru_cache_put shall decrement the least used node size from current_size. ]*/
/*Tests_SRS_LRU_CACHE_13_039: [ The least used node is removed from clds_hash_table by calling clds_hash_table_remove. ]*/
/*Tests_SRS_LRU_CACHE_13_041: [ lru_cache_put shall remove the old node from the list by calling DList_RemoveEntryList. ]*/
/*Tests_SRS_LRU_CACHE_13_043: [ On success, evict_callback is called with the status LRU_CACHE_EVICT_OK and the evicted item. ]*/
/*Tests_SRS_LRU_CACHE_13_049: [ On success, lru_cache_put shall return LRU_CACHE_PUT_OK. ]*/
TEST_FUNCTION(lru_cache_put_triggers_eviction_when_capacity_full_succeeds)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, key2 = 11, size1 = 2, size2 = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_2;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item_1, size1);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    set_lru_put_insert_expectations(&key2, &hash_table_item_2, size2);

    set_lru_put_evict_expectations(&key, capacity+size2, size1);
    set_lru_put_nothing_to_evict_expectations();

    // act
    result = lru_cache_put(lru_cache, &key2, &value, size2, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_037: [ While the current size of the cache exceeds capacity: ]*/
/*Tests_SRS_LRU_CACHE_13_040: [ lru_cache_put shall acquire the lock in exclusive. ]*/
/*Tests_SRS_LRU_CACHE_13_038: [ lru_cache_put shall get the least used node which is Flink of head node. ]*/
/*Tests_SRS_LRU_CACHE_13_042: [ lru_cache_put shall release the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_072: [ lru_cache_put shall decrement the least used node size from current_size. ]*/
/*Tests_SRS_LRU_CACHE_13_039: [ The least used node is removed from clds_hash_table by calling clds_hash_table_remove. ]*/
/*Tests_SRS_LRU_CACHE_13_041: [ lru_cache_put shall remove the old node from the list by calling DList_RemoveEntryList. ]*/
/*Tests_SRS_LRU_CACHE_13_043: [ On success, evict_callback is called with the status LRU_CACHE_EVICT_OK and the evicted item. ]*/
/*Tests_SRS_LRU_CACHE_13_049: [ On success, lru_cache_put shall return LRU_CACHE_PUT_OK. ]*/
TEST_FUNCTION(lru_cache_put_triggers_eviction_twice_when_capacity_full_succeeds)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key1 = 10, value = 1000, key2 = 11, key3 = 12, size1 = 1, size2 = 1, size3 = 2;
    CLDS_HASH_TABLE_ITEM* hash_table_item_1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_2;
    CLDS_HASH_TABLE_ITEM* hash_table_item_3;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key1, &hash_table_item_1, size1);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key1, &value, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    set_lru_put_insert_expectations(&key2, &hash_table_item_2, size2);
    set_lru_put_nothing_to_evict_expectations();

    result = lru_cache_put(lru_cache, &key2, &value, size2, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    // insert key3
    set_lru_put_insert_expectations(&key3, &hash_table_item_3, size3);
    // evicting least used key1
    set_lru_put_evict_expectations(&key1, capacity+size3, size1);
    //evicting least used key2 next
    set_lru_put_evict_expectations(&key2, capacity+size3-size1, size2);
    set_lru_put_nothing_to_evict_expectations();

    // act
    result = lru_cache_put(lru_cache, &key3, &value, size3, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_030: [ If the key is found: ]*/
/*Tests_SRS_LRU_CACHE_13_065: [ lru_cache_put shall update the LRU Node item in the hash table by calling clds_hash_table_set_value. ]*/
/*Tests_SRS_LRU_CACHE_13_064: [ lru_cache_put shall create LRU Node item to be updated in the hash table. ]*/
/*Tests_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
TEST_FUNCTION(lru_cache_put_set_value_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, size = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item, size);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_get_thread(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(clds_hash_table_node_create(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_set_value(IGNORED_ARG, IGNORED_ARG, &key, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG)).SetReturn(CLDS_HASH_TABLE_SET_VALUE_ERROR);
    STRICT_EXPECTED_CALL(clds_hash_table_node_release(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));


    // act
    result = lru_cache_put(lru_cache, &key, &value, size, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
TEST_FUNCTION(lru_cache_put_insert_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, size = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_get_thread(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(clds_hash_table_node_create(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG))
        .CaptureReturn(&hash_table_item);
    STRICT_EXPECTED_CALL(clds_hash_table_set_value(IGNORED_ARG, IGNORED_ARG, &key, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG)).SetReturn(CLDS_HASH_TABLE_INSERT_ERROR);
    STRICT_EXPECTED_CALL(clds_hash_table_node_release(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    // act
    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_037: [ While the current size of the cache exceeds capacity: ]*/
/*Tests_SRS_LRU_CACHE_13_040: [ lru_cache_put shall acquire the lock in exclusive. ]*/
/*Tests_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
TEST_FUNCTION(lru_cache_put_triggers_eviction_and_fails_with_release_and_callback)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, key2 = 11, size1 = 2, size2 = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_2;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item_1, size1);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    set_lru_put_insert_expectations(&key2, &hash_table_item_2, size2);

    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(DList_IsListEmpty(IGNORED_ARG)).SetReturn(1);
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_on_error(test_error_context));

    // act
    result = lru_cache_put(lru_cache, &key2, &value, size2, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_EVICT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_072: [ lru_cache_put shall decrement the least used node size from current_size. ]*/
TEST_FUNCTION(lru_cache_put_triggers_eviction_triggers_loop_when_size_changed)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, key2 = 11, size1 = 2, size2 = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_2;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item_1, size1);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    set_lru_put_insert_expectations(&key2, &hash_table_item_2, size2);

    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(DList_IsListEmpty(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, capacity - size2, capacity + size2)).SetReturn(100);
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    // retry the loop here
    set_lru_put_evict_expectations(&key, capacity + size2, size1);
    set_lru_put_nothing_to_evict_expectations();

    // act
    result = lru_cache_put(lru_cache, &key2, &value, size2, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_078: [ If clds_hash_table_remove returns CLDS_HASH_TABLE_REMOVE_NOT_FOUND, then lru_cache_put shall retry eviction. ]*/
/*Tests_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
TEST_FUNCTION(lru_cache_put_triggers_eviction_triggers_loop_when_not_found)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, key2 = 11, size1 = 2, size2 = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_2;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item_1, size1);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    set_lru_put_insert_expectations(&key2, &hash_table_item_2, size2);

    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(DList_IsListEmpty(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, capacity - size2, capacity + size2));
    STRICT_EXPECTED_CALL(clds_hash_table_remove(IGNORED_ARG, IGNORED_ARG, &key, IGNORED_ARG, IGNORED_ARG)).SetReturn(CLDS_HASH_TABLE_REMOVE_NOT_FOUND);
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, size1));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    // Retry the loop here
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(DList_IsListEmpty(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, capacity - size2, capacity + size2));
    STRICT_EXPECTED_CALL(clds_hash_table_remove(IGNORED_ARG, IGNORED_ARG, &key, IGNORED_ARG, IGNORED_ARG)).SetReturn(CLDS_HASH_TABLE_REMOVE_ERROR);
    STRICT_EXPECTED_CALL(test_on_error(test_error_context));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, size1));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    // act
    result = lru_cache_put(lru_cache, &key2, &value, size2, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_EVICT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_050: [ For any other errors, lru_cache_put shall return LRU_CACHE_PUT_ERROR ]*/
TEST_FUNCTION(lru_cache_put_triggers_eviction_calls_callback_when_remove_error)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 2;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, key2 = 11, size1 = 2, size2 = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_1;
    CLDS_HASH_TABLE_ITEM* hash_table_item_2;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item_1, size1);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size1, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();

    set_lru_put_insert_expectations(&key2, &hash_table_item_2, size2);

    // Retry the loop here
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(DList_IsListEmpty(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, capacity - size2, capacity + size2));
    STRICT_EXPECTED_CALL(clds_hash_table_remove(IGNORED_ARG, IGNORED_ARG, &key, IGNORED_ARG, IGNORED_ARG)).SetReturn(CLDS_HASH_TABLE_REMOVE_ERROR);
    STRICT_EXPECTED_CALL(test_on_error(test_error_context));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, size1));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    // act
    result = lru_cache_put(lru_cache, &key2, &value, size2, test_eviction_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_EVICT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}


/* lru_cache_get */

/*Tests_SRS_LRU_CACHE_13_051: [ If lru_cache is NULL, then lru_cache_get shall fail and return NULL. ]*/
TEST_FUNCTION(lru_cache_get_with_null_handle_fails)
{
    // arrange
    int key1 = 10;

    // act
    void* get_value_ptr = lru_cache_get(NULL, &key1);

    // assert
    ASSERT_IS_NULL(get_value_ptr);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

/*Tests_SRS_LRU_CACHE_13_052: [ If key is NULL, then lru_cache_get shall fail and return NULL. ]*/
TEST_FUNCTION(lru_cache_get_with_null_key_fails)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    uint32_t bucket_size = 1024;
    int64_t capacity = 2;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    // act
    void* get_value_ptr = lru_cache_get(lru_cache, NULL);

    // assert
    ASSERT_IS_NULL(get_value_ptr);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_053: [ lru_cache_get shall get CLDS_HAZARD_POINTERS_THREAD_HANDLE by calling clds_hazard_pointers_thread_helper_get_thread. ]*/
/*Tests_SRS_LRU_CACHE_13_054: [ lru_cache_get shall check hash table for any existence of the value by calling clds_hash_table_find on the key. ]*/
/*Tests_SRS_LRU_CACHE_13_056: [ lru_cache_get shall acquire the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_055: [ If the key is found and the node from the key is not recently used: ]*/
/*Tests_SRS_LRU_CACHE_13_057: [ lru_cache_get shall remove the old value node from doubly_linked_list by calling DList_RemoveEntryList. ]*/
/*Tests_SRS_LRU_CACHE_13_058: [ lru_cache_get shall make the node as the tail by calling DList_InsertTailList. ]*/
/*Tests_SRS_LRU_CACHE_13_059: [ lru_cache_get shall release the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_060: [ On success, lru_cache_get shall return CLDS_HASH_TABLE_ITEM value of the key. ]*/
TEST_FUNCTION(lru_cache_get_succeeds)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;
    int key1 = 10, key2 = 11, value1 = 1000, value2 = 1001, size = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key1, &hash_table_item, size);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key1, &value1, size, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);

    set_lru_put_insert_expectations(&key2, &hash_table_item, size);
    set_lru_put_nothing_to_evict_expectations();

    result = lru_cache_put(lru_cache, &key2, &value2, size, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();
    set_lru_get_value_expectations(&key1);

    // act
    void* get_value_ptr = lru_cache_get(lru_cache, &key1);
    ASSERT_IS_NOT_NULL(get_value_ptr);

    int get_value = *((int*)get_value_ptr);

    // assert
    ASSERT_ARE_EQUAL(int, get_value, value1);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_053: [ lru_cache_get shall get CLDS_HAZARD_POINTERS_THREAD_HANDLE by calling clds_hazard_pointers_thread_helper_get_thread. ]*/
/*Tests_SRS_LRU_CACHE_13_054: [ lru_cache_get shall check hash table for any existence of the value by calling clds_hash_table_find on the key. ]*/
/*Tests_SRS_LRU_CACHE_13_056: [ lru_cache_get shall acquire the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_059: [ lru_cache_get shall release the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_060: [ On success, lru_cache_get shall return CLDS_HASH_TABLE_ITEM value of the key. ]*/
TEST_FUNCTION(lru_cache_get_does_not_change_order_when_Blink_is_current_key_succeeds)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;
    int key1 = 10, value1 = 1000, size = 1;
    CLDS_HASH_TABLE_ITEM* hash_table_item;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key1, &hash_table_item, size);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key1, &value1, size, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    umock_c_reset_all_calls();
    setup_ignore_hazard_pointers_calls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_get_thread(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_find(IGNORED_ARG, IGNORED_ARG, &key1));
    STRICT_EXPECTED_CALL(test_compute_hash(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_node_release(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    // act
    void* get_value_ptr = lru_cache_get(lru_cache, &key1);
    ASSERT_IS_NOT_NULL(get_value_ptr);

    int get_value = *((int*)get_value_ptr);

    // assert
    ASSERT_ARE_EQUAL(int, get_value, value1);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_053: [ lru_cache_get shall get CLDS_HAZARD_POINTERS_THREAD_HANDLE by calling clds_hazard_pointers_thread_helper_get_thread. ]*/
/*Tests_SRS_LRU_CACHE_13_054: [ lru_cache_get shall check hash table for any existence of the value by calling clds_hash_table_find on the key. ]*/
/*Tests_SRS_LRU_CACHE_13_056: [ lru_cache_get shall acquire the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_055: [ If the key is found and the node from the key is not recently used: ]*/
/*Tests_SRS_LRU_CACHE_13_057: [ lru_cache_get shall remove the old value node from doubly_linked_list by calling DList_RemoveEntryList. ]*/
/*Tests_SRS_LRU_CACHE_13_058: [ lru_cache_get shall make the node as the tail by calling DList_InsertTailList. ]*/
/*Tests_SRS_LRU_CACHE_13_059: [ lru_cache_get shall release the lock in exclusive mode. ]*/
/*Tests_SRS_LRU_CACHE_13_060: [ On success, lru_cache_get shall return CLDS_HASH_TABLE_ITEM value of the key. ]*/
TEST_FUNCTION(lru_cache_get_calls_same_key_multiple_times_succeeds)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;
    int key = 10, value = 1000, size = 1, times = 10;
    CLDS_HASH_TABLE_ITEM* hash_table_item;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    set_lru_put_insert_expectations(&key, &hash_table_item, size);
    set_lru_put_nothing_to_evict_expectations();

    LRU_CACHE_PUT_RESULT result = lru_cache_put(lru_cache, &key, &value, size, test_eviction_callback, NULL);
    ASSERT_ARE_EQUAL(LRU_CACHE_PUT_RESULT, LRU_CACHE_PUT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // act
    for (int i = 0; i < times; i++)
    {
        void* get_value_ptr = lru_cache_get(lru_cache, &key);
        ASSERT_IS_NOT_NULL(get_value_ptr);

        int get_value = *((int*)get_value_ptr);

        // assert
        ASSERT_ARE_EQUAL(int, get_value, value);
    }

    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_061: [ If there are any failures, lru_cache_get shall return NULL. ]*/
TEST_FUNCTION(lru_cache_get_return_NULL_hazard_pointers_return_NULL)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;
    int key = 10;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_get_thread(IGNORED_ARG)).SetReturn(NULL);

    // act
    void* get_value_ptr = lru_cache_get(lru_cache, &key);

    // assert
    ASSERT_IS_NULL(get_value_ptr);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());


    // cleanup
    lru_cache_destroy(lru_cache);
}

/*Tests_SRS_LRU_CACHE_13_053: [ lru_cache_get shall get CLDS_HAZARD_POINTERS_THREAD_HANDLE by calling clds_hazard_pointers_thread_helper_get_thread. ]*/
/*Tests_SRS_LRU_CACHE_13_054: [ lru_cache_get shall check hash table for any existence of the value by calling clds_hash_table_find on the key. ]*/
TEST_FUNCTION(lru_cache_get_return_NULL_when_not_found)
{
    // arrange
    LRU_CACHE_HANDLE lru_cache;
    int64_t capacity = 10;
    uint32_t bucket_size = 1024;
    int key = 10;

    set_lru_create_expectations(bucket_size, test_clds_hazard_pointers);

    lru_cache = lru_cache_create(test_compute_hash, test_key_compare_func, bucket_size, test_clds_hazard_pointers, capacity, test_on_error, test_error_context);
    ASSERT_IS_NOT_NULL(lru_cache);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_thread_helper_get_thread(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hash_table_find(IGNORED_ARG, IGNORED_ARG, &key));
    STRICT_EXPECTED_CALL(test_compute_hash(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));


    // act
    void* get_value_ptr = lru_cache_get(lru_cache, &key);

    // assert
    ASSERT_IS_NULL(get_value_ptr);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());


    // cleanup
    lru_cache_destroy(lru_cache);
}


END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
