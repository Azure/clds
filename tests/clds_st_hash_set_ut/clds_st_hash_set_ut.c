// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
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

#undef ENABLE_MOCKS

#include "clds/clds_st_hash_set.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;

TEST_DEFINE_ENUM_TYPE(CLDS_ST_HASH_SET_INSERT_RESULT, CLDS_ST_HASH_SET_INSERT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_ST_HASH_SET_INSERT_RESULT, CLDS_ST_HASH_SET_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_ST_HASH_SET_FIND_RESULT, CLDS_ST_HASH_SET_FIND_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_ST_HASH_SET_FIND_RESULT, CLDS_ST_HASH_SET_FIND_RESULT_VALUES);

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

MOCK_FUNCTION_WITH_CODE(, uint64_t, test_compute_hash, void*, key)
    (void)key;
MOCK_FUNCTION_END(0)

MOCK_FUNCTION_WITH_CODE(, int, test_key_compare, void*, key1, void*, key2)
    (void)key1;
    (void)key2;
MOCK_FUNCTION_END(0)

BEGIN_TEST_SUITE(clds_st_hash_set_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    test_serialize_mutex = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(test_serialize_mutex);

    result = umock_c_init(on_umock_c_error);
    ASSERT_ARE_EQUAL(int, 0, result, "umock_c_init failed");

    result = umocktypes_stdint_register_types();
    ASSERT_ARE_EQUAL(int, 0, result, "umocktypes_stdint_register_types failed");

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, real_free);

    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_HANDLE, void*);

    REGISTER_TYPE(CLDS_ST_HASH_SET_INSERT_RESULT, CLDS_ST_HASH_SET_INSERT_RESULT);
    REGISTER_TYPE(CLDS_ST_HASH_SET_FIND_RESULT, CLDS_ST_HASH_SET_FIND_RESULT);
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

/* Tests_SRS_CLDS_ST_HASH_SET_01_001: [ clds_st_hash_set_create shall create a new hash set object and on success it shall return a non-NULL handle to the newly created hash set. ]*/
/* Tests_SRS_CLDS_ST_HASH_SET_01_022: [ clds_st_hash_set_create shall allocate memory for the array of buckets used to store the hash set data. ]*/
TEST_FUNCTION(clds_st_hash_set_create_succeeds)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set;

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    st_hash_set = clds_st_hash_set_create(test_compute_hash, test_key_compare, 1024);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(st_hash_set);

    // cleanup
    clds_st_hash_set_destroy(st_hash_set);
}

/* Tests_SRS_CLDS_ST_HASH_SET_01_001: [ clds_st_hash_set_create shall create a new hash set object and on success it shall return a non-NULL handle to the newly created hash set. ]*/
/* Tests_SRS_CLDS_ST_HASH_SET_01_022: [ clds_st_hash_set_create shall allocate memory for the array of buckets used to store the hash set data. ]*/
TEST_FUNCTION(clds_st_hash_set_create_with_bucket_size_1_succeeds)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set;

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    st_hash_set = clds_st_hash_set_create(test_compute_hash, test_key_compare, 1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(st_hash_set);

    // cleanup
    clds_st_hash_set_destroy(st_hash_set);
}

/* Tests_SRS_CLDS_ST_HASH_SET_01_003: [ If compute_hash_func is NULL, clds_st_hash_set_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_st_hash_set_create_with_NULL_compute_hash_func_fails)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set;
    umock_c_reset_all_calls();

    // act
    st_hash_set = clds_st_hash_set_create(NULL, test_key_compare, 1024);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(st_hash_set);
}

/* Tests_SRS_CLDS_ST_HASH_SET_01_004: [ If key_compare_func is NULL, clds_st_hash_set_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_st_hash_set_create_with_NULL_key_compare_func_fails)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set;

    // act
    st_hash_set = clds_st_hash_set_create(NULL, test_key_compare, 1024);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(st_hash_set);
}

/* Tests_SRS_CLDS_ST_HASH_SET_01_005: [ If bucket_size is 0, clds_st_hash_set_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_st_hash_set_create_with_0_bucket_size_fails)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set;
    umock_c_reset_all_calls();

    // act
    st_hash_set = clds_st_hash_set_create(test_compute_hash, test_key_compare, 0);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(st_hash_set);
}

/* Tests_SRS_CLDS_ST_HASH_SET_01_002: [ If any error happens, clds_st_hash_set_create shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_for_the_hash_set_fails_clds_st_hash_set_create_also_fails)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set;

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG))
        .SetReturn(NULL);

    // act
    st_hash_set = clds_st_hash_set_create(test_compute_hash, test_key_compare, 1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(st_hash_set);
}

/* Tests_SRS_CLDS_ST_HASH_SET_01_002: [ If any error happens, clds_st_hash_set_create shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_for_the_bucket_array_fails_clds_st_hash_set_create_also_fails)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set;

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG))
        .SetReturn(NULL);
    STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));

    // act
    st_hash_set = clds_st_hash_set_create(test_compute_hash, test_key_compare, 1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(st_hash_set);
}

/* clds_st_hash_set_destroy */

/* Tests_SRS_CLDS_ST_HASH_SET_01_006: [ clds_st_hash_set_destroy shall free all resources associated with the hash set instance. ]*/
TEST_FUNCTION(clds_st_hash_set_destroy_frees_the_memory)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set = clds_st_hash_set_create(test_compute_hash, test_key_compare, 1024);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));

    // act
    clds_st_hash_set_destroy(st_hash_set);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_CLDS_ST_HASH_SET_01_007: [ If clds_st_hash_set is NULL, clds_st_hash_set_destroy shall return. ]*/
TEST_FUNCTION(clds_st_hash_set_destroy_with_NULL_handle_returns)
{
    // arrange

    // act
    clds_st_hash_set_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* clds_st_hash_set_insert */

/* Tests_SRS_CLDS_ST_HASH_SET_01_008: [ clds_st_hash_set_insert shall insert a key in the hash set. ]*/
/* Tests_SRS_CLDS_ST_HASH_SET_01_009: [ On success clds_st_hash_set_insert shall return CLDS_HASH_TABLE_INSERT_OK. ]*/
/* Tests_SRS_CLDS_ST_HASH_SET_01_012: [ clds_st_hash_set_insert shall hash the key by calling the compute_hash function passed to clds_st_hash_set_create. ]*/
TEST_FUNCTION(clds_st_hash_set_insert_succeeds)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set = clds_st_hash_set_create(test_compute_hash, test_key_compare, 1024);
    CLDS_ST_HASH_SET_INSERT_RESULT result;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x42));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    result = clds_st_hash_set_insert(st_hash_set, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_ST_HASH_SET_INSERT_RESULT, CLDS_ST_HASH_SET_INSERT_OK, result);

    // cleanup
    clds_st_hash_set_destroy(st_hash_set);
}

/* Tests_SRS_CLDS_ST_HASH_SET_01_010: [ If clds_st_hash_set is NULL, clds_st_hash_set_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_st_hash_set_insert_with_NULL_st_hash_set_fails)
{
    // arrange
    CLDS_ST_HASH_SET_INSERT_RESULT result;

    // act
    result = clds_st_hash_set_insert(NULL, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_ST_HASH_SET_INSERT_RESULT, CLDS_ST_HASH_SET_INSERT_ERROR, result);
}

/* Tests_SRS_CLDS_ST_HASH_SET_01_011: [ If key is NULL, clds_st_hash_set_insert shall fail and return CLDS_HASH_TABLE_INSERT_ERROR. ]*/
TEST_FUNCTION(clds_st_hash_set_insert_with_NULL_key_fails)
{
    // arrange
    CLDS_ST_HASH_SET_HANDLE st_hash_set = clds_st_hash_set_create(test_compute_hash, test_key_compare, 1024);
    CLDS_ST_HASH_SET_INSERT_RESULT result;
    umock_c_reset_all_calls();

    // act
    result = clds_st_hash_set_insert(st_hash_set, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_ST_HASH_SET_INSERT_RESULT, CLDS_ST_HASH_SET_INSERT_ERROR, result);

    // cleanup
    clds_st_hash_set_destroy(st_hash_set);
}

END_TEST_SUITE(clds_st_hash_set_unittests)
