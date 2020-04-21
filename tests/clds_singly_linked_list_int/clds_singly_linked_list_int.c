// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cinttypes>
#else
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#endif

#include "azure_macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "windows.h"
#include "azure_c_util/timer.h"
#include "azure_c_util/gballoc.h"
#include "azure_c_util/threadapi.h"
#include "azure_c_util/xlogging.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_singly_linked_list.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;

#define XTEST_FUNCTION(A) void A(void)

TEST_DEFINE_ENUM_TYPE(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);

typedef struct TEST_ITEM_TAG
{
    int value;
} TEST_ITEM;

DECLARE_SINGLY_LINKED_LIST_NODE_TYPE(TEST_ITEM)

static void test_item_cleanup_func(void* context, CLDS_SINGLY_LINKED_LIST_ITEM* item)
{
    (void)context;
    (void)item;
}

bool test_item_compare(void* item_compare_context, struct CLDS_SINGLY_LINKED_LIST_ITEM_TAG* item)
{
    uintptr_t item_index = (uintptr_t)item_compare_context;
    TEST_ITEM* test_item = CLDS_SINGLY_LINKED_LIST_GET_VALUE(TEST_ITEM, item);
    return item_index == test_item->value;
}

BEGIN_TEST_SUITE(clds_singly_linked_list_inttests)

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

TEST_FUNCTION(clds_singly_linked_list_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SINGLY_LINKED_LIST_HANDLE list;
    volatile int64_t sequence_number = 45;

    // act
    list = clds_singly_linked_list_create(hazard_pointers);

    // assert
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_singly_linked_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

typedef struct CHAOS_TEST_ITEM_DATA_TAG
{
    CLDS_SINGLY_LINKED_LIST_ITEM* item;
    volatile LONG item_state;
} CHAOS_TEST_ITEM_DATA;

typedef struct CHAOS_TEST_CONTEXT_TAG
{
    volatile LONG done;
    CLDS_SINGLY_LINKED_LIST_HANDLE singly_linked_list;
#ifdef _MSC_VER
    /*warning C4200: nonstandard extension used: zero-sized array in struct/union */
#pragma warning(suppress:4200)
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

#define TEST_LIST_ITEM_STATE_VALUES \
    TEST_LIST_ITEM_NOT_USED, \
    TEST_LIST_ITEM_INSERTING, \
    TEST_LIST_ITEM_USED, \
    TEST_LIST_ITEM_DELETING, \
    TEST_LIST_ITEM_INSERTING_AGAIN, \
    TEST_LIST_ITEM_FINDING

MU_DEFINE_ENUM(TEST_LIST_ITEM_STATE, TEST_LIST_ITEM_STATE_VALUES);

#define CHAOS_TEST_ACTION_VALUES \
    CHAOS_TEST_ACTION_INSERT, \
    CHAOS_TEST_ACTION_DELETE, \
    CHAOS_TEST_ACTION_DELETE_IF, \
    CHAOS_TEST_ACTION_DELETE_NOT_FOUND, \
    CHAOS_TEST_ACTION_FIND, \
    CHAOS_TEST_ACTION_FIND_NOT_FOUND

MU_DEFINE_ENUM_WITHOUT_INVALID(CHAOS_TEST_ACTION, CHAOS_TEST_ACTION_VALUES);

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

    while (InterlockedAdd(&chaos_test_context->done, 0) != 1)
    {
        // perform one of the several actions
        CHAOS_TEST_ACTION action = rand() * ((MU_COUNT_ARG(CHAOS_TEST_ACTION_VALUES)) - 1) / RAND_MAX;
        int item_index = (rand() * (CHAOS_ITEM_COUNT - 1)) / RAND_MAX;

        switch (action)
        {
        default:
            LogError("Invalid action: %d", action);
            break;

        case CHAOS_TEST_ACTION_INSERT:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_INSERTING, TEST_LIST_ITEM_NOT_USED, &item_index))
            {
                chaos_test_context->items[item_index].item = CLDS_SINGLY_LINKED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_SINGLY_LINKED_LIST_GET_VALUE(TEST_ITEM, chaos_test_context->items[item_index].item);
                item_payload->value = item_index + 1;

                ASSERT_ARE_EQUAL(int, 0, clds_singly_linked_list_insert(chaos_test_context->singly_linked_list, chaos_thread_data->clds_hazard_pointers_thread, chaos_test_context->items[item_index].item));

                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_USED);
            }
            break;

        case CHAOS_TEST_ACTION_DELETE:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_OK, clds_singly_linked_list_delete(chaos_test_context->singly_linked_list, chaos_thread_data->clds_hazard_pointers_thread, chaos_test_context->items[item_index].item));
        
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;
        
        case CHAOS_TEST_ACTION_DELETE_IF:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_USED, &item_index))
            {
                ASSERT_ARE_EQUAL(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_OK, clds_singly_linked_list_delete_if(chaos_test_context->singly_linked_list, chaos_thread_data->clds_hazard_pointers_thread, test_item_compare, (void*)(uintptr_t)(item_index + 1)));
        
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;
        
        case CHAOS_TEST_ACTION_DELETE_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_DELETING, TEST_LIST_ITEM_NOT_USED, &item_index))
            {
                CLDS_SINGLY_LINKED_LIST_ITEM* new_item = CLDS_SINGLY_LINKED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
                TEST_ITEM* item_payload = CLDS_SINGLY_LINKED_LIST_GET_VALUE(TEST_ITEM, new_item);
                item_payload->value = item_index + 1;
        
                ASSERT_ARE_EQUAL(CLDS_SINGLY_LINKED_LIST_DELETE_RESULT, CLDS_SINGLY_LINKED_LIST_DELETE_NOT_FOUND, clds_singly_linked_list_delete(chaos_test_context->singly_linked_list, chaos_thread_data->clds_hazard_pointers_thread, new_item));
        
                CLDS_SINGLY_LINKED_LIST_NODE_RELEASE(TEST_ITEM, new_item);
        
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;
        
        case CHAOS_TEST_ACTION_FIND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_FINDING, TEST_LIST_ITEM_USED, &item_index))
            {
                CLDS_SINGLY_LINKED_LIST_ITEM* found_item = clds_singly_linked_list_find(chaos_test_context->singly_linked_list, chaos_thread_data->clds_hazard_pointers_thread, test_item_compare, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NOT_NULL(found_item);
        
                CLDS_SINGLY_LINKED_LIST_NODE_RELEASE(TEST_ITEM, found_item);
        
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_USED);
            }
            break;
        
        case CHAOS_TEST_ACTION_FIND_NOT_FOUND:
            if (get_item_and_change_state(chaos_test_context->items, CHAOS_ITEM_COUNT, TEST_LIST_ITEM_FINDING, TEST_LIST_ITEM_NOT_USED, &item_index))
            {
                CLDS_SINGLY_LINKED_LIST_ITEM* found_item = clds_singly_linked_list_find(chaos_test_context->singly_linked_list, chaos_thread_data->clds_hazard_pointers_thread, test_item_compare, (void*)(uintptr_t)(item_index + 1));
                ASSERT_IS_NULL(found_item);
        
                (void)InterlockedExchange(&chaos_test_context->items[item_index].item_state, TEST_LIST_ITEM_NOT_USED);
            }
            break;
        }
    }

    result = 0;
    ThreadAPI_Exit(result);
    return result;
}

TEST_FUNCTION(clds_singly_linked_list_chaos_knight_test)
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

    chaos_test_context->singly_linked_list = clds_singly_linked_list_create(hazard_pointers);
    ASSERT_IS_NOT_NULL(chaos_test_context->singly_linked_list);

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
    clds_singly_linked_list_destroy(chaos_test_context->singly_linked_list);
    free(chaos_test_context);
    clds_hazard_pointers_destroy(hazard_pointers);
}

END_TEST_SUITE(clds_singly_linked_list_inttests)
