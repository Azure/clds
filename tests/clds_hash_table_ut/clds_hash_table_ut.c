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

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_stdint.h"

#define ENABLE_MOCKS

#include "azure_c_shared_utility/gballoc.h"
#include "clds/clds_sorted_list.h"
#include "clds/clds_st_hash_set.h"
#include "clds/clds_hazard_pointers.h"

#undef ENABLE_MOCKS

#include "clds/clds_hash_table.h"
#include "../reals/real_clds_st_hash_set.h"
#include "../reals/real_clds_hazard_pointers.h"
#include "../reals/real_clds_sorted_list.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;

TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT_VALUES);

TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

static void test_reclaim_function(void* node)
{
    (void)node;
}

MOCK_FUNCTION_WITH_CODE(, void, test_item_cleanup_func, void*, context, struct CLDS_HASH_TABLE_ITEM_TAG*, item)
MOCK_FUNCTION_END()

MOCK_FUNCTION_WITH_CODE(, uint64_t, test_compute_hash, void*, key)
    (void)key;
MOCK_FUNCTION_END(0)

MOCK_FUNCTION_WITH_CODE(, void, test_skipped_seq_no_cb, void*, context, int64_t, skipped_seq_no)
MOCK_FUNCTION_END()

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

BEGIN_TEST_SUITE(clds_hash_table_unittests)

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
    REGISTER_CLDS_SORTED_LIST_GLOBAL_MOCK_HOOKS();

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, real_free);

    REGISTER_UMOCK_ALIAS_TYPE(RECLAIM_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTER_RECORD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_SORTED_LIST_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_THREAD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SORTED_LIST_ITEM_CLEANUP_CB, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SORTED_LIST_GET_ITEM_KEY_CB, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SORTED_LIST_KEY_COMPARE_CB, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SORTED_LIST_SKIPPED_SEQ_NO_CB, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_KEY_COMPARE_FUNC, void*);

    REGISTER_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT);
    REGISTER_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT);
    REGISTER_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT);
    REGISTER_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT);
    REGISTER_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT);
    REGISTER_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT);
    REGISTER_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT);
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

/* clds_hash_table_create */

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ clds_hash_table_create shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_027: [ The hash table shall maintain a list of arrays of buckets, so that it can be resized as needed. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_058: [ start_sequence_number shall be allowed to be NULL, in which case no sequence number computations shall be performed. ]*/
TEST_FUNCTION(clds_hash_table_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    volatile int64_t sequence_number = 55;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hash_table);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ clds_hash_table_create shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_027: [ The hash table shall maintain a list of arrays of buckets, so that it can be resized as needed. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_058: [ start_sequence_number shall be allowed to be NULL, in which case no sequence number computations shall be performed. ]*/
TEST_FUNCTION(clds_hash_table_create_succeeds_with_NULL_skipped_seq_no_cb)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    volatile int64_t sequence_number = 55;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, &sequence_number, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hash_table);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ clds_hash_table_create shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_027: [ The hash table shall maintain a list of arrays of buckets, so that it can be resized as needed. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_058: [ start_sequence_number shall be allowed to be NULL, in which case no sequence number computations shall be performed. ]*/
TEST_FUNCTION(clds_hash_table_create_succeeds_with_NULL_sequence_number_and_skipped_seq_no_cb)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hash_table);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_058: [ start_sequence_number shall be allowed to be NULL, in which case no sequence number computations shall be performed. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_sequence_number_and_non_NULL_skipped_seq_no_cb_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, test_skipped_seq_no_cb, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_002: [ If any error happens, clds_hash_table_create shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_for_the_hash_table_fails_clds_hash_table_create_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG))
        .SetReturn(NULL);

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_002: [ If any error happens, clds_hash_table_create shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_for_the_hash_table_array_fails_clds_hash_table_create_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG))
        .SetReturn(NULL);
    STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_003: [ If compute_hash is NULL, clds_hash_table_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_compute_hash_function_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    // act
    hash_table = clds_hash_table_create(NULL, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_045: [ If key_compare_func is NULL, clds_hash_table_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_key_compare_function_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    // act
    hash_table = clds_hash_table_create(test_compute_hash, NULL, 1, hazard_pointers, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_004: [ If initial_bucket_size is 0, clds_hash_table_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_initial_bucket_size_zero_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 0, hazard_pointers, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_005: [ If clds_hazard_pointers is NULL, clds_hash_table_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_clds_hazard_pointers_fails)
{
    // arrange

    // act
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, NULL, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ clds_hash_table_create shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
TEST_FUNCTION(two_hash_tables_can_be_created_with_the_same_hazard_pointer_instance)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table_1;
    CLDS_HASH_TABLE_HANDLE hash_table_2;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table_1 = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    hash_table_2 = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hash_table_1);
    ASSERT_IS_NOT_NULL(hash_table_2);
    ASSERT_ARE_NOT_EQUAL(void_ptr, hash_table_1, hash_table_2);

    // cleanup
    clds_hash_table_destroy(hash_table_1);
    clds_hash_table_destroy(hash_table_2);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ clds_hash_table_create shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
TEST_FUNCTION(two_hash_tables_can_be_created_with_different_hazard_pointer_instances)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers_1 = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers_2 = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table_1;
    CLDS_HASH_TABLE_HANDLE hash_table_2;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table_1 = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers_1, NULL, NULL, NULL);
    hash_table_2 = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers_2, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hash_table_1);
    ASSERT_IS_NOT_NULL(hash_table_2);
    ASSERT_ARE_NOT_EQUAL(void_ptr, hash_table_1, hash_table_2);

    // cleanup
    clds_hash_table_destroy(hash_table_1);
    clds_hash_table_destroy(hash_table_2);
    clds_hazard_pointers_destroy(hazard_pointers_1);
    clds_hazard_pointers_destroy(hazard_pointers_2);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ clds_hash_table_create shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
TEST_FUNCTION(clds_hash_table_create_with_initial_size_2_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hash_table);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_057: [ start_sequence_number shall be used as the sequence number variable that shall be incremented at every operation that is done on the hash table. ]*/
TEST_FUNCTION(clds_hash_table_create_with_non_NULL_start_sequence_number_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    volatile int64_t sequence_number;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, &sequence_number, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hash_table);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_hash_table_destroy */

/* Tests_SRS_CLDS_HASH_TABLE_01_006: [ clds_hash_table_destroy shall free all resources associated with the hash table instance. ]*/
TEST_FUNCTION(clds_hash_table_destroy_frees_the_hash_table_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_hash_table_destroy(hash_table);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_007: [ If clds_hash_table is NULL, clds_hash_table_destroy shall return. ]*/
TEST_FUNCTION(clds_hash_table_destroy_with_NULL_hash_table_returns)
{
    // arrange

    // act
    clds_hash_table_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* clds_hash_table_insert */

/* Tests_SRS_CLDS_HASH_TABLE_01_009: [ On success clds_hash_table_insert shall return CLDS_HASH_TABLE_INSERT_OK. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_018: [ clds_hash_table_insert shall obtain the bucket index to be used by calling compute_hash and passing to it the key value. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_019: [ If no sorted list exists at the determined bucket index then a new list shall be created. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_020: [ A new sorted list item shall be created by calling clds_sorted_list_node_create. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_021: [ The new sorted list node shall be inserted in the sorted list at the identified bucket by calling clds_sorted_list_insert. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_038: [ clds_hash_table_insert shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_071: [ When a new list is created, the start sequence number passed to clds_hash_tabel_create shall be passed as the start_sequence_number argument. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_059: [ For each insert the order of the operation shall be computed by passing sequence_number to clds_sorted_list_insert. ]*/
TEST_FUNCTION(clds_hash_table_insert_inserts_one_key_value_pair)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_SORTED_LIST_HANDLE linked_list;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_create(hazard_pointers, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, NULL, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CaptureReturn(&linked_list);
    STRICT_EXPECTED_CALL(clds_sorted_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (CLDS_SORTED_LIST_ITEM*)item, NULL))
        .ValidateArgumentValue_clds_sorted_list(&linked_list);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_071: [ When a new list is created, the start sequence number passed to clds_hash_tabel_create shall be passed as the start_sequence_number argument. ]*/
TEST_FUNCTION(clds_hash_table_insert_inserts_one_key_value_pair_with_non_NULL_start_sequence_no)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_SORTED_LIST_HANDLE linked_list;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    volatile int64_t sequence_number = 42;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, &sequence_number, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_create(hazard_pointers, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, &sequence_number, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CaptureReturn(&linked_list);
    STRICT_EXPECTED_CALL(clds_sorted_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (CLDS_SORTED_LIST_ITEM*)item, NULL))
        .ValidateArgumentValue_clds_sorted_list(&linked_list);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_010: [ If clds_hash_table is NULL, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_NULL_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_insert(NULL, hazard_pointers_thread, (void*)0x1, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_ERROR, result);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_012: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_insert(hash_table, NULL, (void*)0x1, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_011: [ If key is NULL, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_INSERT_RESULT result;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, NULL, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_022: [ If any error is encountered while inserting the key/value pair, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
TEST_FUNCTION(when_creating_the_singly_linked_list_fails_clds_hash_table_insert_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_create(hazard_pointers, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(NULL);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_ERROR, result);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_022: [ If any error is encountered while inserting the key/value pair, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
TEST_FUNCTION(when_inserting_the_singly_linked_list_item_fails_clds_hash_table_insert_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_create(hazard_pointers, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_sorted_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (CLDS_SORTED_LIST_ITEM*)item, NULL))
        .SetReturn(CLDS_SORTED_LIST_INSERT_ERROR);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_009: [ On success clds_hash_table_insert shall return CLDS_HASH_TABLE_INSERT_OK. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_018: [ clds_hash_table_insert shall obtain the bucket index to be used by calling compute_hash and passing to it the key value. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_020: [ A new sorted list item shall be created by calling clds_sorted_list_node_create. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_021: [ The new sorted list node shall be inserted in the sorted list at the identified bucket by calling clds_sorted_list_insert. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_038: [ clds_hash_table_insert shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_2nd_key_on_the_same_bucket_does_not_create_another_list)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    uint64_t first_hash;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    umock_c_reset_all_calls();
    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x2))
        .CaptureReturn(&first_hash);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x2))
        .SetReturn(first_hash);
    STRICT_EXPECTED_CALL(clds_sorted_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (CLDS_SORTED_LIST_ITEM*)item_2, NULL));

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_009: [ On success clds_hash_table_insert shall return CLDS_HASH_TABLE_INSERT_OK. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_018: [ clds_hash_table_insert shall obtain the bucket index to be used by calling compute_hash and passing to it the key value. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_020: [ A new sorted list item shall be created by calling clds_sorted_list_node_create. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_021: [ The new sorted list node shall be inserted in the sorted list at the identified bucket by calling clds_sorted_list_insert. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_038: [ clds_hash_table_insert shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_2nd_key_on_a_different_bucket_creates_another_list)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_SORTED_LIST_HANDLE linked_list;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x2))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(clds_sorted_list_create(hazard_pointers, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CaptureReturn(&linked_list);
    STRICT_EXPECTED_CALL(clds_sorted_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (CLDS_SORTED_LIST_ITEM*)item_2, NULL))
        .ValidateArgumentValue_clds_sorted_list(&linked_list);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_030: [ If the number of items in the list reaches the number of buckets, the number of buckets shall be doubled. ]*/
TEST_FUNCTION(clds_hash_table_insert_when_the_number_of_items_reaches_number_of_buckets_allocates_another_bucket_array)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_SORTED_LIST_HANDLE linked_list;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x2));
    STRICT_EXPECTED_CALL(clds_sorted_list_find_key(IGNORED_PTR_ARG, hazard_pointers_thread, (void*)0x2));
    STRICT_EXPECTED_CALL(clds_sorted_list_create(hazard_pointers, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CaptureReturn(&linked_list);
    STRICT_EXPECTED_CALL(clds_sorted_list_insert(IGNORED_PTR_ARG, hazard_pointers_thread, (CLDS_SORTED_LIST_ITEM*)item_2, NULL))
        .ValidateArgumentValue_clds_sorted_list(&linked_list);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_046: [ If the key already exists in the hash table, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ALREADY_EXISTS. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_the_same_key_2_times_returns_KEY_ALREADY_EXISTS)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    umock_c_reset_all_calls();
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (CLDS_SORTED_LIST_ITEM*)item_2, NULL));

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS, result);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item_2);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_059: [ For each insert the order of the operation shall be computed by passing sequence_number to clds_sorted_list_insert. ]*/
TEST_FUNCTION(clds_hash_table_insert_passes_the_sequence_number_to_the_sorted_list_insert)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_SORTED_LIST_HANDLE linked_list;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    volatile int64_t sequence_number = 42;
    int64_t insert_seq_no = 0;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, &sequence_number, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_create(hazard_pointers, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, &sequence_number, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CaptureReturn(&linked_list);
    STRICT_EXPECTED_CALL(clds_sorted_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (CLDS_SORTED_LIST_ITEM*)item, &insert_seq_no))
        .ValidateArgumentValue_clds_sorted_list(&linked_list);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);
    ASSERT_ARE_EQUAL(int64_t, 43, insert_seq_no);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_062: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_hash_table_create, clds_hash_table_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_non_NULL_sequence_no_but_NULL_start_sequence_no_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_INSERT_RESULT result;
    int64_t insert_seq_no = 0;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_ERROR, result);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_hash_table_delete */

/* Tests_SRS_CLDS_HASH_TABLE_01_014: [ On success clds_hash_table_delete shall return CLDS_HASH_TABLE_DELETE_OK. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_039: [ clds_hash_table_delete shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_063: [ For each delete the order of the operation shall be computed by passing sequence_number to clds_sorted_list_delete_key. ]*/
TEST_FUNCTION(clds_hash_table_delete_deletes_the_key)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, hazard_pointers_thread, (void*)0x1, NULL));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_015: [ If clds_hash_table is NULL, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_NULL_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    int result;
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_delete(NULL, hazard_pointers_thread, (void*)0x1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_017: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_delete(hash_table, NULL, (void*)0x1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_016: [ If key is NULL, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_delete shall return CLDS_HASH_TABLE_DELETE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_a_key_that_is_not_in_the_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_delete shall return CLDS_HASH_TABLE_DELETE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_a_key_that_is_already_deleted_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    (void)clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_024: [ If a bucket is identified and the delete of the item from the underlying list fails, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
TEST_FUNCTION(when_the_underlying_delete_from_list_fails_clds_hash_table_delete_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_DELETE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, NULL))
        .SetReturn(CLDS_SORTED_LIST_DELETE_ERROR);

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_025: [ If the element to be deleted is not found in the biggest array of buckets, then it shall be looked up in the next available array of buckets. ]*/
TEST_FUNCTION(delete_looks_in_2_buckets)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_DELETE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, NULL));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, NULL));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_025: [ If the element to be deleted is not found in the biggest array of buckets, then it shall be looked up in the next available array of buckets. ]*/
TEST_FUNCTION(delete_looks_in_3_buckets)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_DELETE_RESULT result;
    // 1st bucket array has 1 bucket
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    // 2nd bucket array has 2 buckets
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    // 3rd bucket array has 4 buckets
    CLDS_HASH_TABLE_ITEM* item_4 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x4, item_4, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, NULL));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, NULL));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, NULL));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_delete shall return CLDS_HASH_TABLE_DELETE_NOT_FOUND. ]*/
TEST_FUNCTION(when_item_is_not_found_in_any_bucket_delete_returns_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_DELETE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x3));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x3, NULL));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x3, NULL));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x3, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_NOT_FOUND, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_063: [ For each delete the order of the operation shall be computed by passing sequence_number to clds_sorted_list_delete_key. ]*/
TEST_FUNCTION(clds_hash_table_delete_deletes_the_key_and_stamps_the_sequence_no)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    volatile int64_t sequence_number = 42;
    int64_t delete_seq_no = 0;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, &sequence_number, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_delete_key(IGNORED_PTR_ARG, hazard_pointers_thread, (void*)0x1, &delete_seq_no));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, &delete_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(int64_t, 44, delete_seq_no);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_066: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_hash_table_create, clds_hash_table_delete shall fail and return CLDS_HASH_TABLE_DELETE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_non_NULL_sequence_no_but_NULL_start_sequence_number_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int64_t delete_seq_no = 0;
    CLDS_HASH_TABLE_DELETE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, &delete_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_hash_table_remove */

/* Tests_SRS_CLDS_HASH_TABLE_01_047: [ clds_hash_table_remove shall remove a key from the hash table and return a pointer to the item to the user. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_049: [ On success clds_hash_table_remove shall return CLDS_HASH_TABLE_REMOVE_OK. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_048: [ clds_hash_table_remove shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_067: [ For each remove the order of the operation shall be computed by passing sequence_number to clds_sorted_list_remove_key. ]*/
TEST_FUNCTION(clds_hash_table_remove_removes_the_key_from_the_list)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, hazard_pointers_thread, (void*)0x1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x1, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_067: [ For each remove the order of the operation shall be computed by passing sequence_number to clds_sorted_list_remove_key. ]*/
TEST_FUNCTION(clds_hash_table_remove_removes_the_key_from_the_list_with_non_NULL_sequence_no)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    volatile int64_t sequence_number = 42;
    int64_t remove_seq_no;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, &sequence_number, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, hazard_pointers_thread, (void*)0x1, IGNORED_PTR_ARG, &remove_seq_no));

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x1, &removed_item, &remove_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_OK, result);
    ASSERT_ARE_EQUAL(int64_t, 44, remove_seq_no);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_050: [ If clds_hash_table is NULL, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_remove_with_NULL_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* removed_item;
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_remove(NULL, hazard_pointers_thread, (void*)0x1, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_ERROR, result);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_052: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_remove_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_remove(hash_table, NULL, (void*)0x1, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_051: [ If key is NULL, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_remove_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, NULL, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_056: [ If item is NULL, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_remove_with_NULL_item_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x1, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_053: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_remove shall return CLDS_HASH_TABLE_REMOVE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_hash_table_remove_with_a_key_that_is_not_in_the_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* removed_item;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x1, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_NOT_FOUND, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_053: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_remove shall return CLDS_HASH_TABLE_REMOVE_NOT_FOUND. ]*/
TEST_FUNCTION(clds_hash_table_remove_with_a_key_that_is_already_deleted_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    (void)clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x1, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_NOT_FOUND, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_054: [ If a bucket is identified and the delete of the item from the underlying list fails, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
TEST_FUNCTION(when_the_underlying_delete_from_list_fails_clds_hash_table_remove_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(CLDS_SORTED_LIST_REMOVE_ERROR);

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x1, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_055: [ If the element to be deleted is not found in the biggest array of buckets, then it shall be looked up in the next available array of buckets. ]*/
TEST_FUNCTION(remove_looks_in_2_buckets)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x1, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_055: [ If the element to be deleted is not found in the biggest array of buckets, then it shall be looked up in the next available array of buckets. ]*/
TEST_FUNCTION(remove_looks_in_3_buckets)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    // 1st bucket array has 1 bucket
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    // 2nd bucket array has 2 buckets
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    // 3rd bucket array has 4 buckets
    CLDS_HASH_TABLE_ITEM* item_4 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x4, item_4, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x1, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_053: [ If the desired key is not found in the hash table (not found in any of the arrays of buckets), clds_hash_table_remove shall return CLDS_HASH_TABLE_REMOVE_NOT_FOUND. ]*/
TEST_FUNCTION(when_item_is_not_found_in_any_bucket_remove_returns_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x3));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x3, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_sorted_list_remove_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x3, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x3, &removed_item, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_NOT_FOUND, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_070: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_hash_table_create, clds_hash_table_remove shall fail and return CLDS_HASH_TABLE_REMOVE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_remove_with_non_NULL_sequence_no_and_NULL_start_sequence_no_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_REMOVE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* removed_item;
    int64_t remove_seq_no;
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 2, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_remove(hash_table, hazard_pointers_thread, (void*)0x1, &removed_item, &remove_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_hash_table_find */

/* Tests_SRS_CLDS_HASH_TABLE_01_034: [ clds_hash_table_find shall find the key identified by key in the hash table and on success return the item corresponding to it. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_040: [ clds_hash_table_find shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_041: [ clds_hash_table_find shall look up the key in the biggest array of buckets. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_044: [ Looking up the key in the array of buckets is done by obtaining the list in the bucket correspoding to the hash and looking up the key in the list by calling clds_sorted_list_find. ]*/
TEST_FUNCTION(clds_hash_table_find_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 3, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_find_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)item, (void*)result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, result);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_034: [ clds_hash_table_find shall find the key identified by key in the hash table and on success return the item corresponding to it. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_040: [ clds_hash_table_find shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
TEST_FUNCTION(clds_hash_table_find_2nd_item_out_of_3_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 3, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x2));
    STRICT_EXPECTED_CALL(clds_sorted_list_find_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x2));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)item_2, (void*)result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, result);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_034: [ clds_hash_table_find shall find the key identified by key in the hash table and on success return the item corresponding to it. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_040: [ clds_hash_table_find shall hash the key by calling the compute_hash function passed to clds_hash_table_create. ]*/
TEST_FUNCTION(clds_hash_table_find_3rd_item_out_of_3_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 3, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x3));
    STRICT_EXPECTED_CALL(clds_sorted_list_find_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x3));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x3);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)item_3, (void*)result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, result);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_035: [ If clds_hash_table is NULL, clds_hash_table_find shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_find_with_NULL_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_ITEM* result;
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_find(NULL, hazard_pointers_thread, (void*)0x3);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_036: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_find shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_find_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_find(hash_table, NULL, (void*)0x3);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_037: [ If key is NULL, clds_hash_table_find shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_find_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_042: [ If the key is not found in the biggest array of buckets, the next bucket arrays shall be looked up. ]*/
TEST_FUNCTION(clds_hash_table_find_looks_up_in_the_2nd_array_of_buckets)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_find_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1));
    STRICT_EXPECTED_CALL(clds_sorted_list_find_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x1));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)item_1, (void*)result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, result);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_043: [ If the key is not found at all, clds_hash_table_find shall return NULL. ]*/
TEST_FUNCTION(when_key_is_not_found_in_any_bucket_levels_clds_hash_table_find_yields_NULL)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2, NULL);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3, NULL);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x4));
    STRICT_EXPECTED_CALL(clds_sorted_list_find_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x4));
    STRICT_EXPECTED_CALL(clds_sorted_list_find_key(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)0x4));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x4);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_hash_table_set_value */

/* Tests_SRS_CLDS_HASH_TABLE_01_079: [ If clds_hash_table is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_set_value_with_NULL_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* old_item;
    CLDS_HASH_TABLE_SET_VALUE_RESULT result;
    int64_t set_value_seq_no;
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_set_value(NULL, hazard_pointers_thread, (void*)0x4, item_1, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_ERROR, result);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item_1);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_080: [ If clds_hazard_pointers_thread is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_set_value_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* old_item;
    CLDS_HASH_TABLE_SET_VALUE_RESULT result;
    int64_t set_value_seq_no;
    int64_t sequence_number = 42;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, &sequence_number, NULL, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_set_value(hash_table, NULL, (void*)0x4, item_1, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item_1);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_081: [ If key is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_set_value_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* old_item;
    CLDS_HASH_TABLE_SET_VALUE_RESULT result;
    int64_t set_value_seq_no;
    int64_t sequence_number = 42;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, &sequence_number, NULL, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, NULL, item_1, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item_1);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_082: [ If new_item is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_set_value_with_NULL_new_item_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_ITEM* old_item;
    CLDS_HASH_TABLE_SET_VALUE_RESULT result;
    int64_t set_value_seq_no;
    int64_t sequence_number = 42;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, &sequence_number, NULL, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)0x4, NULL, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_083: [ If old_item is NULL, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_set_value_with_NULL_old_item_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_SET_VALUE_RESULT result;
    int64_t set_value_seq_no;
    int64_t sequence_number = 42;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, &sequence_number, NULL, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)0x4, item_1, NULL, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item_1);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_084: [ If the sequence_number argument is non-NULL, but no start sequence number was specified in clds_hash_table_create, clds_hash_table_set_value shall fail and return CLDS_HASH_TABLE_SET_VALUE_ERROR. ]*/
TEST_FUNCTION(clds_hash_table_set_value_with_non_NULL_sequence_number_when_a_starting_sequence_number_was_not_specified)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* old_item;
    CLDS_HASH_TABLE_SET_VALUE_RESULT result;
    int64_t set_value_seq_no;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare_func, 1, hazard_pointers, NULL, NULL, NULL);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)0x4, item_1, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_ERROR, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item_1);
}

END_TEST_SUITE(clds_hash_table_unittests)
