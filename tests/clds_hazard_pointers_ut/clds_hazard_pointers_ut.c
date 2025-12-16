// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include "clds_hazard_pointers_ut_pch.h"

void* real_malloc(size_t size)
{
    return real_gballoc_ll_malloc(size);
}
void real_free(void* ptr)
{
    real_gballoc_ll_free(ptr);
}

MU_DEFINE_ENUM_STRINGS(WORKER_THREAD_SCHEDULE_PROCESS_RESULT, WORKER_THREAD_SCHEDULE_PROCESS_RESULT_VALUES)
MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

MOCK_FUNCTION_WITH_CODE(, void, test_reclaim_func, void*, node)
MOCK_FUNCTION_END()

static WORKER_THREAD_HANDLE test_worker_thread = (WORKER_THREAD_HANDLE)0x4242;

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, real_gballoc_hl_init(NULL, NULL));

    ASSERT_ARE_EQUAL(int, 0, umock_c_init(on_umock_c_error));
    ASSERT_ARE_EQUAL(int, 0, umocktypes_stdint_register_types());

    REGISTER_GBALLOC_HL_GLOBAL_MOCK_HOOK();
    REGISTER_INACTIVE_HP_THREAD_QUEUE_GLOBAL_MOCK_HOOK();

    REGISTER_GLOBAL_MOCK_RETURNS(worker_thread_create, test_worker_thread, NULL);

    REGISTER_UMOCK_ALIAS_TYPE(TQUEUE_COPY_ITEM_FUNC(CLDS_HP_INACTIVE_THREAD), void*);
    REGISTER_UMOCK_ALIAS_TYPE(TQUEUE_DISPOSE_ITEM_FUNC(CLDS_HP_INACTIVE_THREAD), void*);
    REGISTER_UMOCK_ALIAS_TYPE(TQUEUE_CONDITION_FUNC(CLDS_HP_INACTIVE_THREAD), void*);
    REGISTER_UMOCK_ALIAS_TYPE(TQUEUE(CLDS_HP_INACTIVE_THREAD), void*);
    REGISTER_UMOCK_ALIAS_TYPE(WORKER_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(WORKER_THREAD_HANDLE, void*);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    real_gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

/* clds_hazard_pointers_create */

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_001: [ clds_hazard_pointers_create shall create a new hazard pointers instance and on success return a non-NULL handle to it. ]*/
TEST_FUNCTION(clds_hazard_pointers_create_succeeds)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(worker_thread_create(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(worker_thread_open(IGNORED_ARG));
    STRICT_EXPECTED_CALL(TQUEUE_CREATE(CLDS_HP_INACTIVE_THREAD)(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(THANDLE_INITIALIZE_MOVE(TQUEUE_TYPEDEF_NAME(CLDS_HP_INACTIVE_THREAD))(IGNORED_ARG, IGNORED_ARG));

    // act
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();

    // assert
    ASSERT_IS_NOT_NULL(clds_hazard_pointers);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_002: [ If any error happens, clds_hazard_pointers_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_create_when_malloc_fails_returns_NULL)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG))
        .SetReturn(NULL);

    // act
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();

    // assert
    ASSERT_IS_NULL(clds_hazard_pointers);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* clds_hazard_pointers_destroy */

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_004: [ clds_hazard_pointers_destroy shall free all resources associated with the hazard pointers instance. ]*/
TEST_FUNCTION(clds_hazard_pointers_destroy_frees_the_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(worker_thread_close(IGNORED_ARG));
    STRICT_EXPECTED_CALL(worker_thread_destroy(IGNORED_ARG));
    STRICT_EXPECTED_CALL(TQUEUE_POP(CLDS_HP_INACTIVE_THREAD)(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(THANDLE_ASSIGN(TQUEUE_TYPEDEF_NAME(CLDS_HP_INACTIVE_THREAD))(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    clds_hazard_pointers_destroy(clds_hazard_pointers);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_005: [ If clds_hazard_pointers is NULL, clds_hazard_pointers_destroy shall return. ]*/
TEST_FUNCTION(clds_hazard_pointers_destroy_with_NULL_handle_returns)
{
    // arrange

    // act
    clds_hazard_pointers_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* clds_hazard_pointers_register_thread */

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_006: [ clds_hazard_pointers_register_thread shall register the current thread with the hazard pointers instance clds_hazard_pointers and on success return a non-NULL handle to the registered thread. ]*/
TEST_FUNCTION(clds_hazard_pointers_register_thread_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);

    // assert
    ASSERT_IS_NOT_NULL(clds_hazard_pointers_thread);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_007: [ If clds_hazard_pointers is NULL, clds_hazard_pointers_register_thread shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_register_thread_with_NULL_handle_fails)
{
    // arrange

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(NULL);

    // assert
    ASSERT_IS_NULL(clds_hazard_pointers_thread);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_008: [ If any error occurs, clds_hazard_pointers_register_thread shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_register_thread_when_malloc_fails_returns_NULL)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG))
        .SetReturn(NULL);

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);

    // assert
    ASSERT_IS_NULL(clds_hazard_pointers_thread);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/* clds_hazard_pointers_unregister_thread */

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_009: [ clds_hazard_pointers_unregister_thread shall unregister the thread identified by clds_hazard_pointers_thread from its associated hazard pointers instance. ]*/
TEST_FUNCTION(clds_hazard_pointers_unregister_thread_frees_the_thread_specific_data)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(worker_thread_schedule_process(IGNORED_ARG));

    // act
    clds_hazard_pointers_unregister_thread(clds_hazard_pointers_thread);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_010: [ If clds_hazard_pointers_thread is NULL, clds_hazard_pointers_unregister_thread shall return. ]*/
TEST_FUNCTION(clds_hazard_pointers_unregister_thread_with_NULL_handle_returns)
{
    // arrange

    // act
    clds_hazard_pointers_unregister_thread(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* clds_hazard_pointers_acquire */

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_011: [ clds_hazard_pointers_acquire shall acquire a hazard pointer for the given node and on success return a non-NULL handle to the hazard pointer record. ]*/
TEST_FUNCTION(clds_hazard_pointer_acquire_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer;
    void* pointer_1 = (void*)0x4242;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));

    // act
    hazard_pointer = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, pointer_1);

    // assert
    ASSERT_IS_NOT_NULL(hazard_pointer);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_012: [ If clds_hazard_pointers_thread is NULL, clds_hazard_pointers_acquire shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_acquire_with_NULL_thread_fails)
{
    // arrange
    void* pointer_1 = (void*)0x4242;

    // act
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer = clds_hazard_pointers_acquire(NULL, pointer_1);

    // assert
    ASSERT_IS_NULL(hazard_pointer);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_013: [ If any error occurs, clds_hazard_pointers_acquire shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_acquire_when_malloc_fails_returns_NULL)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    void* pointer_1 = (void*)0x4242;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG))
        .SetReturn(NULL);

    // act
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, pointer_1);

    // assert
    ASSERT_IS_NULL(hazard_pointer);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/* clds_hazard_pointers_release */

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_014: [ clds_hazard_pointers_release shall release the hazard pointer associated with clds_hazard_pointer_record. ]*/
TEST_FUNCTION(clds_hazard_pointers_release_releases_the_pointer)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer;
    void* pointer_1 = (void*)0x4242;
    hazard_pointer = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, pointer_1);
    umock_c_reset_all_calls();

    // act
    clds_hazard_pointers_release(clds_hazard_pointers_thread, hazard_pointer);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_015: [ If clds_hazard_pointer_record is NULL, clds_hazard_pointers_release shall return. ]*/
TEST_FUNCTION(clds_hazard_pointers_release_with_NULL_record_returns)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    umock_c_reset_all_calls();

    // act
    clds_hazard_pointers_release(clds_hazard_pointers_thread, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/* clds_hazard_pointers_reclaim */

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_016: [ clds_hazard_pointers_reclaim shall add the node to the reclaim list and when the reclaim threshold is reached, it shall trigger a reclaim cycle. ]*/
TEST_FUNCTION(clds_hazard_pointers_reclaim_adds_node_to_reclaim_list_and_triggers_reclaim)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    (void)clds_hazard_pointers_set_reclaim_threshold(clds_hazard_pointers, 1);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    void* pointer_1 = (void*)0x4242;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc_2(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_reclaim_func(pointer_1));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(TQUEUE_POP(CLDS_HP_INACTIVE_THREAD)(IGNORED_ARG, IGNORED_ARG, NULL, IGNORED_ARG, IGNORED_ARG));

    // act
    clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, pointer_1, test_reclaim_func);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_017: [ If clds_hazard_pointers_thread is NULL, clds_hazard_pointers_reclaim shall return. ]*/
TEST_FUNCTION(clds_hazard_pointers_reclaim_with_NULL_thread_returns)
{
    // arrange
    void* pointer_1 = (void*)0x4242;

    // act
    clds_hazard_pointers_reclaim(NULL, pointer_1, test_reclaim_func);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_018: [ If node is NULL, clds_hazard_pointers_reclaim shall return. ]*/
TEST_FUNCTION(clds_hazard_pointers_reclaim_with_NULL_node_returns)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    umock_c_reset_all_calls();

    // act
    clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, NULL, test_reclaim_func);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_019: [ If any other thread has acquired a hazard pointer for node, the reclaim_func shall not be called for node. ]*/
TEST_FUNCTION(clds_hazard_pointers_reclaim_with_a_hazard_pointer_set_does_not_reclaim_the_pointer)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    (void)clds_hazard_pointers_set_reclaim_threshold(clds_hazard_pointers, 1);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer;
    void* pointer_1 = (void*)0x4242;
    hazard_pointer = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, pointer_1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc_2(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(TQUEUE_POP(CLDS_HP_INACTIVE_THREAD)(IGNORED_ARG, IGNORED_ARG, NULL, IGNORED_ARG, IGNORED_ARG)); // pop from queue

    // act
    clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, pointer_1, test_reclaim_func);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_release(clds_hazard_pointers_thread, hazard_pointer);
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_020: [ If no thread has acquired a hazard pointer for node, reclaim_func shall be called for node to reclaim it. ]*/
TEST_FUNCTION(clds_hazard_pointers_reclaim_with_a_pointer_that_is_not_acquired_reclaims_the_memory)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    (void)clds_hazard_pointers_set_reclaim_threshold(clds_hazard_pointers, 1);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    void* pointer_1 = (void*)0x4242;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc_2(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_reclaim_func(pointer_1));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(TQUEUE_POP(CLDS_HP_INACTIVE_THREAD)(IGNORED_ARG, IGNORED_ARG, NULL, IGNORED_ARG, IGNORED_ARG)); // pop from queue

    // act
    clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, pointer_1, test_reclaim_func);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/* clds_hazard_pointers_set_reclaim_threshold */

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_021: [ clds_hazard_pointers_set_reclaim_threshold shall set the reclaim threshold for the hazard pointers instance to reclaim_threshold. ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_01_024: [ On success, clds_hazard_pointers_set_reclaim_threshold shall return 0. ]*/
TEST_FUNCTION(clds_hazard_pointers_set_reclaim_threshold_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    umock_c_reset_all_calls();

    // act
    int result = clds_hazard_pointers_set_reclaim_threshold(clds_hazard_pointers, 100);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_022: [ If clds_hazard_pointers is NULL, clds_hazard_pointers_set_reclaim_threshold shall fail and return a non-zero value. ]*/
TEST_FUNCTION(clds_hazard_pointers_set_reclaim_threshold_with_NULL_handle_fails)
{
    // arrange

    // act
    int result = clds_hazard_pointers_set_reclaim_threshold(NULL, 100);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_01_023: [ If reclaim_threshold is 0, clds_hazard_pointers_set_reclaim_threshold shall fail and return a non-zero value. ]*/
TEST_FUNCTION(clds_hazard_pointers_set_reclaim_threshold_with_zero_threshold_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    umock_c_reset_all_calls();

    // act
    int result = clds_hazard_pointers_set_reclaim_threshold(clds_hazard_pointers, 0);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
