// Copyright (c) Microsoft. All rights reserved.

#include <stdlib.h>
#include <stddef.h>

#include "windows.h"

#include "macro_utils/macro_utils.h"

#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"
#include "umock_c/umock_c_negative_tests.h"
#include "umock_c/umocktypes.h"
#include "umock_c/umocktypes_windows.h"

#define ENABLE_MOCKS
#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "clds/clds_hazard_pointers.h"
#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"

#include "clds/clds_hazard_pointers_thread_helper.h"

static DWORD default_tls_slot = 42;

MOCK_FUNCTION_WITH_CODE(, DWORD, mocked_TlsAlloc)
MOCK_FUNCTION_END(default_tls_slot)
MOCK_FUNCTION_WITH_CODE(, BOOL, mocked_TlsFree, DWORD, dwTlsIndex)
MOCK_FUNCTION_END(TRUE)
MOCK_FUNCTION_WITH_CODE(, LPVOID, mocked_TlsGetValue, DWORD, dwTlsIndex)
MOCK_FUNCTION_END(NULL)
MOCK_FUNCTION_WITH_CODE(, BOOL, mocked_TlsSetValue, DWORD, dwTlsIndex, LPVOID, lpTlsValue)
MOCK_FUNCTION_END(TRUE)

static CLDS_HAZARD_POINTERS_HANDLE test_hazard_pointers = (CLDS_HAZARD_POINTERS_HANDLE)0x1234;
static CLDS_HAZARD_POINTERS_THREAD_HANDLE test_hazard_pointers_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)0xabcd;

static CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE test_create(void)
{
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(mocked_TlsAlloc());

    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = clds_hazard_pointers_thread_helper_create(test_hazard_pointers);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(helper);

    umock_c_reset_all_calls();

    return helper;
}

static CLDS_HAZARD_POINTERS_THREAD_HANDLE test_get_thread(CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper)
{
    STRICT_EXPECTED_CALL(mocked_TlsGetValue(IGNORED_ARG));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_register_thread(test_hazard_pointers));
    STRICT_EXPECTED_CALL(mocked_TlsSetValue(IGNORED_ARG, IGNORED_ARG));

    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(helper);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);

    umock_c_reset_all_calls();

    return hazard_pointers_thread;
}

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, real_gballoc_hl_init(NULL, NULL));

    ASSERT_ARE_EQUAL(int, 0, umock_c_init(on_umock_c_error), "umock_c_init");
    ASSERT_ARE_EQUAL(int, 0, umocktypes_windows_register_types(), "umocktypes_windows_register_types");

    REGISTER_GBALLOC_HL_GLOBAL_MOCK_HOOK();
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(malloc, NULL);

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(mocked_TlsAlloc, TLS_OUT_OF_INDEXES);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(mocked_TlsSetValue, FALSE);
    REGISTER_GLOBAL_MOCK_RETURNS(clds_hazard_pointers_register_thread, test_hazard_pointers_thread, NULL);

    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_THREAD_HANDLE, void*);

    REGISTER_UMOCKC_PAIRED_CREATE_DESTROY_CALLS(mocked_TlsAlloc, mocked_TlsFree);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    real_gballoc_hl_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    umock_c_reset_all_calls();
    umock_c_negative_tests_init();

}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    umock_c_negative_tests_deinit();
}

//
// clds_hazard_pointers_thread_helper_create
//

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_001: [ If hazard_pointers is NULL then clds_hazard_pointers_thread_helper_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_create_with_NULL_hazard_pointers_fails)
{
    // arrange

    // act
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = clds_hazard_pointers_thread_helper_create(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(helper);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_002: [ clds_hazard_pointers_thread_helper_create shall allocate memory for the helper. ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_003: [ clds_hazard_pointers_thread_helper_create shall allocate the thread local storage slot for the hazard pointers by calling TlsAlloc. ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_004: [ clds_hazard_pointers_thread_helper_create shall succeed and return the helper. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_create_succeeds)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(mocked_TlsAlloc());

    // act
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = clds_hazard_pointers_thread_helper_create(test_hazard_pointers);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(helper);

    // cleanup
    clds_hazard_pointers_thread_helper_destroy(helper);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_005: [ If there are any errors then clds_hazard_pointers_thread_helper_create shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_create_fails_when_underlying_functions_fail)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(mocked_TlsAlloc());

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (umock_c_negative_tests_can_call_fail(i))
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = clds_hazard_pointers_thread_helper_create(test_hazard_pointers);

            // assert
            ASSERT_IS_NULL(helper);
        }
    }
}

//
// clds_hazard_pointers_thread_helper_destroy
//

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_006: [ If hazard_pointers_helper is NULL then clds_hazard_pointers_thread_helper_destroy shall return. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_destroy_with_NULL_hazard_pointers_helper_returns)
{
    // arrange

    // act
    clds_hazard_pointers_thread_helper_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_007: [ clds_hazard_pointers_thread_helper_destroy shall free the thread local storage slot by calling TlsFree. ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_008: [ clds_hazard_pointers_thread_helper_destroy shall free the helper. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_destroy_frees_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = test_create();

    STRICT_EXPECTED_CALL(mocked_TlsFree(default_tls_slot));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    clds_hazard_pointers_thread_helper_destroy(helper);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

//
// clds_hazard_pointers_thread_helper_get_thread
//

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_009: [ If hazard_pointers_helper is NULL then clds_hazard_pointers_thread_helper_get_thread shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_get_thread_with_NULL_hazard_pointers_helper_fails)
{
    // arrange

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hazard_pointers_thread);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_010: [ clds_hazard_pointers_thread_helper_get_thread shall get the thread local handle by calling TlsGetValue. ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_011: [ If no thread local handle exists then: ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_012: [ clds_hazard_pointers_thread_helper_get_thread shall create one by calling clds_hazard_pointers_register_thread. ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_013: [ clds_hazard_pointers_thread_helper_get_thread shall store the new handle by calling TlsSetValue. ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_014: [ clds_hazard_pointers_thread_helper_get_thread shall return the thread local handle. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_get_thread_first_time_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = test_create();

    STRICT_EXPECTED_CALL(mocked_TlsGetValue(default_tls_slot));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_register_thread(test_hazard_pointers));
    STRICT_EXPECTED_CALL(mocked_TlsSetValue(default_tls_slot, test_hazard_pointers_thread));

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(helper);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);

    // cleanup
    clds_hazard_pointers_thread_helper_destroy(helper);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_010: [ clds_hazard_pointers_thread_helper_get_thread shall get the thread local handle by calling TlsGetValue. ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_014: [ clds_hazard_pointers_thread_helper_get_thread shall return the thread local handle. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_get_thread_second_time_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = test_create();
    (void)test_get_thread(helper);

    STRICT_EXPECTED_CALL(mocked_TlsGetValue(default_tls_slot))
        .SetReturn((void*)test_hazard_pointers_thread);

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(helper);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);

    // cleanup
    clds_hazard_pointers_thread_helper_destroy(helper);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_010: [ clds_hazard_pointers_thread_helper_get_thread shall get the thread local handle by calling TlsGetValue. ]*/
/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_014: [ clds_hazard_pointers_thread_helper_get_thread shall return the thread local handle. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_get_thread_second_time_succeeds_creates_new_when_different_thread)
{
    // arrange
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = test_create();
    (void)test_get_thread(helper);

    STRICT_EXPECTED_CALL(mocked_TlsGetValue(default_tls_slot));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_register_thread(test_hazard_pointers));
    STRICT_EXPECTED_CALL(mocked_TlsSetValue(default_tls_slot, test_hazard_pointers_thread));

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(helper);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(hazard_pointers_thread);

    // cleanup
    clds_hazard_pointers_thread_helper_destroy(helper);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_015: [ If there are any errors then clds_hazard_pointers_thread_helper_get_thread shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_get_thread_fails_when_clds_hazard_pointers_register_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = test_create();

    STRICT_EXPECTED_CALL(mocked_TlsGetValue(default_tls_slot));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_register_thread(test_hazard_pointers))
        .SetReturn(NULL);

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(helper);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hazard_pointers_thread);

    // cleanup
    clds_hazard_pointers_thread_helper_destroy(helper);
}

/*Tests_SRS_CLDS_HAZARD_POINTERS_THREAD_HELPER_42_015: [ If there are any errors then clds_hazard_pointers_thread_helper_get_thread shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hazard_pointers_thread_helper_get_thread_fails_when_TlsSetValue_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_THREAD_HELPER_HANDLE helper = test_create();

    STRICT_EXPECTED_CALL(mocked_TlsGetValue(default_tls_slot));
    STRICT_EXPECTED_CALL(clds_hazard_pointers_register_thread(test_hazard_pointers));
    STRICT_EXPECTED_CALL(mocked_TlsSetValue(default_tls_slot, test_hazard_pointers_thread))
        .SetReturn(FALSE);
    STRICT_EXPECTED_CALL(clds_hazard_pointers_unregister_thread(test_hazard_pointers_thread));

    // act
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_thread_helper_get_thread(helper);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(hazard_pointers_thread);

    // cleanup
    clds_hazard_pointers_thread_helper_destroy(helper);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
