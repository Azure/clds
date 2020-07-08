// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "clds/clds_hash_table.h"
#include "azure_c_pal/threadapi.h"
#include "azure_c_pal/timer.h"
#include "azure_c_logging/xlogging.h"
#include "windows.h"
#include "clds_hash_table_perf.h"
#include "MurmurHash2.h"
#include "azure_c_util/uuid.h"

#define THREAD_COUNT 8
#define INSERT_COUNT 100000

typedef struct TEST_ITEM_TAG
{
    char key[64];
} TEST_ITEM;

DECLARE_HASH_TABLE_NODE_TYPE(TEST_ITEM)

typedef struct THREAD_DATA_TAG
{
    CLDS_HASH_TABLE_HANDLE hash_table;
    CLDS_HASH_TABLE_ITEM* items[INSERT_COUNT];
    double runtime;
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread;
} THREAD_DATA;

static int insert_thread(void* arg)
{
    size_t i;
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    double start_time = timer_global_get_elapsed_ms();
    for (i = 0; i < INSERT_COUNT; i++)
    {
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, thread_data->items[i]);
        if (clds_hash_table_insert(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, test_item->key, thread_data->items[i], NULL) != 0)
        {
            LogError("Error inserting");
            break;
        }
    }

    if (i < INSERT_COUNT)
    {
        LogError("Error in test");
        result = MU_FAILURE;
    }
    else
    {
        thread_data->runtime = timer_global_get_elapsed_ms() - start_time;
        result = 0;
    }

    ThreadAPI_Exit(result);
    return result;
}

static int delete_thread(void* arg)
{
    size_t i;
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    double start_time = timer_global_get_elapsed_ms();
    for (i = 0; i < INSERT_COUNT; i++)
    {
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, thread_data->items[i]);
        if (clds_hash_table_delete(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, test_item->key, NULL) != CLDS_HASH_TABLE_DELETE_OK)
        {
            LogError("Error deleting");
            break;
        }
    }

    if (i < INSERT_COUNT)
    {
        LogError("Error in test");
        result = MU_FAILURE;
    }
    else
    {
        thread_data->runtime = timer_global_get_elapsed_ms() - start_time;
        result = 0;
    }

    ThreadAPI_Exit(result);
    return result;
}

static int find_thread(void* arg)
{
    size_t i;
    THREAD_DATA* thread_data = (THREAD_DATA*)arg;
    int result;

    double start_time = timer_global_get_elapsed_ms();
    for (i = 0; i < INSERT_COUNT; i++)
    {
        TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, thread_data->items[i]);
        CLDS_HASH_TABLE_ITEM* found_item = clds_hash_table_find(thread_data->hash_table, thread_data->clds_hazard_pointers_thread, test_item->key);
        if (found_item == NULL)
        {
            LogError("Error finding");
            break;
        }
        else
        {
            CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, found_item);
        }
    }

    if (i < INSERT_COUNT)
    {
        LogError("Error in test");
        result = MU_FAILURE;
    }
    else
    {
        thread_data->runtime = timer_global_get_elapsed_ms() - start_time;
        result = 0;
    }

    ThreadAPI_Exit(result);
    return result;
}

static uint64_t test_compute_hash(void* key)
{
    const char* test_key = (const char*)key;
    return (uint64_t)MurmurHash2(test_key, (int)strlen(test_key), 0);
}

static int key_compare_func(void* key_1, void* key_2)
{
    return strcmp((const char*)key_1, (const char*)key_2);
}

int clds_hash_table_perf_main(void)
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HASH_TABLE_HANDLE hash_table;
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
        int64_t sequence_number;
        hash_table = clds_hash_table_create(test_compute_hash, key_compare_func, 1024, clds_hazard_pointers, &sequence_number, NULL, NULL);
        if (hash_table == NULL)
        {
            LogError("Error creating hash table");
        }
        else
        {
            LogInfo("Generating data");

            thread_data = (THREAD_DATA*)malloc(sizeof(THREAD_DATA) * THREAD_COUNT);
            if (thread_data == NULL)
            {
                LogError("Error allocating thread data array");
            }
            else
            {
                for (i = 0; i < THREAD_COUNT; i++)
                {
                    thread_data[i].clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
                    thread_data[i].hash_table = hash_table;

                    for (j = 0; j < INSERT_COUNT; j++)
                    {
                        thread_data[i].items[j] = CLDS_HASH_TABLE_NODE_CREATE(TEST_ITEM, NULL, NULL);
                        if (thread_data[i].items[j] == NULL)
                        {
                            LogError("Error allocating test item");
                            break;
                        }
                        else
                        {
                            UUID_T uuid;
                            if (UUID_generate(&uuid) != 0)
                            {
                                LogError("Cannot get uuid");
                                break;
                            }
                            else
                            {
                                char* uuid_string = UUID_to_string(&uuid);
                                if (uuid_string == NULL)
                                {
                                    LogError("Cannot get uuid string");
                                }
                                else
                                {
                                    TEST_ITEM* test_item = CLDS_HASH_TABLE_GET_VALUE(TEST_ITEM, thread_data[i].items[j]);
                                    (void)sprintf(test_item->key, "%s", uuid_string);
                                    free(uuid_string);
                                }
                            }
                        }
                    }

                    if (j < INSERT_COUNT)
                    {
                        size_t k;

                        for (k = 0; k < j; k++)
                        {
                            CLDS_HASH_TABLE_NODE_RELEASE(TEST_ITEM, thread_data[i].items[k]);
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

                    LogInfo("Starting test");

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
                        double runtime = 0.0;

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
                            LogInfo("Insert test done in %.02f ms, %.02f inserts/s/thread, %.02f inserts/s on all threads",
                                runtime,
                                ((double)THREAD_COUNT * (double)INSERT_COUNT) / (double)runtime * 1000.0,
                                ((double)THREAD_COUNT * (double)INSERT_COUNT) / ((double)runtime / THREAD_COUNT) * 1000.0);

                            // find test

                            for (i = 0; i < THREAD_COUNT; i++)
                            {
                                if (ThreadAPI_Create(&threads[i], find_thread, &thread_data[i]) != THREADAPI_OK)
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
                                    LogInfo("Find test done in %.02f ms, %.02f finds/s/thread, %.02f finds/s on all threads",
                                        runtime,
                                        ((double)THREAD_COUNT * (double)INSERT_COUNT) / (double)runtime * 1000.0,
                                        ((double)THREAD_COUNT * (double)INSERT_COUNT) / ((double)runtime / THREAD_COUNT) * 1000.0);
                                }

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
                                        LogInfo("Delete test done in %.02f ms, %.02f deletes/s/thread, %.02f deletes/s on all threads",
                                            runtime,
                                            ((double)THREAD_COUNT * (double)INSERT_COUNT) / (double)runtime * 1000.0,
                                            ((double)THREAD_COUNT * (double)INSERT_COUNT) / ((double)runtime / THREAD_COUNT) * 1000.0);
                                    }
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

            clds_hash_table_destroy(hash_table);
        }

        clds_hazard_pointers_destroy(clds_hazard_pointers);
    }

    return 0;
}
