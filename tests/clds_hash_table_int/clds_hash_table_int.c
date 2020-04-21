// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cinttypes>
#include <ctime>
#else
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#endif

#include "windows.h"
#include "azure_macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"
#include "azure_c_util/gballoc.h"
#include "azure_c_util/timer.h"
#include "azure_c_util/threadapi.h"
#include "azure_c_util/xlogging.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_hash_table.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;

TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SNAPSHOT_RESULT, CLDS_HASH_TABLE_SNAPSHOT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);

#define THREAD_COUNT 4

typedef struct TEST_ITEM_TAG
{
    uint32_t key;
    uint32_t appendix;
} TEST_ITEM;

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM)

static uint64_t test_compute_hash(void* key)
{
    return (uint64_t)key;
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

static void test_skipped_seq_no_cb(void* context, int64_t skipped_sequence_no)
{
    LogInfo("Skipped seq no.: %" PRIu64, skipped_sequence_no);
    (void)context;
    (void)skipped_sequence_no;
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
    volatile LONG last_written_key;
} SHARED_KEY_INFO;

typedef struct THREAD_DATA_TAG
{
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;
    volatile LONG stop;
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
    (void)InterlockedExchange(&thread_data->stop, 0);
    thread_data->shared = shared;
}

static int continuous_insert_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        CLDS_HASH_TABLE_INSERT_RESULT insert_result;
        CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
        test_item->key = i;
        int64_t insert_seq_no;
        insert_result = clds_hash_table_insert(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(INT_PTR)(i + 1), item, &insert_seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, insert_result);

        (void)InterlockedExchange(&thread_data->shared->last_written_key, (LONG)i);

        i += thread_data->increment;
    } while (InterlockedAdd(&thread_data->stop, 0) == 0);

    result = 0;

    ThreadAPI_Exit(result);
    return result;
}

static int continuous_delete_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        while ((uint32_t)InterlockedAdd(&thread_data->shared->last_written_key, 0) < i)
        {
            if (InterlockedAdd(&thread_data->stop, 0) != 0)
            {
                // Don't wait forever if the insert thread isn't running any more
                break;
            }
            // Spin
        }

        if (InterlockedAdd(&thread_data->stop, 0) != 0)
        {
            break;
        }

        CLDS_HASH_TABLE_DELETE_RESULT delete_result;
        int64_t seq_no;
        delete_result = clds_hash_table_delete(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(INT_PTR)(i + 1), &seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, delete_result);

        i += thread_data->increment;
    } while (InterlockedAdd(&thread_data->stop, 0) == 0);

    result = 0;

    ThreadAPI_Exit(result);
    return result;
}

static int continuous_delete_key_value_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        while ((uint32_t)InterlockedAdd(&thread_data->shared->last_written_key, 0) < i)
        {
            if (InterlockedAdd(&thread_data->stop, 0) != 0)
            {
                // Don't wait forever if the insert thread isn't running any more
                break;
            }
            // Spin
        }

        if (InterlockedAdd(&thread_data->stop, 0) != 0)
        {
            break;
        }

        // To simplify a bit, we actually first do a find and then delete by the value we found
        // Otherwise, we would need to share the item pointers between the insert and delete threads which adds complexity

        CLDS_HASH_TABLE_ITEM* item = clds_hash_table_find(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(INT_PTR)(i + 1));
        ASSERT_IS_NOT_NULL(item);

        CLDS_HASH_TABLE_DELETE_RESULT delete_result;
        int64_t seq_no;
        delete_result = clds_hash_table_delete_key_value(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(INT_PTR)(i + 1), item, &seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, delete_result);

        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);

        i += thread_data->increment;
    } while (InterlockedAdd(&thread_data->stop, 0) == 0);

    result = 0;

    ThreadAPI_Exit(result);
    return result;
}

static int continuous_remove_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        while ((uint32_t)InterlockedAdd(&thread_data->shared->last_written_key, 0) < i)
        {
            if (InterlockedAdd(&thread_data->stop, 0) != 0)
            {
                // Don't wait forever if the insert thread isn't running any more
                break;
            }
            // Spin
        }

        if (InterlockedAdd(&thread_data->stop, 0) != 0)
        {
            break;
        }

        CLDS_HASH_TABLE_ITEM* item;
        CLDS_HASH_TABLE_REMOVE_RESULT remove_result;
        int64_t seq_no;
        remove_result = clds_hash_table_remove(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(INT_PTR)(i + 1), &item, &seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_OK, remove_result);

        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);

        i += thread_data->increment;
    } while (InterlockedAdd(&thread_data->stop, 0) == 0);

    result = 0;

    ThreadAPI_Exit(result);
    return result;
}

static int continuous_set_value_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    uint32_t i = thread_data->key;
    uint32_t appendix = 4242;

    do
    {
        // Just loop over everything forever, re-setting the values
        if ((uint32_t)InterlockedAdd(&thread_data->shared->last_written_key, 0) < i)
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
        set_result = clds_hash_table_set_value(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(INT_PTR)(i + 1), item, &old_item, &seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, set_result);

        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, old_item);

        i += thread_data->increment;
    } while (InterlockedAdd(&thread_data->stop, 0) == 0);

    result = 0;

    ThreadAPI_Exit(result);
    return result;
}

static int continuous_find_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    uint32_t i = thread_data->key;

    do
    {
        // Just loop over everything forever
        if ((uint32_t)InterlockedAdd(&thread_data->shared->last_written_key, 0) < i)
        {
            i = thread_data->key;
        }

        CLDS_HASH_TABLE_ITEM* item = clds_hash_table_find(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, (void*)(INT_PTR)(i + 1));
        ASSERT_IS_NOT_NULL(item);

        CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, item);

        i += thread_data->increment;
    } while (InterlockedAdd(&thread_data->stop, 0) == 0);

    result = 0;

    ThreadAPI_Exit(result);
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
        result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(INT_PTR)(i + 1), item, &insert_seq_no);
        ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);
    }
}

static void verify_all_items_present(uint32_t expected_count, CLDS_HASH_TABLE_ITEM** items, uint64_t actual_count)
{
    ASSERT_ARE_EQUAL(uint64_t, (uint64_t)expected_count, actual_count);

    bool* found_array = (bool*)malloc(sizeof(bool) * expected_count);
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

    bool* found_array = (bool*)malloc(sizeof(bool) * expected_count);
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

typedef struct CHAOS_TEST_ITEM_DATA_TAG
{
    CLDS_HASH_TABLE_ITEM* item;
    volatile LONG item_state;
} CHAOS_TEST_ITEM_DATA;

typedef struct CHAOS_TEST_CONTEXT_TAG
{
    volatile LONG done;
    CLDS_HASH_TABLE_HANDLE hash_table;
#ifdef _MSC_VER
    /*warning C4200: nonstandard extension used: zero-sized array in struct/union */
#pragma warning(disable:4200)
#endif
    CHAOS_TEST_ITEM_DATA items[];
} CHAOS_TEST_CONTEXT;

typedef struct CHAOS_THREAD_DATA_TAG
{
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;
    THREAD_HANDLE thread_handle;
    CHAOS_TEST_CONTEXT* chaos_test_context;
} CHAOS_THREAD_DATA;

#define CHAOS_THREAD_COUNT  8
#define CHAOS_ITEM_COUNT    10000
#define CHAOS_TEST_RUNTIME  30000

#define TEST_HASH_TABLE_ITEM_STATE_VALUES \
    TEST_HASH_TABLE_ITEM_NOT_USED, \
    TEST_HASH_TABLE_ITEM_INSERTING, \
    TEST_HASH_TABLE_ITEM_USED, \
    TEST_HASH_TABLE_ITEM_DELETING, \
    TEST_HASH_TABLE_ITEM_INSERTING_AGAIN, \
    TEST_HASH_TABLE_ITEM_FINDING

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
    CHAOS_TEST_ACTION_FIND_NOT_FOUND

MU_DEFINE_ENUM_WITHOUT_INVALID(CHAOS_TEST_ACTION, CHAOS_TEST_ACTION_VALUES);

BEGIN_TEST_SUITE(clds_hash_table_inttests)

TEST_SUITE_INITIALIZE(suite_init)
{
    test_serialize_mutex = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(test_serialize_mutex);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    TEST_MUTEX_DESTROY(test_serialize_mutex);
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    if (TEST_MUTEX_ACQUIRE(test_serialize_mutex))
    {
        ASSERT_FAIL("Could not acquire test serialization mutex.");
    }
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    TEST_MUTEX_RELEASE(test_serialize_mutex);
}

TEST_FUNCTION(clds_hash_table_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HASH_TABLE_HANDLE hash_table;
    volatile int64_t sequence_number = 45;

    // act
    hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* old_item;
    int64_t set_value_seq_no;

    // act
    set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)0x42, item, &old_item, &set_value_seq_no);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
    set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)0x42, item_3, &old_item, &set_value_seq_no);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result;
    CLDS_HASH_TABLE_DELETE_RESULT delete_result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_HASH_TABLE_ITEM* old_item;
    int64_t set_value_seq_no;
    int64_t delete_seq_no;

    set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)0x42, item, &old_item, &set_value_seq_no);
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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    // Start a thread to insert additional items
    THREAD_DATA thread_data;
    THREAD_HANDLE thread;
    SHARED_KEY_INFO shared;

    (void)InterlockedExchange(&shared.last_written_key, original_count - 1);
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
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count);

    // Inserts continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    (void)InterlockedExchange(&thread_data.stop, 1);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    // Start threads to insert additional items
    THREAD_DATA thread_data[THREAD_COUNT];
    THREAD_HANDLE thread[THREAD_COUNT];
    SHARED_KEY_INFO shared[THREAD_COUNT];

    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)InterlockedExchange(&shared[i].last_written_key, original_count - 1);

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
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count);

    // Inserts continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)InterlockedExchange(&thread_data[i].stop, 1);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
        (void)InterlockedExchange(&shared[i].last_written_key, next_insert - 1);

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
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count);

    // Inserts and deletes continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)InterlockedExchange(&delete_thread_data[i].stop, 1);
        (void)InterlockedExchange(&insert_thread_data[i].stop, 1);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
        (void)InterlockedExchange(&shared[i].last_written_key, next_insert - 1);

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
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count);

    // Inserts and deletes continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)InterlockedExchange(&delete_thread_data[i].stop, 1);
        (void)InterlockedExchange(&insert_thread_data[i].stop, 1);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
        (void)InterlockedExchange(&shared[i].last_written_key, next_insert - 1);

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
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count);

    // Inserts and deletes continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop inserts
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)InterlockedExchange(&delete_thread_data[i].stop, 1);
        (void)InterlockedExchange(&insert_thread_data[i].stop, 1);

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
    volatile int64_t sequence_number = 45;
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
        (void)InterlockedExchange(&shared[i].last_written_key, original_count - 1);

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
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count);

    // Set value continues to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop set value
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)InterlockedExchange(&set_thread_data[i].stop, 1);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    uint32_t original_count = 10000;
    fill_hash_table_sequentially(hash_table, hazard_pointers_thread, original_count);

    // Start threads to set value on th existing items
    SHARED_KEY_INFO shared[THREAD_COUNT];

    THREAD_DATA find_thread_data[THREAD_COUNT];
    THREAD_HANDLE find_thread[THREAD_COUNT];

    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)InterlockedExchange(&shared[i].last_written_key, original_count - 1);

        initialize_thread_data(&find_thread_data[i], &shared[i], hash_table, hazard_pointers, i, THREAD_COUNT);

        if (ThreadAPI_Create(&find_thread[i], continuous_find_thread, &find_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning find test thread %" PRIu32, i);
        }
    }

    // Make sure set value has started
    ThreadAPI_Sleep(1000);

    CLDS_HASH_TABLE_ITEM** items;
    uint64_t item_count;

    // act
    CLDS_HASH_TABLE_SNAPSHOT_RESULT result = clds_hash_table_snapshot(hash_table, hazard_pointers_thread, &items, &item_count);

    // Find threads continue to run a bit longer to make sure we are in a good state
    ThreadAPI_Sleep(1000);

    // Stop find threads
    for (uint32_t i = 0; i < THREAD_COUNT; i++)
    {
        (void)InterlockedExchange(&find_thread_data[i].stop, 1);

        int thread_result;
        (void)ThreadAPI_Join(find_thread[i], &thread_result);
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

TEST_FUNCTION(clds_hash_table_set_value_with_the_same_value_succeeds_with_initial_bucket_size_1)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    ASSERT_IS_NOT_NULL(hazard_pointers);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
    test_item->key = 0;
    test_item->appendix = 42;

    int64_t insert_seq_no;
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(INT_PTR)(1), item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    CLDS_HASH_TABLE_ITEM* old_item;

    // act
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)(INT_PTR)(1), item, &old_item, &insert_seq_no);

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
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1024, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(hash_table);

    CLDS_HASH_TABLE_INSERT_RESULT result;
    CLDS_HASH_TABLE_ITEM* item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, item);
    test_item->key = 0;
    test_item->appendix = 42;

    int64_t insert_seq_no;
    result = clds_hash_table_insert(hash_table, hazard_pointers_thread, (void*)(INT_PTR)(1), item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, result);

    CLDS_HASH_TABLE_ITEM* old_item;

    // act
    CLDS_HASH_TABLE_SET_VALUE_RESULT set_value_result = clds_hash_table_set_value(hash_table, hazard_pointers_thread, (void*)(INT_PTR)(1), item, &old_item, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_OK, set_value_result);

    // cleanup
    clds_hash_table_destroy(hash_table);
    clds_hazard_pointers_destroy(hazard_pointers);
}

static bool get_item_and_change_state(CHAOS_TEST_ITEM_DATA* items, int item_count, LONG new_item_state, LONG old_item_state, int* selected_item_index)
{
    int item_index = (rand() * (item_count - 1)) / RAND_MAX;
    int start_item_index = item_index;
    bool result;

    do
    {
        if (InterlockedCompareExchange(&items[item_index].item_state, new_item_state, old_item_state) == old_item_state)
        {
            *selected_item_index = item_index;
            result = true;
            break;
        }
        else
        {
            item_index++;
            if (item_index == start_item_index)
            {
                result = false;
                break;
            }

            if (item_index == item_count)
            {
                item_index = 0;
            }
        }
    } while (1);

    return result;
}

static int chaos_thread(void* arg)
{
    int result;
    CHAOS_THREAD_DATA* chaos_thread_data = (CHAOS_THREAD_DATA*)arg;
    CHAOS_TEST_CONTEXT* chaos_test_context = (CHAOS_TEST_CONTEXT*)chaos_thread_data->chaos_test_context;

    srand((unsigned int)time(NULL));

    while (InterlockedAdd(&chaos_test_context->done, 0) != 1)
    {
        // perform one of the several actions
        CHAOS_TEST_ACTION action = (CHAOS_TEST_ACTION)(rand() * ((MU_COUNT_ARG(CHAOS_TEST_ACTION_VALUES)) - 1) / RAND_MAX);
        int item_index = (rand() * (CHAOS_ITEM_COUNT - 1)) / RAND_MAX;
        int64_t seq_no;

        switch (action)
        {
        default:
            LogError("Invalid action: %d", action);
            break;

        case CHAOS_TEST_ACTION_INSERT:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_INSERTING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                chaos_test_context->items[item_index].item = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_OK, clds_hash_table_insert(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, &seq_no));

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE_KEY_VALUE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, clds_hash_table_delete_key_value(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, &seq_no));

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_OK, clds_hash_table_delete(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &seq_no));

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_REMOVE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* removed_item;

                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_OK, clds_hash_table_remove(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, removed_item);

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
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

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
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

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_NOT_FOUND, clds_hash_table_delete(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &seq_no));

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_REMOVE_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_DELETING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* removed_item;
                ASSERT_ARE_EQUAL(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_NOT_FOUND, clds_hash_table_remove(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;
        
        case CHAOS_TEST_ACTION_FIND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_FINDING, TEST_HASH_TABLE_ITEM_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* found_item = clds_hash_table_find(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NOT_NULL(found_item);

                CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, found_item);

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_USED);
            }
            break;
        
        case CHAOS_TEST_ACTION_FIND_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_HASH_TABLE_ITEM_FINDING, TEST_HASH_TABLE_ITEM_NOT_USED, &item_index))
            {
                CLDS_HASH_TABLE_ITEM* found_item = clds_hash_table_find(chaos_test_context->hash_table, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NULL(found_item);

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
            }
            break;
        }
    }

    result = 0;
    ThreadAPI_Exit(result);
    return result;
}

TEST_FUNCTION(clds_hash_table_chaos_knight_test)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    volatile int64_t sequence_number = -1;
    size_t i;

    CHAOS_TEST_CONTEXT* chaos_test_context = (CHAOS_TEST_CONTEXT*)malloc(sizeof(CHAOS_TEST_CONTEXT) + (sizeof(CHAOS_TEST_ITEM_DATA) * CHAOS_ITEM_COUNT));
    ASSERT_IS_NOT_NULL(chaos_test_context);

    for (i = 0; i < CHAOS_ITEM_COUNT; i++)
    {
        (void)InterlockedExchange(&chaos_test_context->items[i].item_state, TEST_HASH_TABLE_ITEM_NOT_USED);
    }

    chaos_test_context->hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 256, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(chaos_test_context->hash_table);

    (void)InterlockedExchange(&chaos_test_context->done, 0);

    // start threads doing random things on the list
    CHAOS_THREAD_DATA* chaos_thread_data = (CHAOS_THREAD_DATA*)malloc(sizeof(CHAOS_THREAD_DATA) * CHAOS_THREAD_COUNT);
    ASSERT_IS_NOT_NULL(chaos_thread_data);

    for (i = 0; i < CHAOS_THREAD_COUNT; i++)
    {
        chaos_thread_data[i].chaos_test_context = chaos_test_context;

        chaos_thread_data[i].clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
        ASSERT_IS_NOT_NULL(chaos_thread_data[i].clds_hazard_pointers_thread);

        if (ThreadAPI_Create(&chaos_thread_data[i].thread_handle, chaos_thread, &chaos_thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning test thread");
            break;
        }
    }

    double start_time = timer_global_get_elapsed_ms();

    // act
    while (timer_global_get_elapsed_ms() - start_time < CHAOS_TEST_RUNTIME)
    {
        LogInfo("Test ran for %.02f seconds", (timer_global_get_elapsed_ms() - start_time) / 1000);
        ThreadAPI_Sleep(1000);
    }

    (void)InterlockedExchange(&chaos_test_context->done, 1);

    // assert
    for (i = 0; i < CHAOS_THREAD_COUNT; i++)
    {
        int dont_care;
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(chaos_thread_data[i].thread_handle, &dont_care), "Thread %zu failed to join", i);
    }

    // cleanup
    free(chaos_thread_data);
    clds_hash_table_destroy(chaos_test_context->hash_table);
    free(chaos_test_context);
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(clds_hash_table_inttests)
