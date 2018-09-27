// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cinttypes>
#else
#include <stdlib.h>
#include <inttypes.h>
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

#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_sorted_list.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;

TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT_VALUES);

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

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

static void test_skipped_seq_no_cb(void* context, int64_t skipped_sequence_no)
{
    LogInfo("Skipped seq no.: %" PRIu64, skipped_sequence_no);
    (void)context;
    (void)skipped_sequence_no;
}

static void test_item_cleanup_func(void* context, CLDS_SORTED_LIST_ITEM* item)
{
    (void)context;
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

    umock_c_reset_all_calls();
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

    // assert
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
    free((void*)sequence_no_map.sequence_no_map);
    free(items);
}

END_TEST_SUITE(clds_sorted_list_inttests)
