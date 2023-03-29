// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "real_gballoc_ll.h"
void* real_malloc(size_t size)
{
    return real_gballoc_ll_malloc(size);
}

void real_free(void* ptr)
{
    real_gballoc_ll_free(ptr);
}

#include "umock_c/umock_c.h"

#define ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"

#include "clds/clds_hazard_pointers.h"

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

MOCK_FUNCTION_WITH_CODE(, void, test_reclaim_func, void*, node)
MOCK_FUNCTION_END()

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{

    ASSERT_ARE_EQUAL(int, 0, real_gballoc_hl_init(NULL, NULL));

    umock_c_init(on_umock_c_error);

    REGISTER_GBALLOC_HL_GLOBAL_MOCK_HOOK();
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    real_gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

/* clds_hazard_pointers_create */

TEST_FUNCTION(clds_hazard_pointers_create_succeeds)
{
    // arrange

    // act
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();

    // assert
    ASSERT_IS_NOT_NULL(clds_hazard_pointers);

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

/* clds_hazard_pointers_destroy */

TEST_FUNCTION(clds_hazard_pointers_destroy_frees_the_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();

    // act
    clds_hazard_pointers_destroy(clds_hazard_pointers);

    // assert
}

/* clds_hazard_pointers_register_thread */

TEST_FUNCTION(clds_hazard_pointers_register_thread_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();

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
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
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
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
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
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer;
    void* pointer_1 = (void*)0x4242;
    hazard_pointer = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, pointer_1);

    // act
    clds_hazard_pointers_release(clds_hazard_pointers_thread, hazard_pointer);

    // assert
    ASSERT_IS_NOT_NULL(hazard_pointer);

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

TEST_FUNCTION(clds_hazard_pointers_reclaim_with_a_hazard_pointer_set_does_not_reclaim_the_pointer)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    (void)clds_hazard_pointers_set_reclaim_threshold(clds_hazard_pointers, 1);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_pointer;
    void* pointer_1 = (void*)0x4242;
    hazard_pointer = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, pointer_1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc_2(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, pointer_1, test_reclaim_func);
    
    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_release(clds_hazard_pointers_thread, hazard_pointer);
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

TEST_FUNCTION(clds_hazard_pointers_reclaim_with_a_pointer_that_is_not_acquired_reclaims_the_memory)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_create();
    (void)clds_hazard_pointers_set_reclaim_threshold(clds_hazard_pointers, 1);
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = clds_hazard_pointers_register_thread(clds_hazard_pointers);
    void* pointer_1 = (void*)0x4242;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(malloc_2(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_reclaim_func(pointer_1));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, pointer_1, test_reclaim_func);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(clds_hazard_pointers);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
