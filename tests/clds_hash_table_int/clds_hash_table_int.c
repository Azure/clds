// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>


#include "testrunnerswitcher.h"

#include "macro_utils/macro_utils.h"

#include "c_logging/logger.h"

#include "c_pal/interlocked.h"
#include "c_pal/sync.h"
#include "c_pal/uuid.h"

#include "c_pal/interlocked_hl.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/timer.h"
#include "c_pal/threadapi.h"
#include "c_pal/thandle.h"

#include "c_util/cancellation_token.h"
#include "c_pal/tqueue.h"

#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hash_table.h"

#define SEQ_NO_STATE_VALUES \
    SEQ_NO_NOT_USED, \
    SEQ_NO_USED

MU_DEFINE_ENUM(SEQ_NO_STATE, SEQ_NO_STATE_VALUES);

TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(SEQ_NO_STATE, SEQ_NO_STATE_VALUES);
TEST_DEFINE_ENUM_TYPE(INTERLOCKED_HL_RESULT, INTERLOCKED_HL_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_RESULT_VALUES);

TQUEUE_DEFINE_STRUCT_TYPE(THANDLE(CANCELLATION_TOKEN));
THANDLE_TYPE_DECLARE(TQUEUE_TYPEDEF_NAME(THANDLE(CANCELLATION_TOKEN)));
TQUEUE_TYPE_DECLARE(THANDLE(CANCELLATION_TOKEN));

THANDLE_TYPE_DEFINE(TQUEUE_TYPEDEF_NAME(THANDLE(CANCELLATION_TOKEN)));
TQUEUE_TYPE_DEFINE(THANDLE(CANCELLATION_TOKEN));

#define THREAD_COUNT 4

typedef struct TEST_ITEM_TAG
{
    uint32_t key;
    uint32_t appendix;
} TEST_ITEM;

#define TEST_SEQ_NO_QUEUE_SIZE (1024 * 1024)

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM)

typedef struct CHAOS_TEST_ITEM_DATA_TAG
{
    CLDS_HASH_TABLE_ITEM* item;
    volatile_atomic int32_t item_state;
} CHAOS_TEST_ITEM_DATA;

typedef struct CHAOS_TEST_CONTEXT_TAG
{
    volatile_atomic int32_t done;
    CLDS_HASH_TABLE_HANDLE hash_table;
    volatile_atomic int64_t seq_no_count;
    int64_t next_seq_no_to_ack;
    volatile_atomic int32_t seq_numbers[TEST_SEQ_NO_QUEUE_SIZE];
    TQUEUE(THANDLE(CANCELLATION_TOKEN)) snapshot_cancellation_tokens; // only used for chaos with snapshot
    CHAOS_TEST_ITEM_DATA items[];
} CHAOS_TEST_CONTEXT;

static uint64_t test_compute_hash(void* key)
{
    return (uint64_t)key;
}

static uint64_t test_compute_hash_modulo_4(void* key)
{
    return (uint64_t)key % 4;
}

static int test_key_compare(void* key1, void* key2)
{
    int result;

    if ((int64_t)key1 < (int64_t)key2)
    {
        result = -1;
    }
    else if ((int64_t)key1 > (int64_t)key2)
    {
        result = 1;
    }
    else
    {
        result = 0;
    }

    return result;
}

static void mark_seq_no_as_used(CHAOS_TEST_CONTEXT* chaos_test_context, int64_t seq_no)
{
    (void)interlocked_increment_64(&chaos_test_context->seq_no_count);
    SEQ_NO_STATE seq_no_state = interlocked_compare_exchange(&chaos_test_context->seq_numbers[seq_no % TEST_SEQ_NO_QUEUE_SIZE], SEQ_NO_USED, SEQ_NO_NOT_USED);
    ASSERT_ARE_EQUAL(SEQ_NO_STATE, SEQ_NO_NOT_USED, seq_no_state, "sequence number already used at %" PRId64 "", seq_no);
    wake_by_address_single(&chaos_test_context->seq_numbers[seq_no & TEST_SEQ_NO_QUEUE_SIZE]);
}

static void test_skipped_seq_chaos(void* context, int64_t skipped_sequence_no)
{
    CHAOS_TEST_CONTEXT* chaos_test_context = context;
    mark_seq_no_as_used(chaos_test_context, skipped_sequence_no);
}

static void test_skipped_seq_no_ignore(void* context, int64_t skipped_sequence_no)
{
    (void)context;
    (void)skipped_sequence_no;
}

static void test_item_cleanup_func(void* context, struct CLDS_HASH_TABLE_ITEM_TAG* item)
{
    (void)context;
    (void)item;
}

typedef struct SHARED_KEY_INFO_TAG
{
    volatile_atomic int32_t last_written_key;
} SHARED_KEY_INFO;

typedef struct THREAD_DATA_TAG
{
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;
    volatile_atomic int32_t stop;
    uint32_t key;
    uint32_t increment;
    SHARED_KEY_INFO* shared;
} THREAD_DATA;

static void initialize_thread_data(THREAD_DATA* thread_data, SHARED_KEY_INFO* shared, CLDS_HASH_TABLE_HANDLE hash_table, CLDS_HAZARD_POINTERS_HANDLE hazard_pointers, uint32_t starting_key, uint32_t increment)
{
    thread_data->hash_table = hash_table;
    thread_data->clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data->clds_hazard_pointers_thread);
    thread_data->key = starting_key;
    thread_data->increment = increment;
    (void)interlocked_exchange(&thread_data->stop, 0);
    thread_data->shared = shared;
}

static int continuous_insert_thread(void* arg)
{
    THREAD_DATA* thread_data = arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        CLDS_HASH_TABLE_INSERT_RESULT insert_result;
        CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
        test_item->key = i;
        int64_t insert_seq_no;
        insert_result = clds_hash_table_insert(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(i + 1), item, &insert_seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, insert_result);

        (void)interlocked_exchange(&thread_data->shared->last_written_key, (int32_t)i);

        i += thread_data->increment;

#ifdef USE_VALGRIND
        // yield
        ThreadAPI_Sleep(0);
#endif
    } while (interlocked_add(&thread_data->stop, 0) == 0);

    result = 0;

    return result;
}

static int continuous_delete_thread(void* arg)
{
    THREAD_DATA* thread_data = arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        while ((uint32_t)interlocked_add(&thread_data->shared->last_written_key, 0) < i)
        {
            if (interlocked_add(&thread_data->stop, 0) != 0)
            {
                // Don't wait forever if the insert thread isn't running any more
                break;
            }
            // Spin
        }

        if (interlocked_add(&thread_data->stop, 0) != 0)
        {
            break;
        }

        CLDS_HASH_TABLE_DELETE_RESULT delete_result;
        int64_t seq_no;
        delete_result = clds_hash_table_delete(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(i + 1), &seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, delete_result);

        i += thread_data->increment;

#ifdef USE_VALGRIND
        // yield
        ThreadAPI_Sleep(0);
#endif
    } while (interlocked_add(&thread_data->stop, 0) == 0);

    result = 0;
    return result;
}

static int continuous_delete_key_value_thread(void* arg)
{
    THREAD_DATA* thread_data = arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        while ((uint32_t)interlocked_add(&thread_data->shared->last_written_key, 0) < i)
        {
            if (interlocked_add(&thread_data->stop, 0) != 0)
            {
                // Don't wait forever if the insert thread isn't running any more
                break;
            }
            // Spin
        }

        if (interlocked_add(&thread_data->stop, 0) != 0)
        {
            break;
        }

        // To simplify a bit, we actually first do a find and then delete by the value we found
        // Otherwise, we would need to share the item pointers between the insert and delete threads which adds complexity

        CLDS_HASH_TABLE_ITEM* item = clds_hash_table_find(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(i + 1));
        ASSERT_IS_NOT_NULL(item);

        CLDS_HASH_TABLE_DELETE_RESULT delete_result;
        int64_t seq_no;
        delete_result = clds_hash_table_delete_key_value(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(i + 1), item, &seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, delete_result);

        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);

        i += thread_data->increment;

#ifdef USE_VALGRIND
        // yield
        ThreadAPI_Sleep(0);
#endif
    } while (interlocked_add(&thread_data->stop, 0) == 0);

    result = 0;
    return result;
}

static int continuous_remove_thread(void* arg)
{
    THREAD_DATA* thread_data = arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        while ((uint32_t)interlocked_add(&thread_data->shared->last_written_key, 0) < i)
        {
            if (interlocked_add(&thread_data->stop, 0) != 0)
            {
                // Don't wait forever if the insert thread isn't running any more
                break;
            }
            // Spin
        }

        if (interlocked_add(&thread_data->stop, 0) != 0)
        {
            break;
        }

        CLDS_HASH_TABLE_ITEM* item;
        CLDS_HASH_TABLE_REMOVE_RESULT remove_result;
        int64_t seq_no;
        remove_result = clds_hash_table_remove(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(i + 1), &item, &seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_OK, remove_result);

        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);

        i += thread_data->increment;

#ifdef USE_VALGRIND
        // yield
        ThreadAPI_Sleep(0);
#endif
    } while (interlocked_add(&thread_data->stop, 0) == 0);

    result = 0;
    return result;
}

static int continuous_set_value_thread(void* arg)
{
    THREAD_DATA* thread_data = arg;
    int result;

    uint32_t i = thread_data->key;
    uint32_t appendix = 4242;

    do
    {
        // Just loop over everything forever, re-setting the values
        if ((uint32_t)interlocked_add(&thread_data->shared->last_written_key, 0) < i)
        {
            i = thread_data->key;
            appendix++;
        }

        CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
        test_item->key = i;
        test_item->appendix = appendix;

        CLDS_HASH_TABLE_ITEM* old_item;
        CLDS_HASH_TABLE_SET_VALUE_RESULT set_result;
        int64_t seq_no;
        set_result = clds_hash_table_set_value(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(i + 1), item, NULL, NULL, &old_item, &seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, set_result);

        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, old_item);

        i += thread_data->increment;

#ifdef USE_VALGRIND
        // yield
        ThreadAPI_Sleep(0);
#endif
    } while (interlocked_add(&thread_data->stop, 0) == 0);

    result = 0;
    return result;
}

static int continuous_find_thread(void* arg)
{
    THREAD_DATA* thread_data = arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        // Just loop over everything forever
        if ((uint32_t)interlocked_add(&thread_data->shared->last_written_key, 0) < i)
        {
            i = thread_data->key;
        }

        CLDS_HASH_TABLE_ITEM* item = clds_hash_table_find(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(i + 1));
        ASSERT_IS_NOT_NULL(item);

        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);

        i += thread_data->increment;

#ifdef USE_VALGRIND
        // yield
        ThreadAPI_Sleep(0);
#endif
    } while (interlocked_add(&thread_data->stop, 0) == 0);

    result = 0;

    return result;
}



static void fill_hash_table_sequentially(CLDS_HASH_TABLE_HANDLE hash_table, CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        CLDS_HASH_TABLE_INSERT_RESULT result;
        CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
        test_item->key = i;
        test_item->appendix = 42;
        int64_t insert_seq_no;
        result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(uintptr_t)(i + 1), item, &insert_seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);
    }
}

static void verify_all_items_present(uint32_t expected_count, CLDS_HASH_TABLE_ITEM** items, uint64_t actual_count)
{
    ASSERT_ARE_EQUAL(uint64_t, (uint64_t)expected_count, actual_count);

    bool* found_array = malloc_2(expected_count, sizeof(bool));
    ASSERT_IS_NOT_NULL(found_array);

    for (uint32_t expected_index = 0; expected_index < expected_count; expected_index++)
    {
        found_array[expected_index] = false;
    }

    for (uint32_t actual_index = 0; actual_index < (uint32_t)actual_count; actual_index++)
    {
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, items[actual_index]);

        if (test_item->key >= expected_count)
        {
            ASSERT_FAIL("Found invalid key %" PRIu32 " max expected key was %" PRIu32, test_item->key, expected_count - 1);
        }
        else
        {
            if (found_array[test_item->key])
            {
                ASSERT_FAIL("Found duplicate item with key %" PRIu32, test_item->key);
            }
            else
            {
                found_array[test_item->key] = true;
            }
        }
    }

    for (uint64_t expected_index = 0; expected_index < expected_count; expected_index++)
    {
        ASSERT_IS_TRUE(found_array[expected_index], "Should have found item with key %" PRIu64, expected_index);
    }

    free(found_array);
}

static void verify_all_items_present_ignore_extras(uint32_t expected_count, CLDS_HASH_TABLE_ITEM** items, uint64_t actual_count)
{
    ASSERT_IS_TRUE((uint64_t)expected_count <= actual_count);

    bool* found_array = malloc_2(expected_count, sizeof(bool));
    ASSERT_IS_NOT_NULL(found_array);

    for (uint32_t expected_index = 0; expected_index < expected_count; expected_index++)
    {
        found_array[expected_index] = false;
    }

    for (uint32_t actual_index = 0; actual_index < (uint32_t)actual_count; actual_index++)
    {
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, items[actual_index]);

        if (test_item->key >= expected_count)
        {
            // Ignore extra items
            continue;
        }
        else
        {
            if (found_array[test_item->key])
            {
                // Ignore duplicates
                continue;
            }
            else
            {
                found_array[test_item->key] = true;
            }
        }
    }

    for (uint64_t expected_index = 0; expected_index < expected_count; expected_index++)
    {
        ASSERT_IS_TRUE(found_array[expected_index], "Should have found item with key %" PRIu64, expected_index);
    }

    free(found_array);
}

static void cleanup_snapshot(CLDS_HASH_TABLE_ITEM** items, uint64_t item_count)
{
    for (uint64_t i = 0; i < item_count; i++)
    {
        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, items[i]);
    }
    free(items);
}

typedef struct CHAOS_THREAD_DATA_TAG
{
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;
    THREAD_HANDLE thread_handle;
    CHAOS_TEST_CONTEXT* chaos_test_context;
} CHAOS_THREAD_DATA;

#define CHAOS_THREAD_COUNT  16
#define CHAOS_ITEM_COUNT    10000

#ifdef USE_VALGRIND
// when using Valgrind/Helgrind setting this way lower as that is ... slow
#define CHAOS_TEST_RUNTIME  10000 // ms
#else
// Otherwise run for longer
#define CHAOS_TEST_RUNTIME  300000 // ms
#endif

#define TEST_HASH_TABLE_ITEM_STATE_VALUES \
    TEST_HASH_TABLE_ITEM_NOT_USED, \
    TEST_HASH_TABLE_ITEM_INSERTING, \
    TEST_HASH_TABLE_ITEM_USED, \
    TEST_HASH_TABLE_ITEM_DELETING, \
    TEST_HASH_TABLE_ITEM_INSERTING_AGAIN, \
    TEST_HASH_TABLE_ITEM_FINDING, \
    TEST_HASH_TABLE_ITEM_SETTING_VALUE

MU_DEFINE_ENUM(TEST_HASH_TABLE_ITEM_STATE, TEST_HASH_TABLE_ITEM_STATE_VALUES);

#define CHAOS_TEST_ACTION_VALUES \
    CHAOS_TEST_ACTION_INSERT, \
    CHAOS_TEST_ACTION_DELETE_KEY_VALUE, \
    CHAOS_TEST_ACTION_DELETE, \
    CHAOS_TEST_ACTION_REMOVE, \
    CHAOS_TEST_ACTION_INSERT_KEY_TWICE, \
    CHAOS_TEST_ACTION_DELETE_KEY_VALUE_NOT_FOUND, \
    CHAOS_TEST_ACTION_DELETE_NOT_FOUND, \
    CHAOS_TEST_ACTION_REMOVE_NOT_FOUND, \
    CHAOS_TEST_ACTION_FIND, \
    CHAOS_TEST_ACTION_FIND_NOT_FOUND, \
    CHAOS_TEST_ACTION_SET_VALUE, \
    CHAOS_TEST_ACTION_SET_VALUE_SAME_ITEM, \
    CHAOS_TEST_ACTION_SET_VALUE_WITH_CONDITION_CHECK_OK, \
    CHAOS_TEST_ACTION_SET_VALUE_WITH_CONDITION_CHECK_NOT_MET, \
    CHAOS_TEST_ACTION_SET_VALUE_WITH_CONDITION_CHECK_ERROR \

MU_DEFINE_ENUM_WITHOUT_INVALID(CHAOS_TEST_ACTION, CHAOS_TEST_ACTION_VALUES);
MU_DEFINE_ENUM_STRINGS_WITHOUT_INVALID(CHAOS_TEST_ACTION, CHAOS_TEST_ACTION_VALUES);

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    gballoc_hl_init(NULL, NULL);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

TEST_FUNCTION(clds_hash_table_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    volatile_atomic int64_t sequence_number = 45;

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);

    // assert
    ASSERT_IS_NOT_NULL(hash_table);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_insert_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);
    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    int64_t insert_seq_no;

    // act
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x42, item, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_delete_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);
    CLDS_HASH_TABLE_INSERT_RESULT insert_result;
    CLDS_HASH_TABLE_DELETE_RESULT delete_result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    int64_t insert_seq_no;
    int64_t delete_seq_no;

    insert_result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x42, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, insert_result);

    // act
    delete_result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x42, &delete_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, delete_result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_insert_in_higher_level_buckets_array_returns_already_exists_for_items_left_in_the_lower_level_buckets)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);
    CLDS_HASH_TABLE_INSERT_RESULT insert_result;
    CLDS_HASH_TABLE_DELETE_RESULT delete_result;
    // 2 items to force expanding the hash table
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    int64_t insert_seq_no;
    int64_t delete_seq_no;

    // insert 2 keys: 0x42 and 0x43
    insert_result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x42, item_1, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, insert_result, "item_1 insert failed");
    insert_result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x43, item_2, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, insert_result, "item_2 insert failed");

    // delete key 0x43 (0x42 is still in there in the lowest layer of buckets)
    delete_result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x43, &delete_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, delete_result);

    // act
    // insert again item 1
    insert_result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x42, item_1, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS, insert_result, "item_1 insert second time should fail");

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_set_value_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* old_item;
    int64_t set_value_seq_no;

    // act
    set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)0x42, item, NULL, NULL, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, set_value_result, "item set value failed");

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_set_value_when_the_key_exists_on_a_lower_level_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);
    CLDS_HASH_TABLE_INSERT_RESULT insert_result;
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result;
    CLDS_HASH_TABLE_DELETE_RESULT delete_result;
    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* item_3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* old_item;
    int64_t insert_seq_no;
    int64_t delete_seq_no;
    int64_t set_value_seq_no;

    // insert 2 items and force a new level
    insert_result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x42, item_1, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, insert_result, "item_1 insert failed");
    insert_result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)0x43, item_2, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, insert_result, "item_2 insert failed");

    // delete key 0x43 (0x42 is still in there in the lowest layer of buckets)
    delete_result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x43, &delete_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, delete_result);

    // act
    set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)0x42, item_3, NULL, NULL, &old_item, &set_value_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, set_value_result, "item set value failed");
    ASSERT_ARE_EQUAL(void_ptr, item_1, old_item, "Bad old item returned");

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, old_item);
}

TEST_FUNCTION(clds_hash_table_delete_after_set_value_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result;
    CLDS_HASH_TABLE_DELETE_RESULT delete_result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* old_item;
    int64_t set_value_seq_no;
    int64_t delete_seq_no;

    set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)0x42, item, NULL, NULL, &old_item, &set_value_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, set_value_result, "item set value failed");

    // act
    delete_result = clds_hash_table_delete(hash_table, hazard_pointers_thread, (void*)0x42, &delete_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, delete_result, "item delete failed");

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_snapshot_works_with_10000_sequential_key_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count, NULL);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, result);
    verify_all_items_present(original_count, items, item_count);

    // cleanup
    cleanup_snapshot(items, item_count);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_42_017: [ clds_hash_table_snapshot shall increment a counter to lock the table for writes. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_018: [ clds_hash_table_snapshot shall wait for the ongoing write operations to complete. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_030: [ clds_hash_table_snapshot shall decrement the counter to unlock the table for writes. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_032: [ clds_hash_table_insert shall try the following until it acquires a write lock for the table: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_033: [ clds_hash_table_insert shall increment the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_034: [ If the counter to lock the table for writes is non-zero then: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_035: [ clds_hash_table_insert shall decrement the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_036: [ clds_hash_table_insert shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_063: [ clds_hash_table_insert shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_hash_table_snapshot_works_with_concurrent_inserts)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    // Start a thread to insert additional items
    THREAD_DATA thread_data;
    THREAD_HANDLE thread;
    SHARED_KEY_INFO shared;

    (void)interlocked_exchange(&shared.last_written_key, original_count - 1);
    initialize_thread_data(&thread_data, &shared, hash_table, hazard_pointers, original_count, 1);

    if (ThreadAPI_Create(&thread, continuous_insert_thread, &thread_data) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning insert test thread");
    }

    // Make sure inserts have started
    ThreadAPI_Sleep(1000);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count, NULL);

    // Inserts continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    (void)interlocked_exchange(&thread_data.stop, 1);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, result);
    verify_all_items_present_ignore_extras(original_count, items, item_count);

    int thread_result;
    (void)ThreadAPI_Join(thread, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // cleanup
    cleanup_snapshot(items, item_count);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_snapshot_works_with_multiple_concurrent_inserts)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    // Start threads to insert additional items
    THREAD_DATA thread_data[THREAD_COUNT];
    THREAD_HANDLE thread[THREAD_COUNT];
    SHARED_KEY_INFO shared[THREAD_COUNT];

    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&shared[i].last_written_key, original_count - 1);

        initialize_thread_data(&thread_data[i], &shared[i], hash_table, hazard_pointers, original_count + i, THREAD_COUNT);

        if (ThreadAPI_Create(&thread[i], continuous_insert_thread, &thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning insert test thread %" PRIu32, i);
        }
    }

    // Make sure inserts have started
    ThreadAPI_Sleep(1000);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count, NULL);

    // Inserts continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&thread_data[i].stop, 1);

        int thread_result;
        (void)ThreadAPI_Join(thread[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result);
    }

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, result);
    verify_all_items_present_ignore_extras(original_count, items, item_count);

    // cleanup
    cleanup_snapshot(items, item_count);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_42_037: [ clds_hash_table_delete shall try the following until it acquires a write lock for the table: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_038: [ clds_hash_table_delete shall increment the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_039: [ If the counter to lock the table for writes is non-zero then: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_040: [ clds_hash_table_delete shall decrement the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_041: [ clds_hash_table_delete shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_042: [ clds_hash_table_insert shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_hash_table_snapshot_works_with_multiple_concurrent_deletes)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    // Write additional items that will get deleted
    uint32_t original_count = 10000;
    uint32_t next_insert = original_count + 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, next_insert);

    // Start threads to insert additional items and delete
    THREAD_DATA insert_thread_data[THREAD_COUNT];
    THREAD_HANDLE insert_thread[THREAD_COUNT];

    SHARED_KEY_INFO shared[THREAD_COUNT];

    THREAD_DATA delete_thread_data[THREAD_COUNT];
    THREAD_HANDLE delete_thread[THREAD_COUNT];

    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&shared[i].last_written_key, next_insert - 1);

        initialize_thread_data(&insert_thread_data[i], &shared[i], hash_table, hazard_pointers, next_insert + i, THREAD_COUNT);

        initialize_thread_data(&delete_thread_data[i], &shared[i], hash_table, hazard_pointers, next_insert + i, THREAD_COUNT);

        if (ThreadAPI_Create(&insert_thread[i], continuous_insert_thread, &insert_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning insert test thread %" PRIu32, i);
        }

        if (ThreadAPI_Create(&delete_thread[i], continuous_delete_thread, &delete_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning delete test thread %" PRIu32, i);
        }
    }

    // Make sure inserts and deletes have started
    ThreadAPI_Sleep(1000);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count, NULL);

    // Inserts and deletes continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&delete_thread_data[i].stop, 1);
        (void)interlocked_exchange(&insert_thread_data[i].stop, 1);

        int thread_result;
        (void)ThreadAPI_Join(delete_thread[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result);

        (void)ThreadAPI_Join(insert_thread[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result);
    }

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, result);
    verify_all_items_present_ignore_extras(original_count, items, item_count);

    // cleanup
    cleanup_snapshot(items, item_count);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_42_043: [ clds_hash_table_delete_key_value shall try the following until it acquires a write lock for the table: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_044: [ clds_hash_table_delete_key_value shall increment the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_045: [ If the counter to lock the table for writes is non-zero then: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_046: [ clds_hash_table_delete_key_value shall decrement the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_047: [ clds_hash_table_delete_key_value shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_048: [ clds_hash_table_delete_key_value shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_hash_table_snapshot_works_with_multiple_concurrent_delete_key_value)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    // Write additional items that will get deleted
    uint32_t original_count = 10000;
    uint32_t next_insert = original_count + 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, next_insert);

    // Start threads to insert additional items and delete
    THREAD_DATA insert_thread_data[THREAD_COUNT];
    THREAD_HANDLE insert_thread[THREAD_COUNT];

    SHARED_KEY_INFO shared[THREAD_COUNT];

    THREAD_DATA delete_thread_data[THREAD_COUNT];
    THREAD_HANDLE delete_thread[THREAD_COUNT];

    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&shared[i].last_written_key, next_insert - 1);

        initialize_thread_data(&insert_thread_data[i], &shared[i], hash_table, hazard_pointers, next_insert + i, THREAD_COUNT);

        initialize_thread_data(&delete_thread_data[i], &shared[i], hash_table, hazard_pointers, next_insert + i, THREAD_COUNT);

        if (ThreadAPI_Create(&insert_thread[i], continuous_insert_thread, &insert_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning insert test thread %" PRIu32, i);
        }

        if (ThreadAPI_Create(&delete_thread[i], continuous_delete_key_value_thread, &delete_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning delete test thread %" PRIu32, i);
        }
    }

    // Make sure inserts and deletes have started
    ThreadAPI_Sleep(1000);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count, NULL);

    // Inserts and deletes continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&delete_thread_data[i].stop, 1);
        (void)interlocked_exchange(&insert_thread_data[i].stop, 1);

        int thread_result;
        (void)ThreadAPI_Join(delete_thread[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result);

        (void)ThreadAPI_Join(insert_thread[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result);
    }

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, result);
    verify_all_items_present_ignore_extras(original_count, items, item_count);

    // cleanup
    cleanup_snapshot(items, item_count);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_42_049: [ clds_hash_table_remove shall try the following until it acquires a write lock for the table: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_050: [ clds_hash_table_remove shall increment the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_051: [ If the counter to lock the table for writes is non-zero then: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_052: [ clds_hash_table_remove shall decrement the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_053: [ clds_hash_table_remove shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_054: [ clds_hash_table_remove shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_hash_table_snapshot_works_with_multiple_concurrent_remove)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    // Write additional items that will get deleted
    uint32_t original_count = 10000;
    uint32_t next_insert = original_count + 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, next_insert);

    // Start threads to insert additional items and delete
    THREAD_DATA insert_thread_data[THREAD_COUNT];
    THREAD_HANDLE insert_thread[THREAD_COUNT];

    SHARED_KEY_INFO shared[THREAD_COUNT];

    THREAD_DATA delete_thread_data[THREAD_COUNT];
    THREAD_HANDLE delete_thread[THREAD_COUNT];

    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&shared[i].last_written_key, next_insert - 1);

        initialize_thread_data(&insert_thread_data[i], &shared[i], hash_table, hazard_pointers, next_insert + i, THREAD_COUNT);

        initialize_thread_data(&delete_thread_data[i], &shared[i], hash_table, hazard_pointers, next_insert + i, THREAD_COUNT);

        if (ThreadAPI_Create(&insert_thread[i], continuous_insert_thread, &insert_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning insert test thread %" PRIu32, i);
        }

        if (ThreadAPI_Create(&delete_thread[i], continuous_remove_thread, &delete_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning delete test thread %" PRIu32, i);
        }
    }

    // Make sure inserts and deletes have started
    ThreadAPI_Sleep(1000);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count, NULL);

    // Inserts and deletes continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&delete_thread_data[i].stop, 1);
        (void)interlocked_exchange(&insert_thread_data[i].stop, 1);

        int thread_result;
        (void)ThreadAPI_Join(delete_thread[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result);

        (void)ThreadAPI_Join(insert_thread[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result);
    }

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, result);
    verify_all_items_present_ignore_extras(original_count, items, item_count);

    // cleanup
    cleanup_snapshot(items, item_count);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_HASH_TABLE_42_055: [ clds_hash_table_set_value shall try the following until it acquires a write lock for the table: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_056: [ clds_hash_table_set_value shall increment the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_057: [ If the counter to lock the table for writes is non-zero then: ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_058: [ clds_hash_table_set_value shall decrement the count of pending write operations. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_059: [ clds_hash_table_set_value shall wait for the counter to lock the table for writes to reach 0 and repeat. ]*/
/* Tests_SRS_CLDS_HASH_TABLE_42_060: [ clds_hash_table_set_value shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_hash_table_snapshot_works_with_multiple_concurrent_set_value)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore /*This is noisy with set_value*/, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    // Start threads to set value on th existing items
    SHARED_KEY_INFO shared[THREAD_COUNT];

    THREAD_DATA set_thread_data[THREAD_COUNT];
    THREAD_HANDLE set_thread[THREAD_COUNT];

    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&shared[i].last_written_key, original_count - 1);

        initialize_thread_data(&set_thread_data[i], &shared[i], hash_table, hazard_pointers, i, THREAD_COUNT);

        if (ThreadAPI_Create(&set_thread[i], continuous_set_value_thread, &set_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning set value test thread %" PRIu32, i);
        }
    }

    // Make sure set value has started
    ThreadAPI_Sleep(1000);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count, NULL);

    // Set value continues to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop set value
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&set_thread_data[i].stop, 1);

        int thread_result;
        (void)ThreadAPI_Join(set_thread[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result);
    }

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, result);
    verify_all_items_present(original_count, items, item_count);

    // cleanup
    cleanup_snapshot(items, item_count);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_snapshot_works_with_multiple_concurrent_find)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    // Start threads to set value on th existing items
    SHARED_KEY_INFO shared[THREAD_COUNT];

    THREAD_DATA find_thread_data[THREAD_COUNT];
    THREAD_HANDLE find_thread[THREAD_COUNT];

    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&shared[i].last_written_key, original_count - 1);
    
        initialize_thread_data(&find_thread_data[i], &shared[i], hash_table, hazard_pointers, i, THREAD_COUNT);
    
        if (ThreadAPI_Create(&find_thread[i], continuous_find_thread, &find_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning find test thread %" PRIu32, i);
        }
    }

    LogInfo("Threads created");

    // Make sure set value has started
    ThreadAPI_Sleep(1000);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    LogInfo("Taking snapshot");

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count, NULL);

    LogInfo("Taken snapshot");

    // Find threads continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    LogInfo("Stopping find threads");

    // Stop find threads
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)interlocked_exchange(&find_thread_data[i].stop, 1);

        int thread_result;
        (void)ThreadAPI_Join(find_thread[i], &thread_result);
        ASSERT_ARE_EQUAL(int, 0, thread_result);
    }

    LogInfo("Find threads stopped");

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, result);
    verify_all_items_present(original_count, items, item_count);

    // cleanup
    cleanup_snapshot(items, item_count);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

typedef struct TEST_ITEM2_KEY_TAG {
    uint32_t key;
    UUID_T etag;
} TEST_ITEM2_KEY;

typedef struct TEST_ITEM2_TAG
{
    TEST_ITEM2_KEY key;
} TEST_ITEM2;

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM2)

static int test_key_compare2(void* key1, void* key2)
{
    int result;

    TEST_ITEM2_KEY* k1 = key1;
    TEST_ITEM2_KEY* k2 = key2;

    if ((int64_t)k1->key < (int64_t)k2->key)
    {
        result = -1;
    }
    else if ((int64_t)k1->key > (int64_t)k2->key)
    {
        result = 1;
    }
    else
    {
        result = 0;
    }

    return result;
}

static CLDS_CONDITION_CHECK_RESULT etag_condition_check(void* context, void* new_key, void* old_key)
{
    ASSERT_ARE_EQUAL(void_ptr, (void*)0x42, context);

    TEST_ITEM2_KEY* item_new_key = new_key;
    TEST_ITEM2_KEY* item_old_key = old_key;
    CLDS_CONDITION_CHECK_RESULT result;

    if (item_new_key == NULL || item_old_key == NULL)
    {
        result = CLDS_CONDITION_CHECK_ERROR;
    }
    else if (memcmp(item_new_key->etag, item_old_key->etag, sizeof(UUID_T)) != 0)
    {
        result = CLDS_CONDITION_CHECK_NOT_MET;
    }
    else
    {
        result = CLDS_CONDITION_CHECK_OK;
    }

    return result;
}

TEST_FUNCTION(clds_hash_table_set_value_with_condition_check_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare2, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM2, NULL, NULL);
    TEST_ITEM2* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM2, item);
    test_item->key.key = 0;
    ASSERT_ARE_EQUAL(int, 0, uuid_produce(test_item->key.etag));

    int64_t insert_seq_no;
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)&test_item->key, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    CLDS_HASH_TABLE_ITEM* old_item;

    // act
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)&test_item->key, item, etag_condition_check, (void *)0x42, &old_item, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, set_value_result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_set_value_with_condition_check_fails_with_not_met)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare2, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM2, NULL, NULL);
    TEST_ITEM2* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM2, item);
    test_item->key.key = 0;
    ASSERT_ARE_EQUAL(int, 0, uuid_produce(test_item->key.etag));

    int64_t insert_seq_no;
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)&test_item->key, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    CLDS_HASH_TABLE_ITEM* old_item;

    // act
    CLDS_HASH_TABLE_ITEM* item2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM2, NULL, NULL);
    TEST_ITEM2* test_item2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM2, item2);
    test_item2->key.key = 0;
    ASSERT_ARE_EQUAL(int, 0, uuid_produce(test_item2->key.etag));
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)&test_item2->key, item2, etag_condition_check, (void*)0x42, &old_item, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET, set_value_result);

    // cleanup
    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM2, item2);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_set_value_with_the_same_value_succeeds_with_initial_bucket_size_1)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
    test_item->key = 0;
    test_item->appendix = 42;

    int64_t insert_seq_no;
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(uintptr_t)(1), item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    CLDS_HASH_TABLE_ITEM* old_item;

    // act
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)(uintptr_t)(1), item, NULL, NULL, &old_item, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, set_value_result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_set_value_with_the_same_value_succeeds_with_initial_bucket_size_1024)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1024, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
    test_item->key = 0;
    test_item->appendix = 42;

    int64_t insert_seq_no;
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(uintptr_t)(1), item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    CLDS_HASH_TABLE_ITEM* old_item;

    // act
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)(uintptr_t)(1), item, NULL, NULL, &old_item, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, set_value_result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_set_value_with_same_item_after_bucket_count_increase_allows_a_colliding_key_to_be_added_to_same_bucket)
{
    // the test does the following:
    // - start with a 1 bucket hash table with keys being "integer modulo 4"
    // - insert one item with key value 1
    // - insert one item with key value 2 (at this point the hash table resizes)
    // - set value for key value 1 with the same item (this triggers an insert into the top array and a delete of the item from the old bucket array)
    //   Before the bugfix, this delete would leave the lock bit set on the removed item (which coincidentally is the same item inserted in the top bucket array)
    // - insert a third item with key 5 (has a collision with 1 and thus has to be inserted in the same bucket like item 1)

    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash_modulo_4, test_key_compare, 2, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    CLDS_HASH_TABLE_ITEM* item_1 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item_1 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item_1);
    test_item_1->key = 1;
    test_item_1->appendix = 42;

    CLDS_HASH_TABLE_ITEM* item_2 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item_2 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item_2);
    test_item_2->key = 2;
    test_item_2->appendix = 43;

    CLDS_HASH_TABLE_ITEM* item_3 = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item_3 = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item_3);
    test_item_3->key = 5;
    test_item_3->appendix = 44;

    int64_t insert_seq_no;
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(uintptr_t)(1), item_1, &insert_seq_no));
    // second item, different key
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(uintptr_t)(2), item_2, &insert_seq_no));

    CLDS_HASH_TABLE_ITEM* old_item;
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)(uintptr_t)(1), item_1, NULL, NULL, &old_item, &insert_seq_no));

    // act
    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(uintptr_t)(5), item_3, &insert_seq_no));

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

static bool get_item_and_change_state(CHAOS_TEST_ITEM_DATA* items, int item_count, int32_t new_item_state, int32_t old_item_state, int* selected_item_index)
{
    int item_index = (rand() * (item_count - 1)) / RAND_MAX;
    int start_item_index = item_index;
    bool result;

    do
    {
        if (interlocked_compare_exchange(&items[item_index].item_state, new_item_state, old_item_state) == old_item_state)
        {
            *selected_item_index = item_index;
            result = true;
            break;
        }
        else
        {
            item_index++;
            if (item_index == item_count)
            {
                item_index = 0;
            }

            if (item_index == start_item_index)
            {
                result = false;
                break;
            }
        }
    } while (1);

    return result;
}

static CLDS_CONDITION_CHECK_RESULT condition_check_ok(void* context, void* new_key, void* old_key)
{
    (void)new_key;
    (void)old_key;

    ASSERT_ARE_EQUAL(void_ptr, (void*)0x42, context);
    return CLDS_CONDITION_CHECK_OK;
}

static CLDS_CONDITION_CHECK_RESULT condition_check_not_met(void* context, void* new_key, void* old_key)
{
    (void)new_key;
    (void)old_key;

    ASSERT_ARE_EQUAL(void_ptr, (void*)0x42, context);
    return CLDS_CONDITION_CHECK_NOT_MET;
}

static CLDS_CONDITION_CHECK_RESULT condition_check_error(void* context, void* new_key, void* old_key)
{
    (void)new_key;
    (void)old_key;

    ASSERT_ARE_EQUAL(void_ptr, (void*)0x42, context);
    return CLDS_CONDITION_CHECK_ERROR;
}

static int chaos_thread(void* arg)
{
    int result;
    CHAOS_THREAD_DATA* chaos_thread_data = arg;
    CHAOS_TEST_CONTEXT* chaos_test_context = chaos_thread_data->chaos_test_context;

    srand((unsigned int)time(NULL));

    while (interlocked_add(&chaos_test_context->done, 0) != 1)
    {
        // perform one of the several actions
        CHAOS_TEST_ACTION action = (CHAOS_TEST_ACTION)(rand() * (MU_COUNT_ARG(CHAOS_TEST_ACTION_VALUES)) / (RAND_MAX + 1));
        int item_index = (rand() * (CHAOS_ITEM_COUNT - 1)) / RAND_MAX;
        int64_t seq_no = 0;

        switch (action)
        {
        default:
            ASSERT_FAIL("Invalid action: %" PRI_MU_ENUM "", MU_ENUM_VALUE(CHAOS_TEST_ACTION, action));
            break;

        case CHAOS_TEST_ACTION_INSERT:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_INSERTING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                chaos_test_context->items[item_index].item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, clds_hash_table_insert(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE_KEY_VALUE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, clds_hash_table_delete_key_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, clds_hash_table_delete(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_REMOVE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* removed_item;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_OK, clds_hash_table_remove(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, removed_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_INSERT_KEY_TWICE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_INSERTING_AGAIN, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* new_item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, new_item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS, clds_hash_table_insert(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), new_item, &seq_no));

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, new_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE_KEY_VALUE_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* new_item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, new_item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_NOT_FOUND, clds_hash_table_delete_key_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), new_item, &seq_no));

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, new_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_NOT_FOUND, clds_hash_table_delete(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &seq_no));

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_REMOVE_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* removed_item;
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_NOT_FOUND, clds_hash_table_remove(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;
        
        case CHAOS_TEST_ACTION_FIND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_FINDING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* found_item = clds_hash_table_find(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NOT_NULL(found_item);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, found_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;
        
        case CHAOS_TEST_ACTION_FIND_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_FINDING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* found_item = clds_hash_table_find(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NULL(found_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_SET_VALUE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;

                chaos_test_context->items[item_index].item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, NULL, NULL, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, old_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_SET_VALUE_SAME_ITEM:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;
            
                CLDS_HASH_TABLE_NODE_INC_REF(TEST_ITEM, chaos_test_context->items[item_index].item);
            
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, NULL, NULL, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, old_item);
            
                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_SET_VALUE_WITH_CONDITION_CHECK_OK:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;

                CLDS_HASH_TABLE_NODE_INC_REF(TEST_ITEM, chaos_test_context->items[item_index].item);

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, condition_check_ok, (void*)0x42, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, old_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_SET_VALUE_WITH_CONDITION_CHECK_NOT_MET:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;

                CLDS_HASH_TABLE_NODE_INC_REF(TEST_ITEM, chaos_test_context->items[item_index].item);

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, condition_check_not_met, (void*)0x42, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, chaos_test_context->items[item_index].item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_SET_VALUE_WITH_CONDITION_CHECK_ERROR:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;

                CLDS_HASH_TABLE_NODE_INC_REF(TEST_ITEM, chaos_test_context->items[item_index].item);

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_ERROR, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, condition_check_error, (void*)0x42, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, chaos_test_context->items[item_index].item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;
        }
    }

    result = 0;
    return result;
}

static int seq_no_clean_thread(void* arg)
{
    CHAOS_TEST_CONTEXT* chaos_test_context = arg;

    while (interlocked_add(&chaos_test_context->done, 0) != 1)
    {
        while (interlocked_compare_exchange(&chaos_test_context->seq_numbers[chaos_test_context->next_seq_no_to_ack % TEST_SEQ_NO_QUEUE_SIZE], SEQ_NO_NOT_USED, SEQ_NO_USED) == SEQ_NO_USED)
        {
            chaos_test_context->next_seq_no_to_ack++;
        }
        LogInfo("Advanced indicated sequence number to %" PRId64 "", chaos_test_context->next_seq_no_to_ack);
        (void)InterlockedHL_WaitForValue(&chaos_test_context->seq_numbers[chaos_test_context->next_seq_no_to_ack % TEST_SEQ_NO_QUEUE_SIZE], SEQ_NO_USED, 1000);
    }

    return 0;
}

TEST_FUNCTION(clds_hash_table_chaos_knight_test)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    THREAD_HANDLE seq_no_clean_thread_handle;
    volatile_atomic int64_t sequence_number;
    size_t i;

    (void)interlocked_exchange_64(&sequence_number, 0);

    CHAOS_TEST_CONTEXT* chaos_test_context = malloc_flex(sizeof(CHAOS_TEST_CONTEXT), CHAOS_ITEM_COUNT, sizeof(CHAOS_TEST_ITEM_DATA));
    ASSERT_IS_NOT_NULL(chaos_test_context);

    for (i = 0; i < TEST_SEQ_NO_QUEUE_SIZE; i++)
    {
        (void)interlocked_exchange(&chaos_test_context->seq_numbers[i], SEQ_NO_NOT_USED);
    }

    (void)interlocked_exchange_64(&chaos_test_context->seq_no_count, 0);
    chaos_test_context->next_seq_no_to_ack = 1;

    for (i = 0; i < CHAOS_ITEM_COUNT; i++)
    {
        (void)interlocked_exchange(&chaos_test_context->items[i].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
    }

    chaos_test_context->hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 8, hazard_pointers, &sequence_number, test_skipped_seq_chaos, chaos_test_context);
    ASSERT_IS_NOT_NULL(chaos_test_context->hash_table);

    (void)interlocked_exchange(&chaos_test_context->done, 0);

    // start threads doing random things on the list
    CHAOS_THREAD_DATA* chaos_thread_data = malloc_2(CHAOS_THREAD_COUNT, sizeof(CHAOS_THREAD_DATA));
    ASSERT_IS_NOT_NULL(chaos_thread_data);

    ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&seq_no_clean_thread_handle, seq_no_clean_thread, chaos_test_context), "Error spawning sequence number clean thread");

    for (i = 0; i < CHAOS_THREAD_COUNT; i++)
    {
        chaos_thread_data[i].chaos_test_context = chaos_test_context;

        chaos_thread_data[i].clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
        ASSERT_IS_NOT_NULL(chaos_thread_data[i].clds_hazard_pointers_thread);

        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&chaos_thread_data[i].thread_handle, chaos_thread, &chaos_thread_data[i]), "Error spawning test thread %zu", i);
    }

    double start_time = timer_global_get_elapsed_ms();

    LogInfo("Running chaos test for %.02f seconds", (double)CHAOS_TEST_RUNTIME / 1000);

    // act
    while (timer_global_get_elapsed_ms() - start_time < CHAOS_TEST_RUNTIME)
    {
        LogInfo("Test ran for %.02f seconds", (timer_global_get_elapsed_ms() - start_time) / 1000);
        ThreadAPI_Sleep(1000);
    }

    (void)interlocked_exchange(&chaos_test_context->done, 1);

    int dont_care;

    // assert
    for (i = 0; i < CHAOS_THREAD_COUNT; i++)
    {
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(chaos_thread_data[i].thread_handle, &dont_care), "Thread %zu failed to join", i);
    }

    ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(seq_no_clean_thread_handle, &dont_care), "Sequence number clean thread handle join failed");

    ASSERT_ARE_EQUAL(int64_t, interlocked_add_64(&sequence_number, 0), interlocked_add_64(&chaos_test_context->seq_no_count, 0));

    // cleanup
    free(chaos_thread_data);
    clds_hash_table_destroy(chaos_test_context->hash_table);
    free(chaos_test_context);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_hash_table_snapshot_can_be_cancelled)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile_atomic int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_ignore, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    THANDLE(CANCELLATION_TOKEN) cancellation_token = cancellation_token_create(false);
    ASSERT_IS_NOT_NULL(cancellation_token);

    ASSERT_ARE_EQUAL(int, 0, cancellation_token_cancel(cancellation_token));

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count, cancellation_token);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_ABANDONED, result);

    // cleanup
    THANDLE_ASSIGN(CANCELLATION_TOKEN)(&cancellation_token, NULL);
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

#define CHAOS_TEST_WITH_SNAPSHOT_ACTION_VALUES \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_INSERT, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_DELETE_KEY_VALUE, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_DELETE, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_REMOVE, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_INSERT_KEY_TWICE, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_DELETE_KEY_VALUE_NOT_FOUND, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_DELETE_NOT_FOUND, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_REMOVE_NOT_FOUND, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_FIND, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_FIND_NOT_FOUND, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE_SAME_ITEM, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE_WITH_CONDITION_CHECK_OK, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE_WITH_CONDITION_CHECK_NOT_MET, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE_WITH_CONDITION_CHECK_ERROR, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_GET_SNAPSHOT, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_GET_SNAPSHOT_WITH_CANCELLATION_TOKEN, \
    CHAOS_TEST_WITH_SNAPSHOT_ACTION_CANCEL_SNAPSHOT \

MU_DEFINE_ENUM_WITHOUT_INVALID(CHAOS_TEST_WITH_SNAPSHOT_ACTION, CHAOS_TEST_WITH_SNAPSHOT_ACTION_VALUES);
MU_DEFINE_ENUM_STRINGS_WITHOUT_INVALID(CHAOS_TEST_WITH_SNAPSHOT_ACTION, CHAOS_TEST_WITH_SNAPSHOT_ACTION_VALUES);

static int chaos_thread_with_snapshot(void* arg)
{
    int result;
    CHAOS_THREAD_DATA* chaos_thread_data = arg;
    CHAOS_TEST_CONTEXT* chaos_test_context = chaos_thread_data->chaos_test_context;

    srand((unsigned int)time(NULL));

    while (interlocked_add(&chaos_test_context->done, 0) != 1)
    {
        // perform one of the several actions
        CHAOS_TEST_WITH_SNAPSHOT_ACTION action = (CHAOS_TEST_WITH_SNAPSHOT_ACTION)(rand() * (MU_COUNT_ARG(CHAOS_TEST_WITH_SNAPSHOT_ACTION_VALUES)) / (RAND_MAX + 1));
        int item_index = (rand() * (CHAOS_ITEM_COUNT - 1)) / RAND_MAX;
        int64_t seq_no = 0;

        switch (action)
        {
        default:
            ASSERT_FAIL("Invalid action: %" PRI_MU_ENUM "", MU_ENUM_VALUE(CHAOS_TEST_WITH_SNAPSHOT_ACTION, action));
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_INSERT:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_INSERTING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                chaos_test_context->items[item_index].item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, clds_hash_table_insert(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_DELETE_KEY_VALUE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, clds_hash_table_delete_key_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_DELETE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, clds_hash_table_delete(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_REMOVE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* removed_item;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_OK, clds_hash_table_remove(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, removed_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_INSERT_KEY_TWICE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_INSERTING_AGAIN, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* new_item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, new_item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_KEY_ALREADY_EXISTS, clds_hash_table_insert(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), new_item, &seq_no));

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, new_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_DELETE_KEY_VALUE_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* new_item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, new_item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_NOT_FOUND, clds_hash_table_delete_key_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), new_item, &seq_no));

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, new_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_DELETE_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_NOT_FOUND, clds_hash_table_delete(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &seq_no));

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_REMOVE_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* removed_item;
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_NOT_FOUND, clds_hash_table_remove(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_FIND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_FINDING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* found_item = clds_hash_table_find(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NOT_NULL(found_item);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, found_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_FIND_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_FINDING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* found_item = clds_hash_table_find(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NULL(found_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;

                chaos_test_context->items[item_index].item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, NULL, NULL, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, old_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE_SAME_ITEM:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;

                CLDS_HASH_TABLE_NODE_INC_REF(TEST_ITEM, chaos_test_context->items[item_index].item);

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, NULL, NULL, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, old_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE_WITH_CONDITION_CHECK_OK:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;

                CLDS_HASH_TABLE_NODE_INC_REF(TEST_ITEM, chaos_test_context->items[item_index].item);

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, condition_check_ok, (void*)0x42, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);
                mark_seq_no_as_used(chaos_test_context, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, old_item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE_WITH_CONDITION_CHECK_NOT_MET:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;

                CLDS_HASH_TABLE_NODE_INC_REF(TEST_ITEM, chaos_test_context->items[item_index].item);

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_CONDITION_NOT_MET, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, condition_check_not_met, (void*)0x42, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, chaos_test_context->items[item_index].item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_SET_VALUE_WITH_CONDITION_CHECK_ERROR:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_SETTING_VALUE, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* old_item;

                CLDS_HASH_TABLE_NODE_INC_REF(TEST_ITEM, chaos_test_context->items[item_index].item);

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_SET_VALUE_ERROR, clds_hash_table_set_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, condition_check_error, (void*)0x42, &old_item, &seq_no));
                ASSERT_ARE_NOT_EQUAL(int64_t, 0, seq_no);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, chaos_test_context->items[item_index].item);

                (void)interlocked_exchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_GET_SNAPSHOT:
        {
            CLDS_HASH_TABLE_ITEM** items;
            uint64_t item_count;
            ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_OK, clds_hash_table_snapshot(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, &items, &item_count, NULL));
            for (uint32_t i = 0; i < item_count; i++)
            {
                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, items[i]);
            }
            free(items);
            break;
        }
        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_GET_SNAPSHOT_WITH_CANCELLATION_TOKEN:
        {
            CLDS_HASH_TABLE_ITEM** items;
            uint64_t item_count;
            THANDLE(CANCELLATION_TOKEN) cancellation_token = cancellation_token_create(false);
            ASSERT_IS_NOT_NULL(cancellation_token);

            ASSERT_ARE_EQUAL(TQUEUE_PUSH_RESULT, TQUEUE_PUSH_OK, TQUEUE_PUSH(THANDLE(CANCELLATION_TOKEN))(chaos_test_context->snapshot_cancellation_tokens, &cancellation_token, NULL));

            CLDS_HASH_TABLE_SNAPSHOT_RESULT snapshot_result = clds_hash_table_snapshot(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, &items, &item_count, cancellation_token);

            ASSERT_IS_TRUE((snapshot_result == CLDS_HASH_TABLE_SNAPSHOT_OK) || (snapshot_result == CLDS_HASH_TABLE_SNAPSHOT_ABANDONED), "snapshot_result=%" PRI_MU_ENUM "", MU_ENUM_VALUE(CLDS_HASH_TABLE_SNAPSHOT_RESULT, snapshot_result));
            if (snapshot_result == CLDS_HASH_TABLE_SNAPSHOT_OK)
            {
                for (uint32_t i = 0; i < item_count; i++)
                {
                    CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, items[i]);
                }
                free(items);
            }
            THANDLE_ASSIGN(CANCELLATION_TOKEN)(&cancellation_token, NULL);
            break;
        }
        case CHAOS_TEST_WITH_SNAPSHOT_ACTION_CANCEL_SNAPSHOT:
        {
            // pop a cancellation token and cancel it
            THANDLE(CANCELLATION_TOKEN) cancellation_token = NULL;

            TQUEUE_POP_RESULT pop_result = TQUEUE_POP(THANDLE(CANCELLATION_TOKEN))(chaos_test_context->snapshot_cancellation_tokens, &cancellation_token, NULL, NULL, NULL);
            if (pop_result == TQUEUE_POP_OK)
            {
                cancellation_token_cancel(cancellation_token);
                THANDLE_ASSIGN(CANCELLATION_TOKEN)(&cancellation_token, NULL);
            }

            break;
        }
        }
    }

    result = 0;
    return result;
}

static void THANDLE_CANCELLATION_TOKEN_copy_item(void* context, THANDLE(CANCELLATION_TOKEN)* dst, THANDLE(CANCELLATION_TOKEN)* src)
{
    (void)context;
    THANDLE_INITIALIZE(CANCELLATION_TOKEN)(dst, *src);
}

static void THANDLE_CANCELLATION_TOKEN_dispose(void* context, THANDLE(CANCELLATION_TOKEN)* item)
{
    (void)context;
    THANDLE_ASSIGN(CANCELLATION_TOKEN)(item, NULL);
}

// This is yet another chaos test, but throwing in the mix get snapshot and get snapshot cancel
TEST_FUNCTION(clds_hash_table_chaos_knight_test_with_get_snapshot)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    THREAD_HANDLE seq_no_clean_thread_handle;
    volatile_atomic int64_t sequence_number;
    size_t i;

    (void)interlocked_exchange_64(&sequence_number, 0);

    CHAOS_TEST_CONTEXT* chaos_test_context = malloc_flex(sizeof(CHAOS_TEST_CONTEXT), CHAOS_ITEM_COUNT, sizeof(CHAOS_TEST_ITEM_DATA));
    ASSERT_IS_NOT_NULL(chaos_test_context);

    for (i = 0; i < TEST_SEQ_NO_QUEUE_SIZE; i++)
    {
        (void)interlocked_exchange(&chaos_test_context->seq_numbers[i], SEQ_NO_NOT_USED);
    }

    // we are maintaining a queue of cancellation tokens that can get randomly cancelled by the test
    TQUEUE(THANDLE(CANCELLATION_TOKEN)) cancellation_tokens = TQUEUE_CREATE(THANDLE(CANCELLATION_TOKEN))(1, UINT32_MAX, THANDLE_CANCELLATION_TOKEN_copy_item, THANDLE_CANCELLATION_TOKEN_dispose, NULL);
    ASSERT_IS_NOT_NULL(cancellation_tokens);

    TQUEUE_INITIALIZE_MOVE(THANDLE(CANCELLATION_TOKEN))(&chaos_test_context->snapshot_cancellation_tokens, &cancellation_tokens);

    (void)interlocked_exchange_64(&chaos_test_context->seq_no_count, 0);
    chaos_test_context->next_seq_no_to_ack = 1;

    for (i = 0; i < CHAOS_ITEM_COUNT; i++)
    {
        (void)interlocked_exchange(&chaos_test_context->items[i].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
    }

    chaos_test_context->hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 8, hazard_pointers, &sequence_number, test_skipped_seq_chaos, chaos_test_context);
    ASSERT_IS_NOT_NULL(chaos_test_context->hash_table);

    (void)interlocked_exchange(&chaos_test_context->done, 0);

    // start threads doing random things on the list
    CHAOS_THREAD_DATA* chaos_thread_data = malloc_2(CHAOS_THREAD_COUNT, sizeof(CHAOS_THREAD_DATA));
    ASSERT_IS_NOT_NULL(chaos_thread_data);

    ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&seq_no_clean_thread_handle, seq_no_clean_thread, chaos_test_context), "Error spawning sequence number clean thread");

    for (i = 0; i < CHAOS_THREAD_COUNT; i++)
    {
        chaos_thread_data[i].chaos_test_context = chaos_test_context;

        chaos_thread_data[i].clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
        ASSERT_IS_NOT_NULL(chaos_thread_data[i].clds_hazard_pointers_thread);

        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&chaos_thread_data[i].thread_handle, chaos_thread_with_snapshot, &chaos_thread_data[i]), "Error spawning test thread %zu", i);
    }

    double start_time = timer_global_get_elapsed_ms();

    LogInfo("Running chaos test for %.02f seconds", (double)CHAOS_TEST_RUNTIME / 1000);

    // act
    while (timer_global_get_elapsed_ms() - start_time < CHAOS_TEST_RUNTIME)
    {
        LogInfo("Test ran for %.02f seconds", (timer_global_get_elapsed_ms() - start_time) / 1000);
        ThreadAPI_Sleep(1000);
    }

    (void)interlocked_exchange(&chaos_test_context->done, 1);

    int dont_care;

    // assert
    for (i = 0; i < CHAOS_THREAD_COUNT; i++)
    {
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(chaos_thread_data[i].thread_handle, &dont_care), "Thread %zu failed to join", i);
    }

    ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(seq_no_clean_thread_handle, &dont_care), "Sequence number clean thread handle join failed");

    ASSERT_ARE_EQUAL(int64_t, interlocked_add_64(&sequence_number, 0), interlocked_add_64(&chaos_test_context->seq_no_count, 0));

    // drain cancellation tokens queue
    do
    {
        THANDLE(CANCELLATION_TOKEN) cancellation_token = NULL;

        TQUEUE_POP_RESULT pop_result = TQUEUE_POP(THANDLE(CANCELLATION_TOKEN))(chaos_test_context->snapshot_cancellation_tokens, &cancellation_token, NULL, NULL, NULL);
        if (pop_result == TQUEUE_POP_OK)
        {
            THANDLE_ASSIGN(CANCELLATION_TOKEN)(&cancellation_token, NULL);
        }
        else
        {
            break;
        }
    } while (1);

    // cleanup
    free(chaos_thread_data);
    clds_hash_table_destroy(chaos_test_context->hash_table);
    TQUEUE_ASSIGN(THANDLE(CANCELLATION_TOKEN))(&chaos_test_context->snapshot_cancellation_tokens, NULL);
    free(chaos_test_context);
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
