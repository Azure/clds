// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
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

#define ENABLE_MOCKS

#include "azure_c_shared_utility/gballoc.h"

#undef ENABLE_MOCKS

#include "clds/clds_hazard_pointers.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;
static TEST_MUTEX_HANDLE g_dllByDll;

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

MOCK_FUNCTION_WITH_CODE(, void, test_reclaim_func, void*, node)
MOCK_FUNCTION_END()

BEGIN_TEST_SUITE(clds_hazard_pointers_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);

    test_serialize_mutex = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(test_serialize_mutex);

    umock_c_init(on_umock_c_error);

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, real_free);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    TEST_MUTEX_DESTROY(test_serialize_mutex);
    TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
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

/* clds_hazard_pointers_create */

TEST_FUNCTION(clds_hazard_pointers_create_succeeds)
{
    // arrange

    // act
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create(test_reclaim_func);

    // assert
    ASSERT_IS_NOT_NULL(clds_hazard_pointers);

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/* clds_hazard_pointers_destroy */

TEST_FUNCTION(clds_hazard_pointers_destroy_frees_the_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create(test_reclaim_func);

    // act
    clds_hazard_pointers_destroy(clds_hazard_pointers);

    // assert
}

/* clds_hazard_pointers_register_thread */

TEST_FUNCTION(clds_hazard_pointers_register_thread_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create(test_reclaim_func);

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);

    // assert
    ASSERT_IS_NOT_NULL(clds_hazard_pointers_thread);
    
    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/* clds_hazard_pointers_unregister_thread */

TEST_FUNCTION(clds_hazard_pointers_unregister_thread_frees_the_thread_specific_data)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create(test_reclaim_func);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);

    // act
    clds_hazard_pointers_unregister_thread(clds_hazard_pointers_thread);

    // assert

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/* clds_hazard_pointers_acquire */

TEST_FUNCTION(clds_hazard_pointer_acquire_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create(test_reclaim_func);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer;
    void* pointer_1 = (void*)0x4242;

    // act
    hazard_pointer = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, pointer_1);

    // assert
    ASSERT_IS_NOT_NULL(hazard_pointer);

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/* clds_hazard_pointers_release */

TEST_FUNCTION(clds_hazard_pointers_release_releases_the_pointer)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create(test_reclaim_func);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer;
    void* pointer_1 = (void*)0x4242;
    hazard_pointer = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, pointer_1);

    // act
    clds_hazard_pointers_release(hazard_pointer);

    // assert
    ASSERT_IS_NOT_NULL(hazard_pointer);

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

TEST_FUNCTION(clds_hazard_pointers_reclaim_with_a_hazard_pointer_set_does_not_reclaim_the_pointer)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create(test_reclaim_func);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer;
    void* pointer_1 = (void*)0x4242;
    hazard_pointer = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, pointer_1);

    // act
    if (clds_hazard_pointers_reclaim(hazard_pointer) != 0)
    {

    }
    else
    {

    }

    // assert
    ASSERT_IS_NOT_NULL(hazard_pointer);

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

END_TEST_SUITE(clds_hazard_pointers_unittests)
