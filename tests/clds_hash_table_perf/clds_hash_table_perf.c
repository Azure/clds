// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "clds/clds_hash_table.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "clds_hash_table_perf.h"

#define THREAD_COUNT 4
#define INSERT_COUNT 1000000

typedef struct TEST_ITEM_TAG
{
    char key[20];
} TEST_ITEM;

typedef struct THREAD_DATA_TAG
{
    CLDS_HASH_TABLE_HANDLE hash_table;
    TEST_ITEM* items[INSERT_COUNT];
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
        result = __FAILURE__;
    }
    else
    {
        tickcounter_ms_t start_time;

        if (tickcounter_get_current_ms(tick_counter, &start_time) != 0)
        {
            LogError("Cannot get start time");
            result = __FAILURE__;
        }
        else
        {
            tickcounter_ms_t end_time;
            for (i = 0; i < INSERT_COUNT; i++)
            {
                if (clds_hash_table_insert(thread_data->hash_table, thread_data->items[i]->key, thread_data->items[i]) != 0)
                {
                    LogError("Error inserting");
                    break;
                }
            }

            if (tickcounter_get_current_ms(tick_counter, &end_time) != 0)
            {
                LogError("Cannot get end time");
                result = __FAILURE__;
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

static uint64_t test_compute_hash(void* key)
{
    // use murmur hash at some point, there is nothing better than that :-)
    // for now simply do sum hash
    const char* test_key = (const char*)key;
    size_t key_length = strlen(test_key);
    size_t i;
    uint64_t hash = 0;

    for (i = 0; i < key_length; i++)
    {
        hash += test_key[i];
    }

    return hash;
}

int clds_hash_table_perf_main(void)
{
    CLDS_HASH_TABLE_HANDLE hash_table;
    THREAD_HANDLE threads[THREAD_COUNT];
    THREAD_DATA* thread_data;
    size_t i;
    size_t j;

    hash_table = clds_hash_table_create(test_compute_hash);
    if (hash_table == NULL)
    {
        LogError("Error creating hash table");
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
                thread_data[i].hash_table = hash_table;

                for (j = 0; j < INSERT_COUNT; j++)
                {
                    thread_data[i].items[j] = (TEST_ITEM*)malloc(sizeof(TEST_ITEM));
                    if (thread_data[i].items == NULL)
                    {
                        LogError("Error allocating test item");
                        break;
                    }
                    else
                    {
                        (void)sprintf(thread_data[i].items[j]->key, "%zu_%zu", i, j);
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

        clds_hash_table_destroy(hash_table);
    }

    return 0;
}
