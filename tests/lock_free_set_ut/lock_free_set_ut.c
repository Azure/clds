// Copyright (c) Microsoft. All rights reserved.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdbool.h>
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
#include "umock_c/umocktypes_bool.h"
#include "clds/lock_free_set.h"

#define ENABLE_MOCKS

#include "azure_c_pal/gballoc.h"

#undef ENABLE_MOCKS

static TEST_MUTEX_HANDLE test_serialize_mutex;

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

MOCK_FUNCTION_WITH_CODE(, void, test_node_cleanup, void*, context, struct LOCK_FREE_SET_ITEM_TAG*, item)
MOCK_FUNCTION_END()

BEGIN_TEST_SUITE(lock_free_set_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    test_serialize_mutex = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(test_serialize_mutex);

    umock_c_init(on_umock_c_error);

    result = umocktypes_bool_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, real_free);
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

/* lock_free_set_create */

/* Tests_SRS_LOCK_FREE_SET_01_001: [ lock_free_set_create shall create a lock free set. ] */
/* Tests_SRS_LOCK_FREE_SET_01_002: [ On success, lock_free_set_create shall return a non-NULL handle to the newly created set. ]*/
TEST_FUNCTION(lock_free_set_create_succeeds)
{
    // arrange
    LOCK_FREE_SET_HANDLE set;
    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_ARG));

    // act
    set = lock_free_set_create();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(set);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_003: [ If lock_free_set_create fails allocating memory for the set then it shall fail and return NULL. ] */
TEST_FUNCTION(when_allocating_memory_for_the_set_fails_lock_free_set_create_fails)
{
    // arrange
    LOCK_FREE_SET_HANDLE set;
    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_ARG))
        .SetReturn(NULL);

    // act
    set = lock_free_set_create();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(set);
}

/* Tests_SRS_LOCK_FREE_SET_01_001: [ lock_free_set_create shall create a lock free set. ] */
/* Tests_SRS_LOCK_FREE_SET_01_002: [ On success, lock_free_set_create shall return a non-NULL handle to the newly created set. ]*/
TEST_FUNCTION(two_lock_free_sets_can_be_created)
{
    // arrange
    LOCK_FREE_SET_HANDLE set1;
    LOCK_FREE_SET_HANDLE set2;

    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_ARG));

    set1 = lock_free_set_create();

    // act
    set2 = lock_free_set_create();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(set1);
    ASSERT_IS_NOT_NULL(set2);
    ASSERT_ARE_NOT_EQUAL(void_ptr, set1, set2);

    // cleanup
    lock_free_set_destroy(set1, NULL, NULL);
    lock_free_set_destroy(set2, NULL, NULL);
}

/* lock_free_set_destroy */

/* Tests_SRS_LOCK_FREE_SET_01_004: [ lock_free_set_destroy shall free all resources associated with the lock free set. ]*/
TEST_FUNCTION(lock_free_set_destroy_frees_the_memory_for_the_set)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_ARG));

    // act
    lock_free_set_destroy(set, (NODE_CLEANUP_FUNC)test_node_cleanup, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_LOCK_FREE_SET_01_005: [ If lock_free_set is NULL, lock_free_set_destroy shall do nothing. ]*/
TEST_FUNCTION(lock_free_set_destroy_with_NULL_handle_frees_nothing)
{
    // arrange

    // act
    lock_free_set_destroy(NULL, (NODE_CLEANUP_FUNC)test_node_cleanup, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_LOCK_FREE_SET_01_006: [ node_cleanup_callback and context shall be allowed to be NULL. ]*/
TEST_FUNCTION(lock_free_set_destroy_with_NULL_node_cleanup_callback_still_frees_set_memory)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_ARG));

    // act
    lock_free_set_destroy(set, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_LOCK_FREE_SET_01_007: [ If node_cleanup_callback is non-NULL, node_cleanup_callback shall be called for each item that exists in the set at the time lock_free_set_destroy is called. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_008: [ When node_cleanup_callback is called the set item pointer and context shall be passed as arguments. ]*/
TEST_FUNCTION(lock_free_set_destroy_with_1_item_calls_the_node_free_callback)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[1];
    (void)lock_free_set_insert(set, &items[0]);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_node_cleanup((void*)0x4242, (struct LOCK_FREE_SET_ITEM_TAG*)&items[0]));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_ARG));

    // act
    lock_free_set_destroy(set, (NODE_CLEANUP_FUNC)test_node_cleanup, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_LOCK_FREE_SET_01_007: [ If node_cleanup_callback is non-NULL, node_cleanup_callback shall be called for each item that exists in the set at the time lock_free_set_destroy is called. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_008: [ When node_cleanup_callback is called the set item pointer and context shall be passed as arguments. ]*/
TEST_FUNCTION(lock_free_set_destroy_with_2_item_calls_the_node_free_callback)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[2];
    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_node_cleanup((void*)0x4242, (struct LOCK_FREE_SET_ITEM_TAG*)&items[1]));
    STRICT_EXPECTED_CALL(test_node_cleanup((void*)0x4242, (struct LOCK_FREE_SET_ITEM_TAG*)&items[0]));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_ARG));

    // act
    lock_free_set_destroy(set, (NODE_CLEANUP_FUNC)test_node_cleanup, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_LOCK_FREE_SET_01_007: [ If node_cleanup_callback is non-NULL, node_cleanup_callback shall be called for each item that exists in the set at the time lock_free_set_destroy is called. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_006: [ node_cleanup_callback and context shall be allowed to be NULL. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_008: [ When node_cleanup_callback is called the set item pointer and context shall be passed as arguments. ]*/
TEST_FUNCTION(lock_free_set_destroy_with_1_item_calls_the_node_free_callback_NULL_context)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[1];
    (void)lock_free_set_insert(set, &items[0]);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_node_cleanup(NULL, (struct LOCK_FREE_SET_ITEM_TAG*)&items[0]));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_ARG));

    // act
    lock_free_set_destroy(set, (NODE_CLEANUP_FUNC)test_node_cleanup, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* lock_free_set_insert */

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_inserts_the_item)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM item1;
    int result;
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, &item1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_011: [ If lock_free_set or item is NULL, lock_free_set_insert shall fail and return a non-zero value. ]*/
TEST_FUNCTION(lock_free_set_insert_with_NULL_set_fails)
{
    // arrange
    int result;
    LOCK_FREE_SET_ITEM item1;

    // act
    result = lock_free_set_insert(NULL, &item1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_LOCK_FREE_SET_01_011: [ If lock_free_set or item is NULL, lock_free_set_insert shall fail and return a non-zero value. ]*/
TEST_FUNCTION(lock_free_set_insert_with_NULL_item_fails)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    int result;
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_2nd_item_left)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[2];
    int result;
    (void)lock_free_set_insert(set, &items[1]);
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, &items[0]);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_2nd_item_right)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[2];
    int result;
    (void)lock_free_set_insert(set, &items[0]);
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, &items[1]);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_3rd_item_left_left)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result;
    (void)lock_free_set_insert(set, &items[2]);
    (void)lock_free_set_insert(set, &items[1]);
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, &items[0]);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_3rd_item_right_right)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result;
    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, &items[2]);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_3rd_item_left_right)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result;
    (void)lock_free_set_insert(set, &items[2]);
    (void)lock_free_set_insert(set, &items[0]);
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, &items[1]);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_3rd_item_right_left)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result;
    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[2]);
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, &items[1]);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_3_items_filling_both_left_and_right_left_first)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result;
    (void)lock_free_set_insert(set, &items[1]);
    (void)lock_free_set_insert(set, &items[0]);
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, &items[2]);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_3_items_filling_both_left_and_right_right_first)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result;
    (void)lock_free_set_insert(set, &items[1]);
    (void)lock_free_set_insert(set, &items[2]);
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_insert(set, &items[0]);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_256_items_right_only)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[256];
    int result;
    size_t i;
    umock_c_reset_all_calls();

    for (i = 0; i < 256; i++)
    {
        // act
        result = lock_free_set_insert(set, &items[i]);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
    }

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_009: [ lock_free_set_insert shall insert the item item in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_010: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_insert_256_items_left_only)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[256];
    int result;
    size_t i;
    umock_c_reset_all_calls();

    for (i = 0; i < 256; i++)
    {
        // act
        result = lock_free_set_insert(set, &items[255 - i]);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
    }

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* lock_free_set_remove */

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_the_head)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[1];
    int result;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);

    // act
    result = lock_free_set_remove(set, &items[0]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_017: [ If lock_free_set or item is NULL, lock_free_set_remove shall fail and return a non-zero value. ]*/
TEST_FUNCTION(lock_free_set_remove_with_NULL_item_fails)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    int result;
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_remove(set, NULL);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_017: [ If lock_free_set or item is NULL, lock_free_set_remove shall fail and return a non-zero value. ]*/
TEST_FUNCTION(lock_free_set_remove_with_NULL_set_fails)
{
    // arrange
    LOCK_FREE_SET_ITEM items[1];
    int result;
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_remove(NULL, &items[0]);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_the_first_item)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[2];
    int result;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);

    // act
    result = lock_free_set_remove(set, &items[0]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_the_second_item)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[2];
    int result;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);

    // act
    result = lock_free_set_remove(set, &items[1]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_both_items_insert_0_1_remove_0_1)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[2];
    int result1;
    int result2;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);
    

    // act
    result1 = lock_free_set_remove(set, &items[0]);
    result2 = lock_free_set_remove(set, &items[1]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result1);
    ASSERT_ARE_EQUAL(int, 0, result2);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_both_items_insert_1_0_remove_1_0)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[2];
    int result1;
    int result2;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);


    // act
    result1 = lock_free_set_remove(set, &items[1]);
    result2 = lock_free_set_remove(set, &items[0]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result1);
    ASSERT_ARE_EQUAL(int, 0, result2);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_all_3_items_insert_0_1_2_remove_0_1_2)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result1;
    int result2;
    int result3;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);
    (void)lock_free_set_insert(set, &items[2]);


    // act
    result1 = lock_free_set_remove(set, &items[0]);
    result2 = lock_free_set_remove(set, &items[1]);
    result3 = lock_free_set_remove(set, &items[2]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result1);
    ASSERT_ARE_EQUAL(int, 0, result2);
    ASSERT_ARE_EQUAL(int, 0, result3);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_all_3_items_insert_0_1_2_remove_0_2_1)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result1;
    int result2;
    int result3;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);
    (void)lock_free_set_insert(set, &items[2]);


    // act
    result1 = lock_free_set_remove(set, &items[0]);
    result2 = lock_free_set_remove(set, &items[2]);
    result3 = lock_free_set_remove(set, &items[1]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result1);
    ASSERT_ARE_EQUAL(int, 0, result2);
    ASSERT_ARE_EQUAL(int, 0, result3);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_all_3_items_insert_0_1_2_remove_1_0_2)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result1;
    int result2;
    int result3;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);
    (void)lock_free_set_insert(set, &items[2]);

    // act
    result1 = lock_free_set_remove(set, &items[1]);
    result2 = lock_free_set_remove(set, &items[0]);
    result3 = lock_free_set_remove(set, &items[2]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result1);
    ASSERT_ARE_EQUAL(int, 0, result2);
    ASSERT_ARE_EQUAL(int, 0, result3);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_all_3_items_insert_0_1_2_remove_2_0_1)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result1;
    int result2;
    int result3;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);
    (void)lock_free_set_insert(set, &items[2]);

    // act
    result1 = lock_free_set_remove(set, &items[2]);
    result2 = lock_free_set_remove(set, &items[0]);
    result3 = lock_free_set_remove(set, &items[1]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result1);
    ASSERT_ARE_EQUAL(int, 0, result2);
    ASSERT_ARE_EQUAL(int, 0, result3);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_015: [ lock_free_set_remove shall remove the item item from the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_016: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_removes_all_3_items_insert_0_1_2_remove_2_1_0)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    LOCK_FREE_SET_ITEM items[3];
    int result1;
    int result2;
    int result3;
    umock_c_reset_all_calls();

    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);
    (void)lock_free_set_insert(set, &items[2]);

    // act
    result1 = lock_free_set_remove(set, &items[2]);
    result2 = lock_free_set_remove(set, &items[1]);
    result3 = lock_free_set_remove(set, &items[0]);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result1);
    ASSERT_ARE_EQUAL(int, 0, result2);
    ASSERT_ARE_EQUAL(int, 0, result3);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* lock_free_set_purge_not_thread_safe */

/* Tests_SRS_LOCK_FREE_SET_01_019: [ lock_free_set_purge_not_thread_safe shall remove all items in the set. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_020: [ On success it shall return 0. ]*/
TEST_FUNCTION(lock_free_set_purge_not_thread_safe_with_an_empty_set_succeeds)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    int result;
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_purge_not_thread_safe(set, (NODE_CLEANUP_FUNC)test_node_cleanup, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_021: [ If lock_free_set is NULL, lock_free_set_purge_not_thread_safe shall fail and return a non-zero value. ]*/
TEST_FUNCTION(lock_free_set_purge_not_thread_safe_with_NULL_set_fails)
{
    // arrange
    int result;

    // act
    result = lock_free_set_purge_not_thread_safe(NULL, (NODE_CLEANUP_FUNC)test_node_cleanup, (void*)0x4242);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_LOCK_FREE_SET_01_022: [ node_cleanup_callback and context shall be allowed to be NULL. ]*/
TEST_FUNCTION(lock_free_set_purge_not_thread_safe_with_NULL_callback_and_empty_set_succeeds)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    int result;
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_purge_not_thread_safe(set, NULL, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_022: [ node_cleanup_callback and context shall be allowed to be NULL. ]*/
TEST_FUNCTION(lock_free_set_purge_not_thread_safe_with_NULL_context_and_empty_set_succeeds)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    int result;
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_purge_not_thread_safe(set, (NODE_CLEANUP_FUNC)test_node_cleanup, NULL);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_023: [ If node_cleanup_callback is non-NULL, node_cleanup_callback shall be called for each item that exists in the set at the time lock_free_set_purge_not_thread_safe is called. ]*/
TEST_FUNCTION(lock_free_set_purge_not_thread_safe_with_one_item_calls_the_callback)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    int result;
    LOCK_FREE_SET_ITEM items[1];
    (void)lock_free_set_insert(set, &items[0]);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_node_cleanup((void*)0x4242, (struct LOCK_FREE_SET_ITEM_TAG*)&items[0]));

    // act
    result = lock_free_set_purge_not_thread_safe(set, (NODE_CLEANUP_FUNC)test_node_cleanup, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_023: [ If node_cleanup_callback is non-NULL, node_cleanup_callback shall be called for each item that exists in the set at the time lock_free_set_purge_not_thread_safe is called. ]*/
TEST_FUNCTION(lock_free_set_purge_not_thread_safe_with_2_items_calls_the_callback)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    int result;
    LOCK_FREE_SET_ITEM items[2];
    (void)lock_free_set_insert(set, &items[0]);
    (void)lock_free_set_insert(set, &items[1]);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_node_cleanup((void*)0x4242, (struct LOCK_FREE_SET_ITEM_TAG*)&items[1]));
    STRICT_EXPECTED_CALL(test_node_cleanup((void*)0x4242, (struct LOCK_FREE_SET_ITEM_TAG*)&items[0]));

    // act
    result = lock_free_set_purge_not_thread_safe(set, (NODE_CLEANUP_FUNC)test_node_cleanup, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_022: [ node_cleanup_callback and context shall be allowed to be NULL. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_023: [ If node_cleanup_callback is non-NULL, node_cleanup_callback shall be called for each item that exists in the set at the time lock_free_set_purge_not_thread_safe is called. ]*/
/* Tests_SRS_LOCK_FREE_SET_01_024: [ When node_cleanup_callback is called the set item pointer and context shall be passed as arguments. ]*/
TEST_FUNCTION(lock_free_set_purge_not_thread_safe_with_1_item_and_NULL_context_calls_the_callback)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    int result;
    LOCK_FREE_SET_ITEM items[2];
    (void)lock_free_set_insert(set, &items[0]);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_node_cleanup(NULL, (struct LOCK_FREE_SET_ITEM_TAG*)&items[0]));

    // act
    result = lock_free_set_purge_not_thread_safe(set, (NODE_CLEANUP_FUNC)test_node_cleanup, NULL);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

/* Tests_SRS_LOCK_FREE_SET_01_022: [ node_cleanup_callback and context shall be allowed to be NULL. ]*/
TEST_FUNCTION(lock_free_set_purge_not_thread_safe_with_1_item_and_NULL_callback_succeeds_but_does_not_call_callback)
{
    // arrange
    LOCK_FREE_SET_HANDLE set = lock_free_set_create();
    int result;
    LOCK_FREE_SET_ITEM items[2];
    (void)lock_free_set_insert(set, &items[0]);
    umock_c_reset_all_calls();

    // act
    result = lock_free_set_purge_not_thread_safe(set, NULL, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    lock_free_set_destroy(set, NULL, NULL);
}

END_TEST_SUITE(lock_free_set_unittests)
