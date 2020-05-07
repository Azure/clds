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

#include "azure_macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "windows.h"
#include "azure_c_util/timer.h"
#include "azure_c_util/gballoc.h"
#include "azure_c_util/threadapi.h"
#include "azure_c_util/xlogging.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_sorted_list.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;

#define XTEST_FUNCTION(A) void A(void)

TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);

typedef struct TEST_ITEM_TAG
{
    uint32_t key;
} TEST_ITEM;

DECLARE_SORTED_LIST_NODE_TYPE(TEST_ITEM)

static void* test_get_item_key(void* context, struct CLDS_SORTED_LIST_ITEM_TAG* item)
{
    TEST_ITEM* test_item = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    (void)context;
    return (void*)(uintptr_t)test_item->key;
}

static int test_key_compare(void* context, void* key1, void* key2)
{
    int result;

    (void)context;
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

static void* test_get_item_key_with_sleep(void* context, struct CLDS_SORTED_LIST_ITEM_TAG* item)
{
    TEST_ITEM* test_item = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    uint32_t sleep_ms = *((uint32_t*)context);
    if (sleep_ms > 0)
    {
        ThreadAPI_Sleep(sleep_ms);
    }
    return (void*)(uintptr_t)test_item->key;
}

static void test_skipped_seq_no_cb(void* context, int64_t skipped_sequence_no)
{
    (void)context;
    (void)skipped_sequence_no;
}

static void test_item_cleanup_func(void* context, CLDS_SORTED_LIST_ITEM* item)
{
    (void)context;
    (void)item;
}

static void test_item_cleanup_func_with_sleep(void* context, CLDS_SORTED_LIST_ITEM* item)
{
    uint32_t sleep_ms = *((uint32_t*)context);
    if (sleep_ms > 0)
    {
        ThreadAPI_Sleep(sleep_ms);
    }
    (void)item;
}

typedef struct SEQUENCE_NO_MAP_TAG
{
    volatile LONG* sequence_no_map;
    size_t sequence_no_map_entry_count;
    volatile int64_t max_seq_no;
} SEQUENCE_NO_MAP;

typedef struct THREAD_DATA_TAG
{
    CLDS_SORTED_LIST_HANDLE sorted_list;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;
    SEQUENCE_NO_MAP* sequence_no_map;
    void* context;
} THREAD_DATA;

typedef struct LOCK_WRITE_THREAD_DATA_TAG
{
    CLDS_SORTED_LIST_HANDLE sorted_list;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;
    volatile LONG lock_should_be_unblocked;
} LOCK_WRITE_THREAD_DATA;

typedef struct CHAOS_TEST_ITEM_DATA_TAG
{
    CLDS_SORTED_LIST_ITEM* item;
    volatile LONG item_state;
} CHAOS_TEST_ITEM_DATA;

typedef struct CHAOS_TEST_CONTEXT_TAG
{
    volatile LONG done;
    CLDS_SORTED_LIST_HANDLE sorted_list;
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

#define CHAOS_THREAD_COUNT  16
#define CHAOS_ITEM_COUNT    10000
#define CHAOS_TEST_RUNTIME  300000

#define TEST_LIST_ITEM_STATE_VALUES \
    TEST_LIST_ITEM_NOT_USED, \
    TEST_LIST_ITEM_INSERTING, \
    TEST_LIST_ITEM_USED, \
    TEST_LIST_ITEM_DELETING, \
    TEST_LIST_ITEM_INSERTING_AGAIN, \
    TEST_LIST_ITEM_FINDING, \
    TEST_LIST_ITEM_SETTING_VALUE

MU_DEFINE_ENUM(TEST_LIST_ITEM_STATE, TEST_LIST_ITEM_STATE_VALUES);

#define CHAOS_TEST_ACTION_VALUES \
    CHAOS_TEST_ACTION_INSERT, \
    CHAOS_TEST_ACTION_DELETE_ITEM, \
    CHAOS_TEST_ACTION_DELETE_KEY, \
    CHAOS_TEST_ACTION_REMOVE_KEY, \
    CHAOS_TEST_ACTION_INSERT_KEY_TWICE, \
    CHAOS_TEST_ACTION_DELETE_KEY_NOT_FOUND, \
    CHAOS_TEST_ACTION_REMOVE_KEY_NOT_FOUND, \
    CHAOS_TEST_ACTION_FIND, \
    CHAOS_TEST_ACTION_FIND_NOT_FOUND, \
    CHAOS_TEST_ACTION_SET_VALUE, \
    CHAOS_TEST_ACTION_SET_VALUE_SAME_ITEM, \
    CHAOS_TEST_ACTION_REMOVE_AND_SET, \
    CHAOS_TEST_ACTION_REMOVE_AND_INSERT

MU_DEFINE_ENUM_WITHOUT_INVALID(CHAOS_TEST_ACTION, CHAOS_TEST_ACTION_VALUES);

BEGIN_TEST_SUITE(clds_sorted_list_inttests)

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

TEST_FUNCTION(clds_sorted_list_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = 45;

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);

    // assert
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

#define ITEM_COUNT 100
#define THREAD_COUNT 10

static void add_sequence_no_to_map(SEQUENCE_NO_MAP* sequence_no_map, int64_t seq_no)
{
    do
    {
        int64_t current_max_seq_no = InterlockedAdd64(&sequence_no_map->max_seq_no, 0);
        if (seq_no == current_max_seq_no + 1)
        {
            if (InterlockedCompareExchange64(&sequence_no_map->max_seq_no, seq_no, current_max_seq_no) == current_max_seq_no)
            {
                break;
            }
        }
    } while (1);
}

static int delete_thread(void* arg)
{
    size_t i;
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;
    CLDS_SORTED_LIST_ITEM** items = (CLDS_SORTED_LIST_ITEM**)thread_data->context;

    for (i = 0; i < ITEM_COUNT; i++)
    {
        int64_t delete_seq_no;
        CLDS_SORTED_LIST_DELETE_RESULT delete_result = clds_sorted_list_delete_item(thread_data->sorted_list, thread_data->clds_hazard_pointers_thread, items[i], &delete_seq_no);

        if (delete_result == CLDS_SORTED_LIST_DELETE_ERROR)
        {
            LogError("Error deleting");
            break;
        }
        else if (delete_result == CLDS_SORTED_LIST_DELETE_OK)
        {
            // set the flag in the seen sequence no. array
            add_sequence_no_to_map(thread_data->sequence_no_map, delete_seq_no);
        }
    }

    result = 0;
    ThreadAPI_Exit(result);
    return result;
}

TEST_FUNCTION(clds_sorted_list_contended_delete_test)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;
    size_t i;
    size_t j;
    CLDS_SORTED_LIST_ITEM** items = (CLDS_SORTED_LIST_ITEM**)malloc(sizeof(CLDS_SORTED_LIST_ITEM*) * ITEM_COUNT);
    THREAD_DATA thread_data[THREAD_COUNT];
    THREAD_HANDLE threads[THREAD_COUNT];
    SEQUENCE_NO_MAP sequence_no_map;

    sequence_no_map.sequence_no_map = (volatile LONG*)malloc(sizeof(volatile LONG) * ITEM_COUNT * 100);
    sequence_no_map.sequence_no_map_entry_count = ITEM_COUNT * 100;
    sequence_no_map.max_seq_no = -1;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    // insert a number of items ...
    for (i = 0; i < ITEM_COUNT; i++)
    {
        items[i] = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
        TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, items[i]);
        int64_t insert_seq_no;

        item_payload->key = 0x42 + (uint32_t)i;

        (void)clds_sorted_list_insert(list, hazard_pointers_thread, items[i], &insert_seq_no);

        add_sequence_no_to_map(&sequence_no_map, insert_seq_no);
    }

    // act
    // assert
    // .. and spin multiple threads that try to delete the same items
    for (i = 0; i < THREAD_COUNT; i++)
    {
        thread_data[i].context = items;
        thread_data[i].sequence_no_map = &sequence_no_map;
        thread_data[i].sorted_list = list;
        thread_data[i].clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
        ASSERT_IS_NOT_NULL(thread_data[i].clds_hazard_pointers_thread);

        if (ThreadAPI_Create(&threads[i], delete_thread, &thread_data[i]) != THREADAPI_OK)
        {
            ASSERT_FAIL("Error spawning test thread");
            break;
        }
    }

    if (i < THREAD_COUNT)
    {
        for (j = 0; j < i; j++)
        {
            int dont_care;
            (void)ThreadAPI_Join(threads[j], &dont_care);
        }
    }
    else
    {
        for (i = 0; i < THREAD_COUNT; i++)
        {
            int thread_result;
            (void)ThreadAPI_Join(threads[i], &thread_result);
            ASSERT_ARE_EQUAL(int, 0, thread_result);
        }
    }

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
    free((void*)sequence_no_map.sequence_no_map);
    free(items);
}

static int single_insert_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;
    CLDS_SORTED_LIST_ITEM* item = (CLDS_SORTED_LIST_ITEM*)thread_data->context;

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(thread_data->sorted_list, thread_data->clds_hazard_pointers_thread, item, &insert_seq_no);

    if (insert_result != CLDS_SORTED_LIST_INSERT_OK)
    {
        LogError("Error inserting");
        result = MU_FAILURE;
    }
    else
    {
        result = 0;
    }

    ThreadAPI_Exit(result);
    return result;
}

/*Tests_SRS_CLDS_SORTED_LIST_42_031: [ clds_sorted_list_lock_writes shall increment a counter to lock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_034: [ clds_sorted_list_lock_writes shall decrement a counter to unlock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_001: [ clds_sorted_list_insert shall try the following until it acquires a write lock for the list: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_002: [ clds_sorted_list_insert shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_003: [ If the counter to lock the list for writes is non-zero then: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_004: [ clds_sorted_list_insert shall decrement the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_005: [ clds_sorted_list_insert shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
TEST_FUNCTION(clds_sorted_list_insert_blocks_when_write_lock)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    THREAD_DATA thread_data;
    THREAD_HANDLE thread;

    thread_data.context = item;
    thread_data.sorted_list = list;
    thread_data.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data.clds_hazard_pointers_thread);

    // act
    clds_sorted_list_lock_writes(list);

    if (ThreadAPI_Create(&thread, single_insert_thread, &thread_data) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning test thread");
    }

    ThreadAPI_Sleep(5000);

    // assert

    // Make sure the insert is still blocked, find fails
    CLDS_SORTED_LIST_ITEM* find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NULL(find_result);

    // Unlock will allow the insert to complete
    clds_sorted_list_unlock_writes(list);

    int thread_result;
    (void)ThreadAPI_Join(thread, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // After insert
    find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NOT_NULL(find_result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, find_result);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

static int single_delete_item_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;
    CLDS_SORTED_LIST_ITEM* item = (CLDS_SORTED_LIST_ITEM*)thread_data->context;

    int64_t delete_seq_no;
    CLDS_SORTED_LIST_DELETE_RESULT delete_result = clds_sorted_list_delete_item(thread_data->sorted_list, thread_data->clds_hazard_pointers_thread, item, &delete_seq_no);

    if (delete_result != CLDS_SORTED_LIST_DELETE_OK)
    {
        LogError("Error deleting");
        result = MU_FAILURE;
    }
    else
    {
        result = 0;
    }

    ThreadAPI_Exit(result);
    return result;
}

/*Tests_SRS_CLDS_SORTED_LIST_42_031: [ clds_sorted_list_lock_writes shall increment a counter to lock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_034: [ clds_sorted_list_lock_writes shall decrement a counter to unlock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_006: [ clds_sorted_list_delete_item shall try the following until it acquires a write lock for the list: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_007: [ clds_sorted_list_delete_item shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_008: [ If the counter to lock the list for writes is non-zero then: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_009: [ clds_sorted_list_delete_item shall decrement the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_010: [ clds_sorted_list_delete_item shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
TEST_FUNCTION(clds_sorted_list_delete_item_blocks_when_write_lock)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(list, hazard_pointers_thread, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, insert_result);

    THREAD_DATA thread_data;
    THREAD_HANDLE thread;

    thread_data.context = item;
    thread_data.sorted_list = list;
    thread_data.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data.clds_hazard_pointers_thread);

    // act
    clds_sorted_list_lock_writes(list);

    if (ThreadAPI_Create(&thread, single_delete_item_thread, &thread_data) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning test thread");
    }

    ThreadAPI_Sleep(5000);

    // assert

    // Make sure the delete is still blocked, find succeeds
    CLDS_SORTED_LIST_ITEM* find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NOT_NULL(find_result);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, find_result);
    find_result = NULL;

    // Unlock will allow the delete to complete
    clds_sorted_list_unlock_writes(list);

    int thread_result;
    (void)ThreadAPI_Join(thread, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // After delete, find fails
    find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NULL(find_result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

static int single_delete_key_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;
    CLDS_SORTED_LIST_ITEM* item = (CLDS_SORTED_LIST_ITEM*)thread_data->context;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);

    int64_t delete_seq_no;
    CLDS_SORTED_LIST_DELETE_RESULT delete_result = clds_sorted_list_delete_key(thread_data->sorted_list, thread_data->clds_hazard_pointers_thread, (void*)(UINT_PTR)item_payload->key, &delete_seq_no);

    if (delete_result != CLDS_SORTED_LIST_DELETE_OK)
    {
        LogError("Error deleting");
        result = MU_FAILURE;
    }
    else
    {
        result = 0;
    }

    ThreadAPI_Exit(result);
    return result;
}

/*Tests_SRS_CLDS_SORTED_LIST_42_031: [ clds_sorted_list_lock_writes shall increment a counter to lock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_034: [ clds_sorted_list_lock_writes shall decrement a counter to unlock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_012: [ clds_sorted_list_delete_key shall try the following until it acquires a write lock for the list: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_013: [ clds_sorted_list_delete_key shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_014: [ If the counter to lock the list for writes is non-zero then: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_015: [ clds_sorted_list_delete_key shall decrement the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_016: [ clds_sorted_list_delete_key shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_blocks_when_write_lock)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(list, hazard_pointers_thread, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, insert_result);

    THREAD_DATA thread_data;
    THREAD_HANDLE thread;

    thread_data.context = item;
    thread_data.sorted_list = list;
    thread_data.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data.clds_hazard_pointers_thread);

    // act
    clds_sorted_list_lock_writes(list);

    if (ThreadAPI_Create(&thread, single_delete_key_thread, &thread_data) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning test thread");
    }

    ThreadAPI_Sleep(5000);

    // assert

    // Make sure the delete is still blocked, find succeeds
    CLDS_SORTED_LIST_ITEM* find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NOT_NULL(find_result);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, find_result);
    find_result = NULL;

    // Unlock will allow the delete to complete
    clds_sorted_list_unlock_writes(list);

    int thread_result;
    (void)ThreadAPI_Join(thread, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // After delete, find fails
    find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NULL(find_result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

static int single_remove_key_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;
    CLDS_SORTED_LIST_ITEM* item = (CLDS_SORTED_LIST_ITEM*)thread_data->context;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);

    int64_t remove_seq_no;
    CLDS_SORTED_LIST_ITEM* removed_item;
    CLDS_SORTED_LIST_REMOVE_RESULT remove_result = clds_sorted_list_remove_key(thread_data->sorted_list, thread_data->clds_hazard_pointers_thread, (void*)(UINT_PTR)item_payload->key, &removed_item , &remove_seq_no);

    if (remove_result != CLDS_SORTED_LIST_REMOVE_OK)
    {
        LogError("Error removing");
        result = MU_FAILURE;
    }
    else
    {
        CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
        result = 0;
    }

    ThreadAPI_Exit(result);
    return result;
}

/*Tests_SRS_CLDS_SORTED_LIST_42_031: [ clds_sorted_list_lock_writes shall increment a counter to lock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_034: [ clds_sorted_list_lock_writes shall decrement a counter to unlock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_018: [ clds_sorted_list_remove_key shall try the following until it acquires a write lock for the list: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_019: [ clds_sorted_list_remove_key shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_020: [ If the counter to lock the list for writes is non-zero then: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_021: [ clds_sorted_list_remove_key shall decrement the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_022: [ clds_sorted_list_remove_key shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_blocks_when_write_lock)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(list, hazard_pointers_thread, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, insert_result);

    THREAD_DATA thread_data;
    THREAD_HANDLE thread;

    thread_data.context = item;
    thread_data.sorted_list = list;
    thread_data.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data.clds_hazard_pointers_thread);

    // act
    clds_sorted_list_lock_writes(list);

    if (ThreadAPI_Create(&thread, single_remove_key_thread, &thread_data) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning test thread");
    }

    ThreadAPI_Sleep(5000);

    // assert

    // Make sure the delete is still blocked, find succeeds
    CLDS_SORTED_LIST_ITEM* find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NOT_NULL(find_result);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, find_result);
    find_result = NULL;

    // Unlock will allow the delete to complete
    clds_sorted_list_unlock_writes(list);

    int thread_result;
    (void)ThreadAPI_Join(thread, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // After delete, find fails
    find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NULL(find_result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

static int single_set_value_thread(void* arg)
{
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;
    CLDS_SORTED_LIST_ITEM* item = (CLDS_SORTED_LIST_ITEM*)thread_data->context;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);

    CLDS_SORTED_LIST_ITEM* new_item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* new_item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, new_item);
    new_item_payload->key = item_payload->key;

    int64_t set_seq_no;
    CLDS_SORTED_LIST_ITEM* old_item;
    CLDS_SORTED_LIST_SET_VALUE_RESULT set_result = clds_sorted_list_set_value(thread_data->sorted_list, thread_data->clds_hazard_pointers_thread, (void*)(UINT_PTR)item_payload->key, new_item, &old_item, &set_seq_no);

    if (set_result != CLDS_SORTED_LIST_SET_VALUE_OK)
    {
        LogError("Error setting value");
        result = MU_FAILURE;
    }
    else
    {
        CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, old_item);
        result = 0;
    }

    ThreadAPI_Exit(result);
    return result;
}

/*Tests_SRS_CLDS_SORTED_LIST_42_031: [ clds_sorted_list_lock_writes shall increment a counter to lock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_034: [ clds_sorted_list_lock_writes shall decrement a counter to unlock the list for writes. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_024: [ clds_sorted_list_set_value shall try the following until it acquires a write lock for the list: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_025: [ clds_sorted_list_set_value shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_026: [ If the counter to lock the list for writes is non-zero then: ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_027: [ clds_sorted_list_set_value shall decrement the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_028: [ clds_sorted_list_set_value shall wait for the counter to lock the list for writes to reach 0 and repeat. ]*/
TEST_FUNCTION(clds_sorted_list_set_value_blocks_when_write_lock)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(list, hazard_pointers_thread, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, insert_result);

    THREAD_DATA thread_data;
    THREAD_HANDLE thread;

    thread_data.context = item;
    thread_data.sorted_list = list;
    thread_data.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data.clds_hazard_pointers_thread);

    // act
    clds_sorted_list_lock_writes(list);

    if (ThreadAPI_Create(&thread, single_set_value_thread, &thread_data) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning test thread");
    }

    ThreadAPI_Sleep(5000);

    // assert

    // Make sure the set value is still blocked, find returns original pointer
    CLDS_SORTED_LIST_ITEM* find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NOT_NULL(find_result);
    ASSERT_ARE_EQUAL(void_ptr, item, find_result);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, find_result);
    find_result = NULL;

    // Unlock will allow the set value to complete
    clds_sorted_list_unlock_writes(list);

    int thread_result;
    (void)ThreadAPI_Join(thread, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // After set value
    find_result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    ASSERT_IS_NOT_NULL(find_result);
    ASSERT_ARE_NOT_EQUAL(void_ptr, item, find_result);
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, find_result);
    find_result = NULL;

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

static int lock_write_thread(void* arg)
{
    LOCK_WRITE_THREAD_DATA* thread_data = (LOCK_WRITE_THREAD_DATA*)arg;
    int result;

    if (InterlockedAdd(&thread_data->lock_should_be_unblocked, 0) != 0)
    {
        LogError("Test error, lock_writes should be blocked before the lock call is made");
        result = MU_FAILURE;
    }
    else
    {
        clds_sorted_list_lock_writes(thread_data->sorted_list);

        if (InterlockedAdd(&thread_data->lock_should_be_unblocked, 0) == 0)
        {
            LogError("Test error, lock_writes should be unblocked when clds_sorted_list_lock_writes returns");
            result = MU_FAILURE;
        }
        else
        {
            result = 0;
        }
    }

    ThreadAPI_Exit(result);
    return result;
}

/*Tests_SRS_CLDS_SORTED_LIST_42_032: [ clds_sorted_list_lock_writes shall wait for all pending write operations to complete. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_002: [ clds_sorted_list_insert shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_051: [ clds_sorted_list_insert shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_sorted_list_lock_writes_waits_for_pending_clds_sorted_list_insert)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    // Make operations slow
    uint32_t sleep_time = 5000;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key_with_sleep, (void*)&sleep_time, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    THREAD_DATA thread_data_insert;
    THREAD_HANDLE thread_insert;

    thread_data_insert.context = item;
    thread_data_insert.sorted_list = list;
    thread_data_insert.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_insert.clds_hazard_pointers_thread);

    LOCK_WRITE_THREAD_DATA thread_data_lock;
    THREAD_HANDLE thread_lock;

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 0);
    thread_data_lock.sorted_list = list;
    thread_data_lock.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_lock.clds_hazard_pointers_thread);

    // act
    // assert
    if (ThreadAPI_Create(&thread_insert, single_insert_thread, &thread_data_insert) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning insert test thread");
    }

    // Wait long enough for the insert to start executing
    ThreadAPI_Sleep(1000);

    if (ThreadAPI_Create(&thread_lock, lock_write_thread, &thread_data_lock) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning lock test thread");
    }

    // Wait longer to make sure lock call is blocked still, but insert has not completed
    ThreadAPI_Sleep(3000);

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 1);

    int thread_result;
    (void)ThreadAPI_Join(thread_insert, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);
    (void)ThreadAPI_Join(thread_lock, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // cleanup
    sleep_time = 0;
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_032: [ clds_sorted_list_lock_writes shall wait for all pending write operations to complete. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_007: [ clds_sorted_list_delete_item shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_011: [ clds_sorted_list_delete_item shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_sorted_list_lock_writes_waits_for_pending_clds_sorted_list_delete_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    // Make deleting slow
    uint32_t sleep_time = 5000;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func_with_sleep, (void*)&sleep_time);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(list, hazard_pointers_thread, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, insert_result);

    THREAD_DATA thread_data_delete;
    THREAD_HANDLE thread_delete;

    thread_data_delete.context = item;
    thread_data_delete.sorted_list = list;
    thread_data_delete.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_delete.clds_hazard_pointers_thread);

    LOCK_WRITE_THREAD_DATA thread_data_lock;
    THREAD_HANDLE thread_lock;

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 0);
    thread_data_lock.sorted_list = list;
    thread_data_lock.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_lock.clds_hazard_pointers_thread);

    // act
    // assert
    if (ThreadAPI_Create(&thread_delete, single_delete_item_thread, &thread_data_delete) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning delete item test thread");
    }

    // Wait long enough for the insert to start executing
    ThreadAPI_Sleep(1000);

    if (ThreadAPI_Create(&thread_lock, lock_write_thread, &thread_data_lock) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning lock test thread");
    }

    // Wait longer to make sure lock call is blocked still, but delete has not completed
    ThreadAPI_Sleep(3000);

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 1);

    int thread_result;
    (void)ThreadAPI_Join(thread_delete, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);
    (void)ThreadAPI_Join(thread_lock, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // cleanup
    sleep_time = 0;
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_032: [ clds_sorted_list_lock_writes shall wait for all pending write operations to complete. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_013: [ clds_sorted_list_delete_key shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_017: [ clds_sorted_list_delete_key shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_sorted_list_lock_writes_waits_for_pending_clds_sorted_list_delete_key)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    // Make deleting slow
    uint32_t sleep_time = 5000;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func_with_sleep, (void*)&sleep_time);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(list, hazard_pointers_thread, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, insert_result);

    THREAD_DATA thread_data_delete;
    THREAD_HANDLE thread_delete;

    thread_data_delete.context = item;
    thread_data_delete.sorted_list = list;
    thread_data_delete.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_delete.clds_hazard_pointers_thread);

    LOCK_WRITE_THREAD_DATA thread_data_lock;
    THREAD_HANDLE thread_lock;

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 0);
    thread_data_lock.sorted_list = list;
    thread_data_lock.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_lock.clds_hazard_pointers_thread);

    // act
    // assert
    if (ThreadAPI_Create(&thread_delete, single_delete_key_thread, &thread_data_delete) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning delete key test thread");
    }

    // Wait long enough for the insert to start executing
    ThreadAPI_Sleep(1000);

    if (ThreadAPI_Create(&thread_lock, lock_write_thread, &thread_data_lock) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning lock test thread");
    }

    // Wait longer to make sure lock call is blocked still, but delete has not completed
    ThreadAPI_Sleep(3000);

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 1);

    int thread_result;
    (void)ThreadAPI_Join(thread_delete, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);
    (void)ThreadAPI_Join(thread_lock, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // cleanup
    sleep_time = 0;
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_032: [ clds_sorted_list_lock_writes shall wait for all pending write operations to complete. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_019: [ clds_sorted_list_remove_key shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_023: [ clds_sorted_list_remove_key shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_sorted_list_lock_writes_waits_for_pending_clds_sorted_list_remove_key)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    uint32_t sleep_time = 0;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key_with_sleep, (void*)&sleep_time, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(list, hazard_pointers_thread, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, insert_result);

    THREAD_DATA thread_data_delete;
    THREAD_HANDLE thread_delete;

    thread_data_delete.context = item;
    thread_data_delete.sorted_list = list;
    thread_data_delete.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_delete.clds_hazard_pointers_thread);

    LOCK_WRITE_THREAD_DATA thread_data_lock;
    THREAD_HANDLE thread_lock;

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 0);
    thread_data_lock.sorted_list = list;
    thread_data_lock.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_lock.clds_hazard_pointers_thread);

    // Make operations slow
    sleep_time = 5000;

    // act
    // assert
    if (ThreadAPI_Create(&thread_delete, single_remove_key_thread, &thread_data_delete) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning remove key test thread");
    }

    // Wait long enough for the remove to start executing
    ThreadAPI_Sleep(1000);

    if (ThreadAPI_Create(&thread_lock, lock_write_thread, &thread_data_lock) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning lock test thread");
    }

    // Wait longer to make sure lock call is blocked still, but remove has not completed
    ThreadAPI_Sleep(3000);

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 1);

    int thread_result;
    (void)ThreadAPI_Join(thread_delete, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);
    (void)ThreadAPI_Join(thread_lock, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // cleanup
    sleep_time = 0;
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/*Tests_SRS_CLDS_SORTED_LIST_42_032: [ clds_sorted_list_lock_writes shall wait for all pending write operations to complete. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_025: [ clds_sorted_list_set_value shall increment the count of pending write operations. ]*/
/*Tests_SRS_CLDS_SORTED_LIST_42_029: [ clds_sorted_list_set_value shall decrement the count of pending write operations. ]*/
TEST_FUNCTION(clds_sorted_list_lock_writes_waits_for_pending_clds_sorted_list_set_value)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    uint32_t sleep_time = 0;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key_with_sleep, (void*)&sleep_time, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(list, hazard_pointers_thread, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, insert_result);

    THREAD_DATA thread_data_set;
    THREAD_HANDLE thread_set;

    thread_data_set.context = item;
    thread_data_set.sorted_list = list;
    thread_data_set.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_set.clds_hazard_pointers_thread);

    LOCK_WRITE_THREAD_DATA thread_data_lock;
    THREAD_HANDLE thread_lock;

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 0);
    thread_data_lock.sorted_list = list;
    thread_data_lock.clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    ASSERT_IS_NOT_NULL(thread_data_lock.clds_hazard_pointers_thread);

    // Make operations slow
    sleep_time = 5000;

    // act
    // assert
    if (ThreadAPI_Create(&thread_set, single_set_value_thread, &thread_data_set) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning set value test thread");
    }

    // Wait long enough for the set value to start executing
    ThreadAPI_Sleep(1000);

    if (ThreadAPI_Create(&thread_lock, lock_write_thread, &thread_data_lock) != THREADAPI_OK)
    {
        ASSERT_FAIL("Error spawning lock test thread");
    }

    // Wait longer to make sure lock call is blocked still, but set value has not completed
    ThreadAPI_Sleep(3000);

    InterlockedExchange(&thread_data_lock.lock_should_be_unblocked, 1);

    int thread_result;
    (void)ThreadAPI_Join(thread_set, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);
    (void)ThreadAPI_Join(thread_lock, &thread_result);
    ASSERT_ARE_EQUAL(int, 0, thread_result);

    // cleanup
    sleep_time = 0;
    clds_sorted_list_unlock_writes(list);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

TEST_FUNCTION(clds_sorted_list_set_value_with_same_item_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    volatile int64_t sequence_number = -1;

    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;

    uint32_t sleep_time = 0;

    list = clds_sorted_list_create(hazard_pointers, test_get_item_key_with_sleep, (void*)&sleep_time, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(list);

    int64_t insert_seq_no;
    CLDS_SORTED_LIST_INSERT_RESULT insert_result = clds_sorted_list_insert(list, hazard_pointers_thread, item, &insert_seq_no);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, insert_result);

    CLDS_SORTED_LIST_ITEM* old_item;
    // act
    CLDS_SORTED_LIST_SET_VALUE_RESULT set_value_result = clds_sorted_list_set_value(list, hazard_pointers_thread, (const void*)(uintptr_t)item_payload->key, item, &old_item, &insert_seq_no);

    // assert
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, set_value_result);

    // cleanup
    clds_sorted_list_destroy(list);
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
        int item_index;
        int64_t seq_no;

        // each thread decides on a random action to execute: inserts, deletes, removes and finds
        switch (action)
        {
        default:
            LogError("Invalid action: %d", action);
            break;
        case CHAOS_TEST_ACTION_INSERT:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_INSERTING, TEST_LIST_ITEM_NOT_USED, &item_index))
            {
                chaos_test_context->items[item_index].item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                item_payload->key = item_index + 1;

                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, clds_sorted_list_insert(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, chaos_test_context->items[item_index].item, &seq_no));

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE_ITEM:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, clds_sorted_list_delete_item(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, chaos_test_context->items[item_index].item, &seq_no));
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE_KEY:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, clds_sorted_list_delete_key(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &seq_no));
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_REMOVE_KEY:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_USED, &item_index))
            {
                CLDS_SORTED_LIST_ITEM* removed_item;
            
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, clds_sorted_list_remove_key(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));
            
                CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_INSERT_KEY_TWICE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_INSERTING_AGAIN, TEST_LIST_ITEM_USED, &item_index))
            {
                CLDS_SORTED_LIST_ITEM* new_item;
                new_item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, new_item);
                item_payload->key = item_index + 1;
            
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS, clds_sorted_list_insert(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, chaos_test_context->items[item_index].item, &seq_no));
            
                CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, new_item);
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE_KEY_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_NOT_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, clds_sorted_list_delete_key(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &seq_no));
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_REMOVE_KEY_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_NOT_USED, &item_index))
            {
                CLDS_SORTED_LIST_ITEM* removed_item;
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_NOT_FOUND, clds_sorted_list_remove_key(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_FIND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_FINDING, TEST_LIST_ITEM_USED, &item_index))
            {
                CLDS_SORTED_LIST_ITEM* found_item = clds_sorted_list_find_key(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NOT_NULL(found_item);
            
                CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, found_item);
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_FIND_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_FINDING, TEST_LIST_ITEM_NOT_USED, &item_index))
            {
                CLDS_SORTED_LIST_ITEM* found_item = clds_sorted_list_find_key(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NULL(found_item);
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;

        case CHAOS_TEST_ACTION_SET_VALUE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_SETTING_VALUE, TEST_LIST_ITEM_USED, &item_index))
            {
                CLDS_SORTED_LIST_ITEM* old_item;
                chaos_test_context->items[item_index].item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                item_payload->key = item_index + 1;
            
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, clds_sorted_list_set_value(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, &old_item, &seq_no));
            
                CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, old_item);
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_SET_VALUE_SAME_ITEM:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_SETTING_VALUE, TEST_LIST_ITEM_USED, &item_index))
            {
                CLDS_SORTED_LIST_ITEM* old_item;
                CLDS_SORTED_LIST_NODE_INC_REF(TEST_ITEM, chaos_test_context->items[item_index].item);

                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, clds_sorted_list_set_value(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), chaos_test_context->items[item_index].item, &old_item, &seq_no));

                CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, old_item);

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_USED);
            }
            break;
        case CHAOS_TEST_ACTION_REMOVE_AND_SET:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_USED, &item_index))
            {
                CLDS_SORTED_LIST_ITEM* removed_item;
                CLDS_SORTED_LIST_ITEM* old_item;
            
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, clds_sorted_list_remove_key(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_SET_VALUE_RESULT, CLDS_SORTED_LIST_SET_VALUE_OK, clds_sorted_list_set_value(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), removed_item, &old_item, &seq_no));
            
                ASSERT_IS_NULL(old_item);
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_USED);
            }
            break;
        case CHAOS_TEST_ACTION_REMOVE_AND_INSERT:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_USED, &item_index))
            {
                CLDS_SORTED_LIST_ITEM* removed_item;
            
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, clds_sorted_list_remove_key(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, (void*)(uintptr_t)(item_index + 1), &removed_item, &seq_no));
                ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, clds_sorted_list_insert(chaos_test_context->sorted_list, chaos_thread_data->clds_hazard_pointers_thread, removed_item, &seq_no));
            
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_USED);
            }
            break;
        }
    }

    result = 0;
    ThreadAPI_Exit(result);
    return result;
}

TEST_FUNCTION(clds_sorted_list_chaos_knight_test)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    volatile int64_t sequence_number = -1;
    size_t i;

    CHAOS_TEST_CONTEXT* chaos_test_context = (CHAOS_TEST_CONTEXT*)malloc(sizeof(CHAOS_TEST_CONTEXT) + (sizeof(CHAOS_TEST_ITEM_DATA) * CHAOS_ITEM_COUNT));
    ASSERT_IS_NOT_NULL(chaos_test_context);

    for (i = 0; i < CHAOS_ITEM_COUNT; i++)
    {
        (void)InterlockedExchange(&chaos_test_context->items[i].item_state, TEST_LIST_ITEM_NOT_USED);
    }

    chaos_test_context->sorted_list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
    ASSERT_IS_NOT_NULL(chaos_test_context->sorted_list);

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
    clds_sorted_list_destroy(chaos_test_context->sorted_list);
    free(chaos_test_context);
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(clds_sorted_list_inttests)
