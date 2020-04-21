// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "clds/clds_singly_linked_list.h"
#include "azure_c_util/threadapi.h"
#include "azure_c_util/xlogging.h"
#include "azure_c_util/tickcounter.h"
#include "clds_singly_linked_list_perf.h"

#define THREAD_COUNT 10
#define INSERT_COUNT 1000

typedef struct TEST_ITEM_TAG
{
    char key[20];
} TEST_ITEM;

DECLARE_SINGLY_LINKED_LIST_NODE_TYPE(TEST_ITEM);

typedef struct THREAD_DATA_TAG
{
    CLDS_SINGLY_LINKED_LIST_HANDLE singly_linked_list;
    CLDS_SINGLY_LINKED_LIST_ITEM* items[INSERT_COUNT];
    TICK_COUNTER_HANDLE tick_counter;
    tickcounter_ms_t runtime;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;
} THREAD_DATA;

static int insert_thread(void* arg)
{
    size_t i;
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    TICK_COUNTER_HANDLE tick_counter = tickcounter_create();
    if (tick_counter == NULL)
    {
        LogError("Cannot create tick counter");
        result = MU_FAILURE;
    }
    else
    {
        tickcounter_ms_t start_time;

        if (tickcounter_get_current_ms(tick_counter, &start_time) != 0)
        {
            LogError("Cannot get start time");
            result = MU_FAILURE;
        }
        else
        {
            tickcounter_ms_t end_time;
            for (i = 0; i < INSERT_COUNT; i++)
            {
                if (clds_singly_linked_list_insert(thread_data->singly_linked_list, thread_data->clds_hazard_pointers_thread, thread_data->items[i]) != 0)
                {
                    LogError("Error inserting");
                    break;
                }
            }

            if (tickcounter_get_current_ms(tick_counter, &end_time) != 0)
            {
                LogError("Cannot get end time");
                result = MU_FAILURE;
            }
            else
            {
                thread_data->runtime = end_time - start_time;
                result = 0;
            }
        }

        tickcounter_destroy(tick_counter);
    }

    ThreadAPI_Exit(result);
    return result;
}

static int delete_thread(void* arg)
{
    size_t i;
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    TICK_COUNTER_HANDLE tick_counter = tickcounter_create();
    if (tick_counter == NULL)
    {
        LogError("Cannot create tick counter");
        result = MU_FAILURE;
    }
    else
    {
        tickcounter_ms_t start_time;

        if (tickcounter_get_current_ms(tick_counter, &start_time) != 0)
        {
            LogError("Cannot get start time");
            result = MU_FAILURE;
        }
        else
        {
            tickcounter_ms_t end_time;
            for (i = 0; i < INSERT_COUNT; i++)
            {
                if (clds_singly_linked_list_delete(thread_data->singly_linked_list, thread_data->clds_hazard_pointers_thread, thread_data->items[i]) != CLDS_SINGLY_LINKED_LIST_DELETE_OK)
                {
                    LogError("Error deleting");
                    break;
                }
            }

            if (tickcounter_get_current_ms(tick_counter, &end_time) != 0)
            {
                LogError("Cannot get end time");
                result = MU_FAILURE;
            }
            else
            {
                thread_data->runtime = end_time - start_time;
                result = 0;
            }
        }

        tickcounter_destroy(tick_counter);
    }

    ThreadAPI_Exit(result);
    return result;
}

int clds_singly_linked_list_perf_main(void)
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_SINGLY_LINKED_LIST_HANDLE singly_linked_list;
    THREAD_HANDLE threads[THREAD_COUNT];
    THREAD_DATA* thread_data;
    size_t i;
    size_t j;

    clds_hazard_pointers = clds_hazard_pointers_create();
    if (clds_hazard_pointers == NULL)
    {
        LogError("Error creating hazard pointers");
    }
    else
    {
        singly_linked_list = clds_singly_linked_list_create(clds_hazard_pointers);
        if (singly_linked_list == NULL)
        {
            LogError("Error creating singly linked list");
        }
        else
        {
            thread_data = (THREAD_DATA*)malloc(sizeof(THREAD_DATA) * THREAD_COUNT);
            if (thread_data == NULL)
            {
                LogError("Error allocating thread data array");
            }
            else
            {
                for (i = 0; i < THREAD_COUNT; i++)
                {
                    thread_data[i].singly_linked_list = singly_linked_list;
                    thread_data[i].clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
                    if (thread_data[i].clds_hazard_pointers_thread == NULL)
                    {
                        LogError("Error registering thread with harzard pointers");
                        break;
                    }
                    else
                    {
                        for (j = 0; j < INSERT_COUNT; j++)
                        {
                            thread_data[i].items[j] = CLDS_SINGLY_LINKED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
                            if (thread_data[i].items[j] == NULL)
                            {
                                LogError("Error allocating test item");
                                break;
                            }
                            else
                            {
                                TEST_ITEM* test_item = CLDS_SINGLY_LINKED_LIST_GET_VALUE(TEST_ITEM, thread_data[i].items[j]);
                                (void)sprintf(test_item->key, "%zu_%zu", i, j);
                            }
                        }

                        if (j < INSERT_COUNT)
                        {
                            size_t k;

                            for (k = 0; k < j; k++)
                            {
                                CLDS_SINGLY_LINKED_LIST_NODE_RELEASE(TEST_ITEM, thread_data[i].items[k]);
                            }
                            break;
                        }
                    }
                }

                if (i < THREAD_COUNT)
                {
                    LogError("Error creating test thread data");
                }
                else
                {
                    // insert test
                    LogInfo("Start insert test");

                    for (i = 0; i < THREAD_COUNT; i++)
                    {
                        if (ThreadAPI_Create(&threads[i], insert_thread, &thread_data[i]) != THREADAPI_OK)
                        {
                            LogError("Error spawning test thread");
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
                        bool is_error = false;
                        tickcounter_ms_t runtime = 0;

                        for (i = 0; i < THREAD_COUNT; i++)
                        {
                            int thread_result;
                            (void)ThreadAPI_Join(threads[i], &thread_result);
                            if (thread_result != 0)
                            {
                                is_error = true;
                            }
                            else
                            {
                                runtime += thread_data[i].runtime;
                            }
                        }

                        if (!is_error)
                        {
                            LogInfo("Insert test done in %zu ms, %02f inserts/s/thread, %02f inserts/s on all threads",
                                (size_t)runtime,
                                ((double)THREAD_COUNT * (double)INSERT_COUNT) / (double)runtime * 1000.0,
                                ((double)THREAD_COUNT * (double)INSERT_COUNT) / ((double)runtime / THREAD_COUNT) * 1000.0);

                            // delete test

                            for (i = 0; i < THREAD_COUNT; i++)
                            {
                                if (ThreadAPI_Create(&threads[i], delete_thread, &thread_data[i]) != THREADAPI_OK)
                                {
                                    LogError("Error spawning test thread");
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
                                is_error = false;
                                runtime = 0;

                                for (i = 0; i < THREAD_COUNT; i++)
                                {
                                    int thread_result;
                                    (void)ThreadAPI_Join(threads[i], &thread_result);
                                    if (thread_result != 0)
                                    {
                                        is_error = true;
                                    }
                                    else
                                    {
                                        runtime += thread_data[i].runtime;
                                    }
                                }

                                if (!is_error)
                                {
                                    LogInfo("Delete test done in %zu ms, %02f deletes/s/thread, %02f deletes/s on all threads",
                                        (size_t)runtime,
                                        ((double)THREAD_COUNT * (double)INSERT_COUNT) / (double)runtime * 1000.0,
                                        ((double)THREAD_COUNT * (double)INSERT_COUNT) / ((double)runtime / THREAD_COUNT) * 1000.0);
                                }
                            }
                        }
                    }

                    for (i = 0; i < THREAD_COUNT; i++)
                    {
                        clds_hazard_pointers_unregister_thread(thread_data[i].clds_hazard_pointers_thread);
                    }

                    free(thread_data);
                }
            }

            clds_singly_linked_list_destroy(singly_linked_list);
        }

        clds_hazard_pointers_destroy(clds_hazard_pointers);
    }

    return 0;
}
