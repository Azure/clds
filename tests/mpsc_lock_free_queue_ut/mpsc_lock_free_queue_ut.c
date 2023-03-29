// Copyright (c) Microsoft. All rights reserved.

#include <stdbool.h>
#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "real_gballoc_ll.h"
void* real_malloc(size_t size)
{
    return real_gballoc_ll_malloc(size);
}

void real_free(void* ptr)
{
    real_gballoc_ll_free(ptr);
}

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_bool.h"

#define ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"

#include "clds/mpsc_lock_free_queue.h"

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    ASSERT_ARE_EQUAL(int, 0, real_gballoc_hl_init(NULL, NULL));

    umock_c_init(on_umock_c_error);

    result = umocktypes_bool_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_GLOBAL_MOCK_HOOK(malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(free, real_free);
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

/* mpsc_lock_free_queue_create */

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_001: [ mpsc_lock_free_queue_create shall create a lock free multiple producer single consumer queue. ] */
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_002: [ On success, mpsc_lock_free_queue_create shall return a non-NULL handle to the newly created queue. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_create_succeeds)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue;
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));

    // act
    queue = mpsc_lock_free_queue_create();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(queue);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_003: [ If mpsc_lock_free_queue_create fails allocating memory for the queue then it shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_fails_mpsc_lock_free_queue_create_fails)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue;
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG))
        .SetReturn(NULL);

    // act
    queue = mpsc_lock_free_queue_create();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(queue);
}

/* mpsc_lock_free_queue_destroy */

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_004: [ mpsc_lock_free_queue_destroy shall free all resources associated with the lock free queue. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_destroy_frees_the_memory_for_the_queue)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    mpsc_lock_free_queue_destroy(queue);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_005: [ If mpsc_lock_free_queue is NULL, mpsc_lock_free_queue_destroy shall do nothing. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_destroy_with_NULL_handle_does_not_free_anything)
{
    // arrange

    // act
    mpsc_lock_free_queue_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* mpsc_lock_free_queue_enqueue */

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_006: [ mpsc_lock_free_queue_enqueue shall enqueue the item item in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_007: [ On success it shall return 0. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_enqueue_succeeds)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item1;
    int result;
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_enqueue(queue, &item1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_008: [ If mpsc_lock_free_queue or item is NULL, mpsc_lock_free_queue_enqueue shall fail and return a non-zero value. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_enqueue_with_NULL_queue_fails)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_ITEM item1;
    int result;

    // act
    result = mpsc_lock_free_queue_enqueue(NULL, &item1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_008: [ If mpsc_lock_free_queue or item is NULL, mpsc_lock_free_queue_enqueue shall fail and return a non-zero value. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_enqueue_with_NULL_item_fails)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    int result;
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_enqueue(queue, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_006: [ mpsc_lock_free_queue_enqueue shall enqueue the item item in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_007: [ On success it shall return 0. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_enqueue_with_2_items_succeeds)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item1;
    MPSC_LOCK_FREE_QUEUE_ITEM item2;
    int result1;
    int result2;
    umock_c_reset_all_calls();

    // act
    result1 = mpsc_lock_free_queue_enqueue(queue, &item1);
    result2 = mpsc_lock_free_queue_enqueue(queue, &item2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result1);
    ASSERT_ARE_EQUAL(int, 0, result2);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_006: [ mpsc_lock_free_queue_enqueue shall enqueue the item item in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_007: [ On success it shall return 0. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_enqueue_with_256_items_succeeds)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM items[256];
    int result;
    size_t i;
    umock_c_reset_all_calls();

    for (i = 0; i < 256; i++)
    {
        char index_string[4];
        (void)sprintf(index_string, "%zu", i);

        // act
        result = mpsc_lock_free_queue_enqueue(queue, &items[i]);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), index_string);
        ASSERT_ARE_EQUAL(int, 0, result, index_string);
    }

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* mpsc_lock_free_queue_dequeue */

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_009: [ mpsc_lock_free_queue_dequeue shall dequeue the oldest item in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_010: [ On success it shall return a pointer to the dequeued item. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_dequeue_yeilds_the_enqueued_item)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item;
    MPSC_LOCK_FREE_QUEUE_ITEM* result;
    (void)mpsc_lock_free_queue_enqueue(queue, &item);
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_dequeue(queue);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)&item, (void*)result);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_011: [ If mpsc_lock_free_queue is NULL, mpsc_lock_free_queue_dequeue shall fail and return NULL. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_dequeue_with_NULL_queue_fails)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_ITEM* result;

    // act
    result = mpsc_lock_free_queue_dequeue(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_019: [ If the queue is empty, mpsc_lock_free_queue_dequeue shall return NULL. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_dequeue_when_queue_is_empty_yields_NULL)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM* result;
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_dequeue(queue);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_009: [ mpsc_lock_free_queue_dequeue shall dequeue the oldest item in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_010: [ On success it shall return a pointer to the dequeued item. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_dequeue_2_items_yields_the_items_in_correct_order)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item1;
    MPSC_LOCK_FREE_QUEUE_ITEM item2;
    MPSC_LOCK_FREE_QUEUE_ITEM* result1;
    MPSC_LOCK_FREE_QUEUE_ITEM* result2;
    (void)mpsc_lock_free_queue_enqueue(queue, &item1);
    (void)mpsc_lock_free_queue_enqueue(queue, &item2);
    umock_c_reset_all_calls();

    // act
    result1 = mpsc_lock_free_queue_dequeue(queue);
    result2 = mpsc_lock_free_queue_dequeue(queue);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)&item1, (void*)result1);
    ASSERT_ARE_EQUAL(void_ptr, (void*)&item2, (void*)result2);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_009: [ mpsc_lock_free_queue_dequeue shall dequeue the oldest item in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_010: [ On success it shall return a pointer to the dequeued item. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_dequeue_256_items_yields_the_items_in_correct_order)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM items[256];
    MPSC_LOCK_FREE_QUEUE_ITEM* result;
    size_t i;
    for (i = 0; i < 256; i++)
    {
        (void)mpsc_lock_free_queue_enqueue(queue, &items[i]);
    }
    umock_c_reset_all_calls();

    for (i = 0; i < 256; i++)
    {
        char index_string[4];
        (void)sprintf(index_string, "%zu", i);

        // act
        result = mpsc_lock_free_queue_dequeue(queue);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), index_string);
        ASSERT_ARE_EQUAL(void_ptr, (void*)&items[i], (void*)result, index_string);
    }

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_009: [ mpsc_lock_free_queue_dequeue shall dequeue the oldest item in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_010: [ On success it shall return a pointer to the dequeued item. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_dequeue_after_each_enqueue_256_times_succeeds)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM items[256];
    MPSC_LOCK_FREE_QUEUE_ITEM* result;
    size_t i;
    umock_c_reset_all_calls();

    for (i = 0; i < 256; i++)
    {
        char index_string[4];
        (void)sprintf(index_string, "%zu", i);

        // arrange
        (void)mpsc_lock_free_queue_enqueue(queue, &items[i]);

        // act
        result = mpsc_lock_free_queue_dequeue(queue);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), index_string);
        ASSERT_ARE_EQUAL(void_ptr, (void*)&items[i], (void*)result, index_string);
    }

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* mpsc_lock_free_queue_peek */

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_025: [ mpsc_lock_free_queue_peek shall return the oldest item in the queue without removing it. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_026: [ On success it shall return a non-NULL pointer to the peeked item. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_peek_returns_the_enqueued_item)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item;
    MPSC_LOCK_FREE_QUEUE_ITEM* result;
    (void)mpsc_lock_free_queue_enqueue(queue, &item);
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_peek(queue);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)&item, (void*)result);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_027: [ If mpsc_lock_free_queue is NULL, mpsc_lock_free_queue_peek shall fail and return NULL. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_peek_with_NULL_queue_fails)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_ITEM* result;

    // act
    result = mpsc_lock_free_queue_peek(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_028: [ If the queue is empty, mpsc_lock_free_queue_peek shall return NULL. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_peek_with_an_empty_queue_returns_NULL)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM* result;
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_peek(queue);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_025: [ mpsc_lock_free_queue_peek shall return the oldest item in the queue without removing it. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_026: [ On success it shall return a non-NULL pointer to the peeked item. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_peek_twice_returns_the_same_item)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item;
    MPSC_LOCK_FREE_QUEUE_ITEM* result_1;
    MPSC_LOCK_FREE_QUEUE_ITEM* result_2;
    (void)mpsc_lock_free_queue_enqueue(queue, &item);
    umock_c_reset_all_calls();

    // act
    result_1 = mpsc_lock_free_queue_peek(queue);
    result_2 = mpsc_lock_free_queue_peek(queue);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)&item, (void*)result_1);
    ASSERT_ARE_EQUAL(void_ptr, (void*)&item, (void*)result_2);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_025: [ mpsc_lock_free_queue_peek shall return the oldest item in the queue without removing it. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_026: [ On success it shall return a non-NULL pointer to the peeked item. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_peek_twice_with_2_items_returns_the_same_item)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item1;
    MPSC_LOCK_FREE_QUEUE_ITEM item2;
    MPSC_LOCK_FREE_QUEUE_ITEM* result_1;
    MPSC_LOCK_FREE_QUEUE_ITEM* result_2;
    (void)mpsc_lock_free_queue_enqueue(queue, &item1);
    (void)mpsc_lock_free_queue_enqueue(queue, &item2);
    umock_c_reset_all_calls();

    // act
    result_1 = mpsc_lock_free_queue_peek(queue);
    result_2 = mpsc_lock_free_queue_peek(queue);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, (void*)&item1, (void*)result_1);
    ASSERT_ARE_EQUAL(void_ptr, (void*)&item1, (void*)result_2);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* mpsc_lock_free_queue_is_empty */

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_021: [ mpsc_lock_free_queue_is_empty shall set is_empty to false if at least one item is in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_023: [ On success it shall return 0. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_is_empty_on_a_queue_with_1_item_yields_false)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item;
    int result;
    bool is_empty;
    (void)mpsc_lock_free_queue_enqueue(queue, &item);
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_is_empty(queue, &is_empty);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_IS_FALSE(is_empty);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_021: [ mpsc_lock_free_queue_is_empty shall set is_empty to false if at least one item is in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_023: [ On success it shall return 0. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_is_empty_on_a_queue_with_2_items_yields_false)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item1;
    MPSC_LOCK_FREE_QUEUE_ITEM item2;
    int result;
    bool is_empty;
    (void)mpsc_lock_free_queue_enqueue(queue, &item1);
    (void)mpsc_lock_free_queue_enqueue(queue, &item2);
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_is_empty(queue, &is_empty);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_IS_FALSE(is_empty);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_022: [ mpsc_lock_free_queue_is_empty shall set is_empty to true if no items are in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_023: [ On success it shall return 0. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_is_empty_on_a_empty_queue_yields_true)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    int result;
    bool is_empty;
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_is_empty(queue, &is_empty);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_IS_TRUE(is_empty);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_022: [ mpsc_lock_free_queue_is_empty shall set is_empty to true if no items are in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_023: [ On success it shall return 0. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_is_empty_on_a_empty_queue_after_an_enqueue_and_dequeue_yields_true)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item;
    int result;
    bool is_empty;
    (void)mpsc_lock_free_queue_enqueue(queue, &item);
    (void)mpsc_lock_free_queue_dequeue(queue);
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_is_empty(queue, &is_empty);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_IS_TRUE(is_empty);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_022: [ mpsc_lock_free_queue_is_empty shall set is_empty to true if no items are in the queue. ]*/
/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_023: [ On success it shall return 0. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_is_empty_on_a_empty_queue_after_2_enqueues_and_2_dequeues_yields_true)
{
    // arrange
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    MPSC_LOCK_FREE_QUEUE_ITEM item1;
    MPSC_LOCK_FREE_QUEUE_ITEM item2;
    int result;
    bool is_empty;
    (void)mpsc_lock_free_queue_enqueue(queue, &item1);
    (void)mpsc_lock_free_queue_enqueue(queue, &item2);
    (void)mpsc_lock_free_queue_dequeue(queue);
    (void)mpsc_lock_free_queue_dequeue(queue);
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_is_empty(queue, &is_empty);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_IS_TRUE(is_empty);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_024: [ If mpsc_lock_free_queue or is_empty is NULL then mpsc_lock_free_queue_is_empty shall fail and return a non-zero value. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_is_empty_with_NULL_queue_fails)
{
    // arrange
    bool is_empty;
    int result;

    // act
    result = mpsc_lock_free_queue_is_empty(NULL, &is_empty);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
}

/* Tests_SRS_MPSC_LOCK_FREE_QUEUE_01_024: [ If mpsc_lock_free_queue or is_empty is NULL then mpsc_lock_free_queue_is_empty shall fail and return a non-zero value. ]*/
TEST_FUNCTION(mpsc_lock_free_queue_is_empty_with_NULL_is_empy_arg_fails)
{
    MPSC_LOCK_FREE_QUEUE_HANDLE queue = mpsc_lock_free_queue_create();
    int result;
    umock_c_reset_all_calls();

    // act
    result = mpsc_lock_free_queue_is_empty(queue, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_NOT_EQUAL(int, 0, result);

    // cleanup
    mpsc_lock_free_queue_destroy(queue);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
