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
#include "clds/clds_singly_linked_list.h"
#include "clds/clds_st_hash_set.h"
#include "clds/clds_hazard_pointers.h"

#undef ENABLE_MOCKS

#include "clds/clds_hash_table.h"
#include "../reals/real_clds_st_hash_set.h"
#include "../reals/real_clds_hazard_pointers.h"
#include "../reals/real_clds_singly_linked_list.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;
static TEST_MUTEX_HANDLE g_dllByDll;

TEST_DEFINE_ENUM_TYPE(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);

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

MOCK_FUNCTION_WITH_CODE(, void, test_item_cleanup_func, void*, context, struct CLDS_HASH_TABLE_ITEM_TAG*, item)
MOCK_FUNCTION_END()

MOCK_FUNCTION_WITH_CODE(, uint64_t, test_compute_hash, void*, key)
    (void)key;
MOCK_FUNCTION_END(0)

typedef struct TEST_ITEM_TAG
{
    int dummy;
} TEST_ITEM;

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM)

BEGIN_TEST_SUITE(clds_hash_table_unittests)

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

    REGISTER_CLDS_ST_HASH_SET_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_SINGLY_LINKED_LIST_GLOBAL_MOCK_HOOKS();

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, real_free);

    REGISTER_UMOCK_ALIAS_TYPE(RECLAIM_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTER_RECORD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_SINGLY_LINKED_LIST_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_THREAD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SINGLY_LINKED_LIST_ITEM_CLEANUP_CB, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SINGLY_LINKED_LIST_ITEM_COMPARE_CB, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_HANDLE, void*);

    REGISTER_TYPE(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_RESULT);
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
/* Tests_SRS_CLDS_HASH_TABLE_01_027: [ The hash table shall maintain a list of arrays of buckets, so that it can be resized as needed. ]*/
TEST_FUNCTION(clds_hash_table_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
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
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
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
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
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
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
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
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
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
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
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
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
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

/* clds_hash_table_destroy */

/* Tests_SRS_CLDS_HASH_TABLE_01_006: [ `clds_hash_table_destroy` shall free all resources associated with the hash table instance. ]*/
TEST_FUNCTION(clds_hash_table_destroy_frees_the_hash_table_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
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

/* Tests_SRS_CLDS_HASH_TABLE_01_007: [ If `clds_hash_table` is NULL, `clds_hash_table_destroy` shall return. ]*/
TEST_FUNCTION(clds_hash_table_destroy_with_NULL_hash_table_returns)
{
    // arrange

    // act
    clds_hash_table_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* clds_hash_table_insert */

/* Tests_SRS_CLDS_HASH_TABLE_01_008: [ `clds_hash_table_insert` shall insert a key/value pair in the hash table. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_009: [ On success `clds_hash_table_insert` shall return 0. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_018: [ `clds_hash_table_insert` shall obtain the bucket index to be used by calling `compute_hash` and passing to it the `key` value. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_019: [ If no singly linked list exists at the determined bucket index then a new list shall be created. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_020: [ A new singly linked list item shall be created by calling `clds_singly_linked_list_node_create`. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_021: [ The new singly linked list node shall be inserted in the singly linked list at the identified bucket by calling `clds_singly_linked_list_insert`. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_038: [ `clds_hash_table_insert` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
TEST_FUNCTION(clds_hash_table_insert_inserts_one_key_value_pair)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_SINGLY_LINKED_LIST_HANDLE linked_list;
    int result;
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_create(hazard_pointers))
        .CaptureReturn(&linked_list);
    STRICT_EXPECTED_CALL(clds_singly_linked_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)item))
        .ValidateArgumentValue_clds_singly_linked_list(&linked_list);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_010: [ If `clds_hash_table` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_NULL_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_insert(NULL, hazard_pointers_thread, (void*)0x1, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_012: [ If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_insert(hash_table, NULL, (void*)0x1, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_011: [ If `key` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    int result;
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, NULL, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_022: [ If any error is encountered while inserting the key/value pair, `clds_hash_table_insert` shall fail and return a non-zero value. ]*/
TEST_FUNCTION(when_creating_the_singly_linked_list_fails_clds_hash_table_insert_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_create(hazard_pointers))
        .SetReturn(NULL);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    CLDS_HASH_TABLE_NODE_DESTROY(TEST_ITEM, item);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_022: [ If any error is encountered while inserting the key/value pair, `clds_hash_table_insert` shall fail and return a non-zero value. ]*/
TEST_FUNCTION(when_inserting_the_singly_linked_list_item_fails_clds_hash_table_insert_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_create(hazard_pointers));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)item))
        .SetReturn(1);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_008: [ `clds_hash_table_insert` shall insert a key/value pair in the hash table. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_009: [ On success `clds_hash_table_insert` shall return 0. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_018: [ `clds_hash_table_insert` shall obtain the bucket index to be used by calling `compute_hash` and passing to it the `key` value. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_020: [ A new singly linked list item shall be created by calling `clds_singly_linked_list_node_create`. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_021: [ The new singly linked list node shall be inserted in the singly linked list at the identified bucket by calling `clds_singly_linked_list_insert`. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_038: [ `clds_hash_table_insert` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_2nd_key_on_the_same_bucket_does_not_create_another_list)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x2));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)item_2));

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_008: [ `clds_hash_table_insert` shall insert a key/value pair in the hash table. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_009: [ On success `clds_hash_table_insert` shall return 0. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_018: [ `clds_hash_table_insert` shall obtain the bucket index to be used by calling `compute_hash` and passing to it the `key` value. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_020: [ A new singly linked list item shall be created by calling `clds_singly_linked_list_node_create`. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_021: [ The new singly linked list node shall be inserted in the singly linked list at the identified bucket by calling `clds_singly_linked_list_insert`. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_038: [ `clds_hash_table_insert` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
TEST_FUNCTION(clds_hash_table_insert_with_2nd_key_on_a_different_bucket_creates_another_list)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_SINGLY_LINKED_LIST_HANDLE linked_list;
    int result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x2))
        .SetReturn(1);
    STRICT_EXPECTED_CALL(clds_singly_linked_list_create(hazard_pointers))
        .CaptureReturn(&linked_list);
    STRICT_EXPECTED_CALL(clds_singly_linked_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)item_2))
        .ValidateArgumentValue_clds_singly_linked_list(&linked_list);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

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
    CLDS_SINGLY_LINKED_LIST_HANDLE linked_list;
    int result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_create(hazard_pointers))
        .CaptureReturn(&linked_list);
    STRICT_EXPECTED_CALL(clds_singly_linked_list_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG, (void*)item_2))
        .ValidateArgumentValue_clds_singly_linked_list(&linked_list);

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_hash_table_delete */

/* Tests_SRS_CLDS_HASH_TABLE_01_013: [ `clds_hash_table_delete` shall delete a key from the hash table. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_014: [ On success `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_RESULT_OK`. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_039: [ `clds_hash_table_delete` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
TEST_FUNCTION(clds_hash_table_delete_deletes_the_key)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_delete_if(IGNORED_PTR_ARG, hazard_pointers_thread, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_015: [ If `clds_hash_table` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_RESULT_ERROR`. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_NULL_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    int result;
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_delete(NULL, hazard_pointers_thread, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_017: [ If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_RESULT_ERROR`. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_delete(hash_table, NULL, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_016: [ If `key` is NULL, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_RESULT_ERROR`. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);
    umock_c_reset_all_calls();

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table, `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_RESULT_NOT_FOUND`. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_a_key_that_is_not_in_the_hash_table_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_023: [ If the desired key is not found in the hash table, `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_RESULT_NOT_FOUND`. ]*/
TEST_FUNCTION(clds_hash_table_delete_with_a_key_that_is_already_deleted_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    int result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);
    (void)clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_024: [ If a bucket is identified and the delete of the item from the underlying list fails, `clds_hash_table_delete` shall fail and return `CLDS_HASH_TABLE_DELETE_RESULT_ERROR`. ]*/
TEST_FUNCTION(when_the_underlying_delete_from_list_fails_clds_hash_table_delete_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_DELETE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 2, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_delete_if(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(CLDS_SINGLY_LINKED_LIST_DELETE_ERROR);

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1);

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
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_delete_if(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_delete_if(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1);

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
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x4, item_4);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_delete_if(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_delete_if(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_delete_if(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_033: [ If the key is not found in any of the arrays of buckets, `clds_hash_table_delete` shall return `CLDS_HASH_TABLE_DELETE_RESULT_NOT_FOUND`. ]*/
TEST_FUNCTION(when_item_is_not_found_in_any_bucket_delete_returns_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_DELETE_RESULT result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x3));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_delete_if(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_delete_if(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x3);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_NOT_FOUND, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_hash_table_find */

/* Tests_SRS_CLDS_HASH_TABLE_01_034: [ `clds_hash_table_find` shall find the key identified by `key` in the hash table and on success return the item corresponding to it. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_040: [ `clds_hash_table_find` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_041: [ `clds_hash_table_find` shall look up the key in the biggest array of buckets. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_044: [ Looking up the key in the array of buckets is done by obtaining the list in the bucket correspoding to the hash and looking up the key in the list by calling `clds_singly_linked_list_find`. ]*/
TEST_FUNCTION(clds_hash_table_find_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 3, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)item, (void*)result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_034: [ `clds_hash_table_find` shall find the key identified by `key` in the hash table and on success return the item corresponding to it. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_040: [ `clds_hash_table_find` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
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
    hash_table = clds_hash_table_create(test_compute_hash, 3, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x2));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)item_2, (void*)result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_034: [ `clds_hash_table_find` shall find the key identified by `key` in the hash table and on success return the item corresponding to it. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_01_040: [ `clds_hash_table_find` shall hash the key by calling the `compute_hash` function passed to `clds_hash_table_create`. ]*/
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
    hash_table = clds_hash_table_create(test_compute_hash, 3, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x3));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x3);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)item_3, (void*)result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_035: [ If `clds_hash_table` is NULL, `clds_hash_table_find` shall fail and return NULL. ]*/
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

/* Tests_SRS_CLDS_HASH_TABLE_01_036: [ If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_find` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_find_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);
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

/* Tests_SRS_CLDS_HASH_TABLE_01_037: [ If `key` is NULL, `clds_hash_table_find` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_find_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item);
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
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x1));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)item_1, (void*)result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_01_043: [ If the key is not found at all, `clds_hash_table_find` shall return NULL. ]*/
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
    hash_table = clds_hash_table_create(test_compute_hash, 1, hazard_pointers);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x1, item_1);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x2, item_2);
    (void)clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x3, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    STRICT_EXPECTED_CALL(test_compute_hash((void*)0x4));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(clds_singly_linked_list_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));

    // act
    result = clds_hash_table_find(hash_table, hazard_pointers_thread, (void*)0x4);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(clds_hash_table_unittests)
