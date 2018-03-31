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

#define ENABLE_MOCKS

#include "azure_c_shared_utility/gballoc.h"
#include "clds/clds_singly_linked_list.h"
#include "clds/clds_hazard_pointers.h"

#undef ENABLE_MOCKS

#include "clds/clds_hash_table.h"
#include "../reals/real_clds_hazard_pointers.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;
static TEST_MUTEX_HANDLE g_dllByDll;

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

static void test_reclaim_function(void* node)
{
    (void)node;
}

static uint64_t test_compute_hash(void* key)
{
    (void)key;
    return 0;
}

BEGIN_TEST_SUITE(clds_hash_table_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);

    test_serialize_mutex = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(test_serialize_mutex);

    umock_c_init(on_umock_c_error);

    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, real_free);

    REGISTER_UMOCK_ALIAS_TYPE(RECLAIM_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_HANDLE, void*);
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

/* clds_hash_table_create */

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ `clds_hash_table_create` shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
TEST_FUNCTION(clds_hash_table_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create(test_reclaim_function);
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hash_table);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_002: [ If any error happens, `clds_hash_table_create` shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_for_the_hash_table_fails_clds_hash_table_create_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create(test_reclaim_function);
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG))
        .SetReturn(NULL);

    // act
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_002: [ If any error happens, `clds_hash_table_create` shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_for_the_hash_table_array_fails_clds_hash_table_create_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create(test_reclaim_function);
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG))
        .SetReturn(NULL);
    STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));

    // act
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_003: [ If `compute_hash` is NULL, `clds_hash_table_create` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_compute_hash_function_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create(test_reclaim_function);
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    // act
    hash_table = clds_hash_table_create(NULL, 1, hazard_pointers);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_004: [ If `initial_bucket_size` is 0, `clds_hash_table_create` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_initial_bucket_size_zero_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create(test_reclaim_function);
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    // act
    hash_table = clds_hash_table_create(test_compute_hash, 0, hazard_pointers);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_005: [ If `clds_hazard_pointers` is NULL, `clds_hash_table_create` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_clds_hazard_pointers_fails)
{
    // arrange

    // act
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, 1, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hash_table);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ `clds_hash_table_create` shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
TEST_FUNCTION(two_hash_tables_can_be_created_with_the_same_hazard_pointer_instance)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create(test_reclaim_function);
    CLDS_HASH_TABLE_HANDLE hash_table_1;
    CLDS_HASH_TABLE_HANDLE hash_table_2;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table_1 = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);
    hash_table_2 = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);

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

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ `clds_hash_table_create` shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
TEST_FUNCTION(two_hash_tables_can_be_created_with_different_hazard_pointer_instances)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers_1 = clds_hazard_pointers_create(test_reclaim_function);
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers_2 = clds_hazard_pointers_create(test_reclaim_function);
    CLDS_HASH_TABLE_HANDLE hash_table_1;
    CLDS_HASH_TABLE_HANDLE hash_table_2;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table_1 = clds_hash_table_create(test_compute_hash, 1, hazard_pointers_1);
    hash_table_2 = clds_hash_table_create(test_compute_hash, 1, hazard_pointers_2);

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

/* Tests_SRS_CLDS_HASH_TABLE_01_001: [ `clds_hash_table_create` shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. ]*/
TEST_FUNCTION(clds_hash_table_create_with_initial_size_2_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create(test_reclaim_function);
    CLDS_HASH_TABLE_HANDLE hash_table;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hash_table);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(clds_hash_table_unittests)
