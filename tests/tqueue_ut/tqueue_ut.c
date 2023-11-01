// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "umock_c/umock_c.h"
#include "umock_c/umock_c_negative_tests.h"
#include "umock_c/umocktypes_bool.h"
#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umocktypes_stdint.h"

#define ENABLE_MOCKS
#include "c_pal/interlocked.h"

#undef ENABLE_MOCKS

#include "c_pal/thandle.h"

#define ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/interlocked.h"

#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"
#include "real_interlocked.h"

#include "clds/tqueue.h"

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

TEST_DEFINE_ENUM_TYPE(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_RESULT_VALUES)
TEST_DEFINE_ENUM_TYPE(TQUEUE_POP_RESULT, TQUEUE_POP_RESULT_VALUES)

// This is the call dispatcher used for most tests
TQUEUE_DEFINE_STRUCT_TYPE(int32_t);
THANDLE_TYPE_DECLARE(TQUEUE_TYPEDEF_NAME(int32_t))
TQUEUE_TYPE_DECLARE(int32_t);
THANDLE_TYPE_DEFINE(TQUEUE_TYPEDEF_NAME(int32_t))
TQUEUE_TYPE_DEFINE(int32_t);

MOCK_FUNCTION_WITH_CODE(, void, test_push_cb_1, void*, context, int32_t*, push_dst, const int32_t*, push_src)
    *push_dst = *push_src;
MOCK_FUNCTION_END();

MOCK_FUNCTION_WITH_CODE(, void, test_pop_cb_1, void*, context, int32_t*, pop_dst, const int32_t*, pop_src)
    *pop_dst = *pop_src;
MOCK_FUNCTION_END();

MOCK_FUNCTION_WITH_CODE(, void, test_dispose_item, void*, context, const int32_t*, item)
MOCK_FUNCTION_END();

MOCK_FUNCTION_WITH_CODE(, bool, test_condition_function_true, void*, context, const int32_t*, item)
MOCK_FUNCTION_END(true);

MOCK_FUNCTION_WITH_CODE(, bool, test_condition_function_false, void*, context, const int32_t*, item)
MOCK_FUNCTION_END(false);

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, real_gballoc_hl_init(NULL, NULL));

    ASSERT_ARE_EQUAL(int, 0, umock_c_init(on_umock_c_error));
    ASSERT_ARE_EQUAL(int, 0, umocktypes_bool_register_types(), "umocktypes_bool_register_types failed");
    ASSERT_ARE_EQUAL(int, 0, umocktypes_charptr_register_types(), "umocktypes_charptr_register_types failed");
    ASSERT_ARE_EQUAL(int, 0, umocktypes_stdint_register_types(), "umocktypes_stdint_register_types failed");

    REGISTER_GBALLOC_HL_GLOBAL_MOCK_HOOK();
    REGISTER_INTERLOCKED_GLOBAL_MOCK_HOOK();

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(malloc_flex, NULL);

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
    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

/* TQUEUE_CREATE */

/* Tests_SRS_TQUEUE_01_001: [ If queue_size is 0, TQUEUE_CREATE(T) shall fail and return NULL. ]*/
TEST_FUNCTION(TQUEUE_CREATE_with_0_queue_size_fails)
{
    // arrange

    // act
    TQUEUE(int32_t) queue = TQUEUE_CREATE(int32_t)(0, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);

    // assert
    ASSERT_IS_NULL(queue);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_002: [ If any of push_cb_function, pop_cb_function and dispose_item_function is NULL and at least one of them is not NULL, TQUEUE_CREATE(T) shall fail and return NULL. ]*/
TEST_FUNCTION(TQUEUE_CREATE_with_only_push_cb_being_NULL_fails)
{
    // arrange

    // act
    TQUEUE(int32_t) queue = TQUEUE_CREATE(int32_t)(1024, NULL, test_pop_cb_1, test_dispose_item, (void*)0x4242);

    // assert
    ASSERT_IS_NULL(queue);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_002: [ If any of push_cb_function, pop_cb_function and dispose_item_function is NULL and at least one of them is not NULL, TQUEUE_CREATE(T) shall fail and return NULL. ]*/
TEST_FUNCTION(TQUEUE_CREATE_with_only_pop_cb_being_NULL_fails)
{
    // arrange

    // act
    TQUEUE(int32_t) queue = TQUEUE_CREATE(int32_t)(1024, test_push_cb_1, NULL, test_dispose_item, (void*)0x4242);

    // assert
    ASSERT_IS_NULL(queue);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_002: [ If any of push_cb_function, pop_cb_function and dispose_item_function is NULL and at least one of them is not NULL, TQUEUE_CREATE(T) shall fail and return NULL. ]*/
TEST_FUNCTION(TQUEUE_CREATE_with_only_dispose_item_being_NULL_fails)
{
    // arrange

    // act
    TQUEUE(int32_t) queue = TQUEUE_CREATE(int32_t)(1024, test_push_cb_1, test_pop_cb_1, NULL, (void*)0x4242);

    // assert
    ASSERT_IS_NULL(queue);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_002: [ If any of push_cb_function, pop_cb_function and dispose_item_function is NULL and at least one of them is not NULL, TQUEUE_CREATE(T) shall fail and return NULL. ]*/
TEST_FUNCTION(TQUEUE_CREATE_with_push_and_pop_cb_NULL_fails)
{
    // arrange

    // act
    TQUEUE(int32_t) queue = TQUEUE_CREATE(int32_t)(1024, NULL, NULL, test_dispose_item, (void*)0x4242);

    // assert
    ASSERT_IS_NULL(queue);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_002: [ If any of push_cb_function, pop_cb_function and dispose_item_function is NULL and at least one of them is not NULL, TQUEUE_CREATE(T) shall fail and return NULL. ]*/
TEST_FUNCTION(TQUEUE_CREATE_with_push_cb_and_dispose_item_NULL_fails)
{
    // arrange

    // act
    TQUEUE(int32_t) queue = TQUEUE_CREATE(int32_t)(1024, NULL, test_pop_cb_1, NULL, (void*)0x4242);

    // assert
    ASSERT_IS_NULL(queue);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_003: [ TQUEUE_CREATE(T) shall call THANDLE_MALLOC_FLEX with TQUEUE_DISPOSE_FUNC(T) as dispose function, nmemb set to queue_size and size set to sizeof(T). ]*/
/* Tests_SRS_TQUEUE_01_004: [ TQUEUE_CREATE(T) shall initialize the head and tail of the list with 0 by using interlocked_exchange_64. ]*/
/* Tests_SRS_TQUEUE_01_005: [ TQUEUE_CREATE(T) shall initialize the state for each entry in the array used for the queue with NOT_USED by using interlocked_exchange. ]*/
/* Tests_SRS_TQUEUE_01_006: [ TQUEUE_CREATE(T) shall succeed and return a non-NULL value. ]*/
TEST_FUNCTION(TQUEUE_CREATE_with_all_functions_non_NULL_succeeds)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc_flex(IGNORED_ARG, 1024, IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, 1));
    STRICT_EXPECTED_CALL(interlocked_exchange_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(interlocked_exchange_64(IGNORED_ARG, 0));
    for (uint32_t i = 0; i < 1024; i++)
    {
        STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED));
    }

    // act
    TQUEUE(int32_t) queue = TQUEUE_CREATE(int32_t)(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);

    // assert
    ASSERT_IS_NOT_NULL(queue);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_003: [ TQUEUE_CREATE(T) shall call THANDLE_MALLOC_FLEX with TQUEUE_DISPOSE_FUNC(T) as dispose function, nmemb set to queue_size and size set to sizeof(T). ]*/
/* Tests_SRS_TQUEUE_01_004: [ TQUEUE_CREATE(T) shall initialize the head and tail of the list with 0 by using interlocked_exchange_64. ]*/
/* Tests_SRS_TQUEUE_01_005: [ TQUEUE_CREATE(T) shall initialize the state for each entry in the array used for the queue with NOT_USED by using interlocked_exchange. ]*/
/* Tests_SRS_TQUEUE_01_006: [ TQUEUE_CREATE(T) shall succeed and return a non-NULL value. ]*/
TEST_FUNCTION(TQUEUE_CREATE_with_all_functions_NULL_succeeds)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc_flex(IGNORED_ARG, 1024, IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, 1));
    STRICT_EXPECTED_CALL(interlocked_exchange_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(interlocked_exchange_64(IGNORED_ARG, 0));
    for (uint32_t i = 0; i < 1024; i++)
    {
        STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED));
    }

    // act
    TQUEUE(int32_t) queue = TQUEUE_CREATE(int32_t)(1024, NULL, NULL, NULL, (void*)0x4242);

    // assert
    ASSERT_IS_NOT_NULL(queue);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_007: [ If there are any failures then TQUEUE_CREATE(T) shall fail and return NULL. ]*/
TEST_FUNCTION(when_underlying_calls_fail_TQUEUE_CREATE_also_fails)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc_flex(IGNORED_ARG, 1024, IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, 1))
        .CallCannotFail();
    STRICT_EXPECTED_CALL(interlocked_exchange_64(IGNORED_ARG, 0))
        .CallCannotFail();
    STRICT_EXPECTED_CALL(interlocked_exchange_64(IGNORED_ARG, 0))
        .CallCannotFail();
    for (uint32_t i = 0; i < 1024; i++)
    {
        STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED))
            .CallCannotFail();
    }

    umock_c_negative_tests_snapshot();

    for (uint32_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (umock_c_negative_tests_can_call_fail(i))
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            TQUEUE(int32_t) queue = TQUEUE_CREATE(int32_t)(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);

            // assert
            ASSERT_IS_NULL(queue, "test failed for call %" PRIu32 "", i);
        }
    }
}

/* TQUEUE_DISPOSE_FUNC(T) */

static TQUEUE(int32_t) test_queue_create(uint32_t queue_size, TQUEUE_PUSH_CB_FUNC(int32_t) push_cb_function, TQUEUE_POP_CB_FUNC(int32_t) pop_cb_function, TQUEUE_DISPOSE_ITEM_FUNC(int32_t) dispose_item_function, void* dispose_function_context)
{
    STRICT_EXPECTED_CALL(malloc_flex(IGNORED_ARG, queue_size, IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, 1));
    STRICT_EXPECTED_CALL(interlocked_exchange_64(IGNORED_ARG, 0));
    STRICT_EXPECTED_CALL(interlocked_exchange_64(IGNORED_ARG, 0));
    for (uint32_t i = 0; i < queue_size; i++)
    {
        STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED));
    }

    TQUEUE(int32_t) result = TQUEUE_CREATE(int32_t)(queue_size, push_cb_function, pop_cb_function, dispose_item_function, dispose_function_context);
    ASSERT_IS_NOT_NULL(result);
    umock_c_reset_all_calls();

    return result;
}

/* Tests_SRS_TQUEUE_01_008: [ If dispose_item_function passed to TQUEUE_CREATE(T) is NULL, TQUEUE_DISPOSE_FUNC(T) shall return. ]*/
TEST_FUNCTION(TQUEUE_DISPOSE_FUNC_with_NULL_dispose_item_returns)
{
    // arrange
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, (void*)0x4242);

    STRICT_EXPECTED_CALL(interlocked_decrement(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_009: [ Otherwise, TQUEUE_DISPOSE_FUNC(T) shall obtain the current queue head by calling interlocked_add_64. ]*/
/* Tests_SRS_TQUEUE_01_010: [ TQUEUE_DISPOSE_FUNC(T) shall obtain the current queue tail by calling interlocked_add_64. ]*/
/* Tests_SRS_TQUEUE_01_011: [ For each item in the queue, dispose_item_function shall be called with dispose_function_context and a pointer to the array entry value (T*). ]*/
TEST_FUNCTION(TQUEUE_DISPOSE_FUNC_with_non_NULL_dispose_item_with_empty_queue_does_not_call_dispose_item)
{
    // arrange
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);

    STRICT_EXPECTED_CALL(interlocked_decrement(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

static void test_queue_push(TQUEUE(int32_t) queue, int32_t item)
{
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, TQUEUE_PUSH(int32_t)(queue, &item, NULL));
    umock_c_reset_all_calls();
}

static int32_t test_queue_pop(TQUEUE(int32_t) queue)
{
    int32_t result;
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, TQUEUE_POP(int32_t)(queue, &result, NULL, NULL, NULL));
    umock_c_reset_all_calls();
    return result;
}

static int32_t test_queue_pop_rejected(TQUEUE(int32_t) queue)
{
    int32_t result;

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(test_condition_function_false((void*)0x4247, IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // revert entry_state

    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_REJECTED, TQUEUE_POP(int32_t)(queue, &result, NULL, test_condition_function_false, (void*)0x4247));
    umock_c_reset_all_calls();
    return result;
}

/* Tests_SRS_TQUEUE_01_009: [ Otherwise, TQUEUE_DISPOSE_FUNC(T) shall obtain the current queue head by calling interlocked_add_64. ]*/
/* Tests_SRS_TQUEUE_01_010: [ TQUEUE_DISPOSE_FUNC(T) shall obtain the current queue tail by calling interlocked_add_64. ]*/
/* Tests_SRS_TQUEUE_01_011: [ For each item in the queue, dispose_item_function shall be called with dispose_function_context and a pointer to the array entry value (T*). ]*/
TEST_FUNCTION(TQUEUE_DISPOSE_FUNC_with_non_NULL_dispose_item_with_1_item_calls_dispose_item_once)
{
    // arrange
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);
    test_queue_push(queue, 42);

    STRICT_EXPECTED_CALL(interlocked_decrement(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(test_dispose_item((void*)0x4242, IGNORED_ARG)); // tail
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_009: [ Otherwise, TQUEUE_DISPOSE_FUNC(T) shall obtain the current queue head by calling interlocked_add_64. ]*/
/* Tests_SRS_TQUEUE_01_010: [ TQUEUE_DISPOSE_FUNC(T) shall obtain the current queue tail by calling interlocked_add_64. ]*/
/* Tests_SRS_TQUEUE_01_011: [ For each item in the queue, dispose_item_function shall be called with dispose_function_context and a pointer to the array entry value (T*). ]*/
TEST_FUNCTION(TQUEUE_DISPOSE_FUNC_with_non_NULL_dispose_item_with_2_items_calls_dispose_item_2_times)
{
    // arrange
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);
    test_queue_push(queue, 42);
    test_queue_push(queue, 42);

    STRICT_EXPECTED_CALL(interlocked_decrement(IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(test_dispose_item((void*)0x4242, IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_dispose_item((void*)0x4242, IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* TQUEUE_PUSH(T) */

/* Tests_SRS_TQUEUE_01_012: [ If tqueue is NULL then TQUEUE_PUSH(T) shall fail and return TQUEUE_PUSH_INVALID_ARG. ]*/
TEST_FUNCTION(TQUEUE_PUSH_with_NULL_queue_fails)
{
    // arrange
    int32_t item = 42;

    // act
    TQUEUE_PUSH_RESULT result = TQUEUE_PUSH(int32_t)(NULL, &item, (void*)0x4243);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_013: [ If item is NULL then TQUEUE_PUSH(T) shall fail and return TQUEUE_PUSH_INVALID_ARG. ]*/
TEST_FUNCTION(TQUEUE_PUSH_with_NULL_item_fails)
{
    // arrange
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);

    // act
    TQUEUE_PUSH_RESULT result = TQUEUE_PUSH(int32_t)(queue, NULL, (void*)0x4243);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_014: [ TQUEUE_PUSH(T) shall execute the following actions until it is either able to push the item in the queue or the queue is full: ]*/
    /* Tests_SRS_TQUEUE_01_015: [ TQUEUE_PUSH(T) shall obtain the current head queue by calling interlocked_add_64. ]*/
    /* Tests_SRS_TQUEUE_01_016: [ TQUEUE_PUSH(T) shall obtain the current tail queue by calling interlocked_add_64. ]*/
    /* Tests_SRS_TQUEUE_01_017: [ Using interlocked_compare_exchange, TQUEUE_PUSH(T) shall change the head array entry state to PUSHING (from NOT_USED). ]*/
    /* Tests_SRS_TQUEUE_01_018: [ Using interlocked_compare_exchange_64, TQUEUE_PUSH(T) shall replace the head value with the head value obtained earlier + 1. ]*/
    /* Tests_SRS_TQUEUE_01_019: [ If no push_cb_function was specified in TQUEUE_CREATE(T), TQUEUE_PUSH(T) shall copy the value of item into the array entry value whose state was changed to PUSHING. ]*/
    /* Tests_SRS_TQUEUE_01_020: [ TQUEUE_PUSH(T) shall set the state to USED by using interlocked_exchange. ]*/
    /* Tests_SRS_TQUEUE_01_021: [ TQUEUE_PUSH(T) shall succeed and return TQUEUE_PUSH_OK. ]*/
TEST_FUNCTION(TQUEUE_PUSH_with_valid_args_without_push_cb_func_copies_the_data_in)
{
    // arrange
    int32_t item = 42;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // head change
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED)); // entry state
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // entry state

    // act
    TQUEUE_PUSH_RESULT result = TQUEUE_PUSH(int32_t)(queue, &item, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_014: [ TQUEUE_PUSH(T) shall execute the following actions until it is either able to push the item in the queue or the queue is full: ]*/
    /* Tests_SRS_TQUEUE_01_015: [ TQUEUE_PUSH(T) shall obtain the current head queue by calling interlocked_add_64. ]*/
    /* Tests_SRS_TQUEUE_01_016: [ TQUEUE_PUSH(T) shall obtain the current tail queue by calling interlocked_add_64. ]*/
    /* Tests_SRS_TQUEUE_01_017: [ Using interlocked_compare_exchange, TQUEUE_PUSH(T) shall change the head array entry state to PUSHING (from NOT_USED). ]*/
    /* Tests_SRS_TQUEUE_01_018: [ Using interlocked_compare_exchange_64, TQUEUE_PUSH(T) shall replace the head value with the head value obtained earlier + 1. ]*/
    /* Tests_SRS_TQUEUE_01_019: [ If no push_cb_function was specified in TQUEUE_CREATE(T), TQUEUE_PUSH(T) shall copy the value of item into the array entry value whose state was changed to PUSHING. ]*/
    /* Tests_SRS_TQUEUE_01_020: [ TQUEUE_PUSH(T) shall set the state to USED by using interlocked_exchange. ]*/
    /* Tests_SRS_TQUEUE_01_021: [ TQUEUE_PUSH(T) shall succeed and return TQUEUE_PUSH_OK. ]*/
TEST_FUNCTION(TQUEUE_PUSH_twice_copies_the_data_in)
{
    // arrange
    int32_t item_1 = 42;
    int32_t item_2 = 43;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // head change
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED)); // entry state
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // entry state

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 2, 1)); // head change
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED)); // entry state
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // entry state

    // act
    TQUEUE_PUSH_RESULT result_1 = TQUEUE_PUSH(int32_t)(queue, &item_1, NULL);
    TQUEUE_PUSH_RESULT result_2 = TQUEUE_PUSH(int32_t)(queue, &item_2, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, result_1);
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, result_2);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_022: [ If the queue is full (current head >= current tail + queue size), TQUEUE_PUSH(T) shall return TQUEUE_PUSH_QUEUE_FULL. ]*/
TEST_FUNCTION(TQUEUE_PUSH_twice_for_queue_size_1_returns_QUEUE_FULL)
{
    // arrange
    int32_t item = 42;
    TQUEUE(int32_t) queue = test_queue_create(1, NULL, NULL, NULL, NULL);
    test_queue_push(queue, 42);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail

    // act
    TQUEUE_PUSH_RESULT result = TQUEUE_PUSH(int32_t)(queue, &item, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_QUEUE_FULL, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_043: [ If the queue head has changed, TQUEUE_PUSH(T) shall try again. ]*/
TEST_FUNCTION(when_head_changes_TQUEUE_PUSH_tries_again)
{
    // arrange
    int32_t item = 42;
    TQUEUE(int32_t) queue = test_queue_create(2, NULL, NULL, NULL, NULL);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0))
        .SetReturn(1); // head changed!

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // head change
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED)); // entry state
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // entry state

    // act
    TQUEUE_PUSH_RESULT result = TQUEUE_PUSH(int32_t)(queue, &item, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

static void test_TQUEUE_PUSH_when_entry_state_is_different_than_NOT_USED_tries_again(QUEUE_ENTRY_STATE queue_entry_state)
{
    // arrange
    int32_t item = 42;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // head change
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED))
        .SetReturn(queue_entry_state); // fail change one time
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED))
        .SetReturn(queue_entry_state); // fail change 2 times, just for fun
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED));
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // entry state

    // act
    TQUEUE_PUSH_RESULT result = TQUEUE_PUSH(int32_t)(queue, &item, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_023: [ If the state of the array entry corresponding to the head is not NOT_USED, TQUEUE_PUSH(T) shall try changing the state again. ]*/
TEST_FUNCTION(TQUEUE_PUSH_when_entry_state_is_PUSHING_tries_again)
{
    test_TQUEUE_PUSH_when_entry_state_is_different_than_NOT_USED_tries_again(QUEUE_ENTRY_STATE_PUSHING);
}

/* Tests_SRS_TQUEUE_01_023: [ If the state of the array entry corresponding to the head is not NOT_USED, TQUEUE_PUSH(T) shall try changing the state again. ]*/
TEST_FUNCTION(TQUEUE_PUSH_when_entry_state_is_POPPING_tries_again)
{
    test_TQUEUE_PUSH_when_entry_state_is_different_than_NOT_USED_tries_again(QUEUE_ENTRY_STATE_POPPING);
}

/* Tests_SRS_TQUEUE_01_023: [ If the state of the array entry corresponding to the head is not NOT_USED, TQUEUE_PUSH(T) shall try changing the state again. ]*/
TEST_FUNCTION(TQUEUE_PUSH_when_entry_state_is_USED_tries_again)
{
    test_TQUEUE_PUSH_when_entry_state_is_different_than_NOT_USED_tries_again(QUEUE_ENTRY_STATE_USED);
}

/* Tests_SRS_TQUEUE_01_024: [ If a push_cb_function was specified in TQUEUE_CREATE(T), TQUEUE_PUSH(T) shall call the push_cb_function with push_cb_function_context as context, a pointer to the array entry value whose state was changed to PUSHING as push_dst and item as push_src. ] */
TEST_FUNCTION(TQUEUE_PUSH_with_push_cb_function_calls_the_cb_function)
{
    // arrange
    int32_t item = 42;
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // head change
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED)); // entry state
    STRICT_EXPECTED_CALL(test_push_cb_1((void*)0x4243, IGNORED_ARG, &item)); // entry state
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // entry state

    // act
    TQUEUE_PUSH_RESULT result = TQUEUE_PUSH(int32_t)(queue, &item, (void*)0x4243);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_024: [ If a push_cb_function was specified in TQUEUE_CREATE(T), TQUEUE_PUSH(T) shall call the push_cb_function with push_cb_function_context as context, a pointer to the array entry value whose state was changed to PUSHING as push_dst and item as push_src. ] */
TEST_FUNCTION(TQUEUE_PUSH_with_push_cb_function_calls_the_cb_function_2_items)
{
    // arrange
    int32_t item_1 = 42;
    int32_t item_2 = 43;
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // head change
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED)); // entry state
    STRICT_EXPECTED_CALL(test_push_cb_1((void*)0x4243, IGNORED_ARG, &item_1)); // entry state
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // entry state

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 2, 1)); // head change
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_PUSHING, QUEUE_ENTRY_STATE_NOT_USED)); // entry state
    STRICT_EXPECTED_CALL(test_push_cb_1((void*)0x4244, IGNORED_ARG, &item_2)); // entry state
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // entry state

    // act
    TQUEUE_PUSH_RESULT result_1 = TQUEUE_PUSH(int32_t)(queue, &item_1, (void*)0x4243);
    TQUEUE_PUSH_RESULT result_2 = TQUEUE_PUSH(int32_t)(queue, &item_2, (void*)0x4244);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, result_1);
    ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, result_2);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* TQUEUE_POP(T) */

/* Tests_SRS_TQUEUE_01_025: [ If tqueue is NULL then TQUEUE_POP(T) shall fail and return TQUEUE_POP_INVALID_ARG. ]*/
TEST_FUNCTION(TQUEUE_POP_with_NULL_tqueue_fails)
{
    // arrange
    int32_t item = 45;

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(NULL, &item, (void*)0x4243, test_condition_function_true, (void*)0x4244);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_TQUEUE_01_027: [ If item is NULL then TQUEUE_POP(T) shall fail and return TQUEUE_POP_INVALID_ARG. ]*/
TEST_FUNCTION(TQUEUE_POP_with_NULL_item_fails)
{
    // arrange
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, NULL, (void*)0x4243, test_condition_function_true, (void*)0x4244);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_026: [ TQUEUE_POP(T) shall execute the following actions until it is either able to pop the item from the queue or the queue is empty: ] */
    /* Tests_SRS_TQUEUE_01_028: [ TQUEUE_POP(T) shall obtain the current head queue by calling interlocked_add_64. ]*/
    /* Tests_SRS_TQUEUE_01_029: [ TQUEUE_POP(T) shall obtain the current tail queue by calling interlocked_add_64. ]*/
    /* Tests_SRS_TQUEUE_01_030: [ Using interlocked_compare_exchange, TQUEUE_PUSH(T) shall set the tail array entry state to POPPING (from USED). ]*/
    /* Tests_SRS_TQUEUE_01_031: [ TQUEUE_POP(T) shall replace the tail value with the tail value obtained earlier + 1 by using interlocked_exchange_64. ]*/
    /* Tests_SRS_TQUEUE_01_032: [ If a pop_cb_function was not specified in TQUEUE_CREATE(T): ]*/
    /* Tests_SRS_TQUEUE_01_033: [ TQUEUE_POP(T) shall copy array entry value whose state was changed to POPPING to item. ]*/
    /* Tests_SRS_TQUEUE_01_034: [ TQUEUE_POP(T) shall set the state to NOT_USED by using interlocked_exchange, succeed and return TQUEUE_POP_OK. ]*/
TEST_FUNCTION(TQUEUE_POP_with_NULL_pop_cb_function_and_NULL_condition_function_succeeds)
{
    // arrange
    int32_t item = 45;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);
    test_queue_push(queue, 42);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result);
    ASSERT_ARE_EQUAL(int32_t, 42, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_026: [ TQUEUE_POP(T) shall execute the following actions until it is either able to pop the item from the queue or the queue is empty: ] */
    /* Tests_SRS_TQUEUE_01_028: [ TQUEUE_POP(T) shall obtain the current head queue by calling interlocked_add_64. ]*/
    /* Tests_SRS_TQUEUE_01_029: [ TQUEUE_POP(T) shall obtain the current tail queue by calling interlocked_add_64. ]*/
    /* Tests_SRS_TQUEUE_01_030: [ Using interlocked_compare_exchange, TQUEUE_PUSH(T) shall set the tail array entry state to POPPING (from USED). ]*/
    /* Tests_SRS_TQUEUE_01_031: [ TQUEUE_POP(T) shall replace the tail value with the tail value obtained earlier + 1 by using interlocked_exchange_64. ]*/
    /* Tests_SRS_TQUEUE_01_032: [ If a pop_cb_function was not specified in TQUEUE_CREATE(T): ]*/
    /* Tests_SRS_TQUEUE_01_033: [ TQUEUE_POP(T) shall copy array entry value whose state was changed to POPPING to item. ]*/
    /* Tests_SRS_TQUEUE_01_034: [ TQUEUE_POP(T) shall set the state to NOT_USED by using interlocked_exchange, succeed and return TQUEUE_POP_OK. ]*/
TEST_FUNCTION(TQUEUE_POP_twice_with_NULL_pop_cb_function_and_NULL_condition_function_succeeds)
{
    // arrange
    int32_t item_1 = 45;
    int32_t item_2 = 46;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);
    test_queue_push(queue, 42);
    test_queue_push(queue, 43);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 2, 1)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    // act
    TQUEUE_POP_RESULT result_1 = TQUEUE_POP(int32_t)(queue, &item_1, NULL, NULL, NULL);
    TQUEUE_POP_RESULT result_2 = TQUEUE_POP(int32_t)(queue, &item_2, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result_1);
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result_2);
    ASSERT_ARE_EQUAL(int32_t, 42, item_1);
    ASSERT_ARE_EQUAL(int32_t, 43, item_2);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_035: [ If the queue is empty (current tail == current head), TQUEUE_POP(T) shall return TQUEUE_POP_QUEUE_EMPTY. ]*/
TEST_FUNCTION(TQUEUE_POP_when_queue_is_empty_returns_QUEUE_EMPTY)
{
    // arrange
    int32_t item = 45;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_QUEUE_EMPTY, result);
    ASSERT_ARE_EQUAL(int32_t, 45, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_035: [ If the queue is empty (current tail == current head), TQUEUE_POP(T) shall return TQUEUE_POP_QUEUE_EMPTY. ]*/
TEST_FUNCTION(TQUEUE_POP_when_queue_is_empty_and_queue_size_1_returns_QUEUE_EMPTY)
{
    // arrange
    int32_t item = 45;
    TQUEUE(int32_t) queue = test_queue_create(1, NULL, NULL, NULL, NULL);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_QUEUE_EMPTY, result);
    ASSERT_ARE_EQUAL(int32_t, 45, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_035: [ If the queue is empty (current tail == current head), TQUEUE_POP(T) shall return TQUEUE_POP_QUEUE_EMPTY. ]*/
TEST_FUNCTION(TQUEUE_POP_when_queue_is_empty_after_a_push_and_a_pop_and_queue_size_1_returns_QUEUE_EMPTY)
{
    // arrange
    TQUEUE(int32_t) queue = test_queue_create(1, NULL, NULL, NULL, NULL);
    test_queue_push(queue, 42);
    ASSERT_ARE_EQUAL(int32_t, 42, test_queue_pop(queue));

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail

    // act
    int32_t item = 45;
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_QUEUE_EMPTY, result);
    ASSERT_ARE_EQUAL(int32_t, 45, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

static void test_TQUEUE_POP_when_entry_state_is_different_than_USED_tries_again(QUEUE_ENTRY_STATE queue_entry_state)
{
    // arrange
    int32_t item = 45;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);
    test_queue_push(queue, 42);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED))
        .SetReturn(queue_entry_state); // entry_state

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // retry for entry_state
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result);
    ASSERT_ARE_EQUAL(int32_t, 42, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_036: [ If the state of the array entry corresponding to the tail is not USED, TQUEUE_POP(T) shall try again. ]*/
TEST_FUNCTION(TQUEUE_POP_when_entry_state_is_PUSHING_tries_again)
{
    test_TQUEUE_POP_when_entry_state_is_different_than_USED_tries_again(QUEUE_ENTRY_STATE_PUSHING);
}

/* Tests_SRS_TQUEUE_01_036: [ If the state of the array entry corresponding to the tail is not USED, TQUEUE_POP(T) shall try again. ]*/
TEST_FUNCTION(TQUEUE_POP_when_entry_state_is_POPPING_tries_again)
{
    test_TQUEUE_POP_when_entry_state_is_different_than_USED_tries_again(QUEUE_ENTRY_STATE_POPPING);
}

/* Tests_SRS_TQUEUE_01_036: [ If the state of the array entry corresponding to the tail is not USED, TQUEUE_POP(T) shall try again. ]*/
TEST_FUNCTION(TQUEUE_POP_when_entry_state_is_NOT_USED_tries_again)
{
    test_TQUEUE_POP_when_entry_state_is_different_than_USED_tries_again(QUEUE_ENTRY_STATE_NOT_USED);
}

/* Tests_SRS_TQUEUE_01_037: [ If a pop_cb_function was specified in TQUEUE_CREATE(T): ]*/
/* Tests_SRS_TQUEUE_01_038: [ TQUEUE_POP(T) shall call pop_cb_function with pop_cb_function_context as context, the array entry value whose state was changed to POPPING to item as pop_src and item as pop_dst. ]*/
TEST_FUNCTION(TQUEUE_POP_with_pop_cb_function_succeeds)
{
    // arrange
    int32_t item = 45;
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);
    test_queue_push(queue, 42);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // tail
    STRICT_EXPECTED_CALL(test_pop_cb_1((void*)0x4243, &item, IGNORED_ARG)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, (void*)0x4243, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result);
    ASSERT_ARE_EQUAL(int32_t, 42, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_037: [ If a pop_cb_function was specified in TQUEUE_CREATE(T): ]*/
/* Tests_SRS_TQUEUE_01_038: [ TQUEUE_POP(T) shall call pop_cb_function with pop_cb_function_context as context, the array entry value whose state was changed to POPPING to item as pop_src and item as pop_dst. ]*/
TEST_FUNCTION(TQUEUE_POP_with_pop_cb_function_twice_succeeds)
{
    // arrange
    int32_t item_1 = 45;
    int32_t item_2 = 46;
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);
    test_queue_push(queue, 42);
    test_queue_push(queue, 43);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // tail
    STRICT_EXPECTED_CALL(test_pop_cb_1((void*)0x4243, &item_1, IGNORED_ARG)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 2, 1)); // tail
    STRICT_EXPECTED_CALL(test_pop_cb_1((void*)0x4244, &item_2, IGNORED_ARG)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    // act
    TQUEUE_POP_RESULT result_1 = TQUEUE_POP(int32_t)(queue, &item_1, (void*)0x4243, NULL, NULL);
    TQUEUE_POP_RESULT result_2 = TQUEUE_POP(int32_t)(queue, &item_2, (void*)0x4244, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result_1);
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result_2);
    ASSERT_ARE_EQUAL(int32_t, 42, item_1);
    ASSERT_ARE_EQUAL(int32_t, 43, item_2);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_039: [ If condition_function is not NULL: ]*/
/* Tests_SRS_TQUEUE_01_040: [ TQUEUE_POP(T) shall call condition_function with condition_function_context and a pointer to the array entry value whose state was changed to POPPING. ] */ \
/* Tests_SRS_TQUEUE_01_042: [ Otherwise, shall proceed with the pop. ]*/
TEST_FUNCTION(TQUEUE_POP_with_NULL_pop_cb_function_and_non_NULL_condition_function_succeeds)
{
    // arrange
    int32_t item = 45;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);
    test_queue_push(queue, 42);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(test_condition_function_true((void*)0x4247, IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, NULL, test_condition_function_true, (void*)0x4247);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result);
    ASSERT_ARE_EQUAL(int32_t, 42, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_041: [ If condition_function returns false, TQUEUE_POP(T) shall set the state to USED by using interlocked_exchange and return TQUEUE_POP_REJECTED. ]*/
TEST_FUNCTION(TQUEUE_POP_with_NULL_pop_cb_function_and_non_NULL_condition_function_returning_false_does_not_pop)
{
    // arrange
    int32_t item = 45;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);
    test_queue_push(queue, 42);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(test_condition_function_false((void*)0x4247, IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // revert entry_state

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, NULL, test_condition_function_false, (void*)0x4247);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_REJECTED, result);
    ASSERT_ARE_EQUAL(int32_t, 45, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_039: [ If condition_function is not NULL: ]*/
/* Tests_SRS_TQUEUE_01_040: [ TQUEUE_POP(T) shall call condition_function with condition_function_context and a pointer to the array entry value whose state was changed to POPPING. ] */ \
/* Tests_SRS_TQUEUE_01_042: [ Otherwise, shall proceed with the pop. ]*/
TEST_FUNCTION(TQUEUE_POP_after_a_POP_that_was_rejected_succeeds)
{
    // arrange
    int32_t item = 45;
    TQUEUE(int32_t) queue = test_queue_create(1024, NULL, NULL, NULL, NULL);
    test_queue_push(queue, 42);
    test_queue_pop_rejected(queue);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(test_condition_function_true((void*)0x4248, IGNORED_ARG));
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, NULL, test_condition_function_true, (void*)0x4248);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result);
    ASSERT_ARE_EQUAL(int32_t, 42, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

/* Tests_SRS_TQUEUE_01_044: [ If incrementing the tail by using interlocked_compare_exchange_64 does not succeed, TQUEUE_POP(T) shall revert the state of the array entry to USED and retry. ]*/
TEST_FUNCTION(when_switching_the_tail_does_not_succeed_TQUEUE_POP_retries)
{
    // arrange
    int32_t item = 45;
    TQUEUE(int32_t) queue = test_queue_create(1024, test_push_cb_1, test_pop_cb_1, test_dispose_item, (void*)0x4242);
    test_queue_push(queue, 42);

    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0))
        .SetReturn(1); // tail changed
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_USED)); // revert entry_state

    // restart
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // head
    STRICT_EXPECTED_CALL(interlocked_add_64(IGNORED_ARG, 0)); // tail
    STRICT_EXPECTED_CALL(interlocked_compare_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_POPPING, QUEUE_ENTRY_STATE_USED)); // entry_state
    STRICT_EXPECTED_CALL(interlocked_compare_exchange_64(IGNORED_ARG, 1, 0)); // tail
    STRICT_EXPECTED_CALL(test_pop_cb_1((void*)0x4243, &item, IGNORED_ARG)); // tail
    STRICT_EXPECTED_CALL(interlocked_exchange(IGNORED_ARG, QUEUE_ENTRY_STATE_NOT_USED)); // entry_state

    // act
    TQUEUE_POP_RESULT result = TQUEUE_POP(int32_t)(queue, &item, (void*)0x4243, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(TQUEUE_POP_RESULT, TQUEUE_POP_OK, result);
    ASSERT_ARE_EQUAL(int32_t, 42, item);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TQUEUE_ASSIGN(int32_t)(&queue, NULL);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
