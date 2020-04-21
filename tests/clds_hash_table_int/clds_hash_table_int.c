// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cinttypes>
#else
#include <stdlib.h>
#include <inttypes.h>
#endif

#include "windows.h"
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

#include "azure_c_util/gballoc.h"
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

END_TEST_SUITE(clds_hash_table_inttests)
