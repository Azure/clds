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
#include "clds/clds_hash_table.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;

TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_INSERT_RESULT, CLDS_HASH_TABLE_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_DELETE_RESULT, CLDS_HASH_TABLE_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_REMOVE_RESULT, CLDS_HASH_TABLE_REMOVE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_HASH_TABLE_SET_VALUE_RESULT, CLDS_HASH_TABLE_SET_VALUE_RESULT_VALUES);

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

typedef struct TEST_ITEM_TAG
{
    uint32_t key;
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

static void test_item_cleanup_func(void* context, struct CLDS_HASH_TABLE_ITEM_TAG* item)
{
    (void)context;
    (void)item;
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

    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    TEST_MUTEX_RELEASE(test_serialize_mutex);
}

TEST_FUNCTION(clds_hash_table_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
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
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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
}

TEST_FUNCTION(clds_hash_table_delete_after_set_value_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    volatile int64_t sequence_number = 45;
    CLDS_HASH_TABLE_HANDLE hash_table = clds_hash_table_create(test_compute_hash, test_key_compare, 1, hazard_pointers, &sequence_number, test_skipped_seq_no_cb, (void*)0x5556);
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

END_TEST_SUITE(clds_hash_table_inttests)
