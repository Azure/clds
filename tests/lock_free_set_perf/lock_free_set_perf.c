// Copyright (c) Microsoft. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "clds/lock_free_set.h"
#include "azure_c_util/threadapi.h"
#include "azure_c_util/xlogging.h"
#include "azure_c_util/tickcounter.h"

#define THREAD_COUNT 4
#define INSERT_COUNT 1000000

typedef struct TEST_SET_ITEM_TAG
{
    LOCK_FREE_SET_ITEM item;
} TEST_SET_ITEM;

typedef struct THREAD_DATA_TAG
{
    LOCK_FREE_SET_HANDLE set;
    TEST_SET_ITEM* items[INSERT_COUNT];
    TICK_COUNTER_HANDLE tick_counter;
    tickcounter_ms_t runtime;
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
                if (lock_free_set_insert(thread_data->set, &thread_data->items[i]->item) != 0)
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

static int remove_thread(void* arg)
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
                if (lock_free_set_remove(thread_data->set, &thread_data->items[i]->item) != 0)
                {
                    LogError("Error removing item %zu", i);
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

static int insert_and_remove_thread(void* arg)
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
                if (lock_free_set_insert(thread_data->set, &thread_data->items[i]->item) != 0)
                {
                    LogError("Error inserting item %zu", i);
                    break;
                }
                if (lock_free_set_remove(thread_data->set, &thread_data->items[i]->item) != 0)
                {
                    LogError("Error removing item %zu", i);
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

static void set_item_cleanup(void* context, LOCK_FREE_SET_ITEM* item)
{
    (void)context;
    (void)item;
}

int lock_free_set_perf_main(void)
{
    LOCK_FREE_SET_HANDLE set;
    THREAD_HANDLE threads[THREAD_COUNT];
    THREAD_DATA* thread_data;
    size_t i;
    size_t j;

    set = lock_free_set_create();
    if (set == NULL)
    {
        LogError("Error creating set");
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
                thread_data[i].set = set;

                for (j = 0; j < INSERT_COUNT; j++)
                {
                    thread_data[i].items[j] = (TEST_SET_ITEM*)malloc(sizeof(TEST_SET_ITEM));
                    if (thread_data[i].items == NULL)
                    {
                        LogError("Error allocating test item");
                        break;
                    }
                }

                if (j < INSERT_COUNT)
                {
                    size_t k;

                    for (k = 0; k < j; k++)
                    {
                        free(thread_data[i].items[k]);
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

                        // remove test

                        for (i = 0; i < THREAD_COUNT; i++)
                        {
                            if (ThreadAPI_Create(&threads[i], remove_thread, &thread_data[i]) != THREADAPI_OK)
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
                                LogInfo("Remove test done in %zu ms, %02f removes/s/thread, %02f removes/s on all threads",
                                    (size_t)runtime,
                                    ((double)THREAD_COUNT * (double)INSERT_COUNT) / (double)runtime * 1000.0,
                                    ((double)THREAD_COUNT * (double)INSERT_COUNT) / ((double)runtime / THREAD_COUNT) * 1000.0);

                                // insert/remove test

                                for (i = 0; i < THREAD_COUNT; i++)
                                {
                                    if (ThreadAPI_Create(&threads[i], insert_and_remove_thread, &thread_data[i]) != THREADAPI_OK)
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
                                        LogInfo("Insert&remove test done in %zu ms, %02f cycles/s/thread, %02f cycles/s on all threads",
                                            (size_t)runtime,
                                            ((double)THREAD_COUNT * (double)INSERT_COUNT) / (double)runtime * 1000.0,
                                            ((double)THREAD_COUNT * (double)INSERT_COUNT) / ((double)runtime / THREAD_COUNT) * 1000.0);
                                    }
                                }
                            }
                        }
                    }
                }

                for (i = 0; i < THREAD_COUNT; i++)
                {
                    for (j = 0; j < INSERT_COUNT; j++)
                    {
                        free(thread_data[i].items[j]);
                    }
                }

                free(thread_data);
            }
        }

        lock_free_set_destroy(set, set_item_cleanup, NULL);
    }

    return 0;
}
