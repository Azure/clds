// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>

#include "windows.h"

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "c_logging/logger.h"

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_windows.h"
#include "umock_c/umock_c_negative_tests.h"

#include "c_pal/interlocked.h"

#define ENABLE_MOCKS

#include "c_pal/ps_util.h"

#undef ENABLE_MOCKS

#include "thread_notifications_lackey_dll/thread_notifications_lackey_dll.h"

MOCK_FUNCTION_WITH_CODE(, void, test_thread_notifications_callback_1, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, reason);
MOCK_FUNCTION_END()
MOCK_FUNCTION_WITH_CODE(, void, test_thread_notifications_callback_2, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, reason);
MOCK_FUNCTION_END()
MOCK_FUNCTION_WITH_CODE(, int, mock_logger_init);
MOCK_FUNCTION_END(0)
MOCK_FUNCTION_WITH_CODE(, void, mock_logger_deinit);
MOCK_FUNCTION_END()

BOOL WINAPI DllMain_under_test(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

static HMODULE test_module_handle = (HMODULE)0x42;

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
IMPLEMENT_UMOCK_C_ENUM_TYPE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON);

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, umock_c_init(on_umock_c_error), "umock_c_init failed");
    ASSERT_ARE_EQUAL(int, 0, umocktypes_windows_register_types(), "umocktypes_windows_register_types failed");

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(mock_logger_init, MU_FAILURE);
    REGISTER_TYPE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON);

    ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_negative_tests_deinit();
    umock_c_deinit();
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

/* thread_notifications_lackey_dll_init_callback */

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_001: [ If thread_notification_cb is NULL, thread_notifications_lackey_set_callback shall fail and return a non-zero value. ]*/
TEST_FUNCTION(thread_notifications_lackey_dll_init_callback_with_NULL_thread_notification_cb_fails)
{
    // arrange
    
    // act
    int result = thread_notifications_lackey_dll_init_callback(NULL);
    
    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_002: [ Otherwise, thread_notifications_lackey_dll_init_callback shall initialize the callback maintained by the module with thread_notification_cb, succeed and return 0. ]*/
TEST_FUNCTION(thread_notifications_lackey_dll_init_callback_succeeds)
{
    // arrange

    // act
    int result = thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_1);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    thread_notifications_lackey_dll_deinit_callback();
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_003: [ If the callback was already initialized, thread_notifications_lackey_dll_init_callback shall fail and return a non-zero value. ] */
TEST_FUNCTION(thread_notifications_lackey_dll_init_callback_when_already_initialized_fails)
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_1));

    // act
    int result = thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_2);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    thread_notifications_lackey_dll_deinit_callback();
}

/* thread_notifications_lackey_dll_deinit_callback */

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_004: [ thread_notifications_lackey_dll_deinit_callback shall set the callback maintained by the module to NULL. ]*/
TEST_FUNCTION(thread_notifications_lackey_dll_deinit_callback_clears_the_callback)
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_1));

    // act
    thread_notifications_lackey_dll_deinit_callback();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_004: [ thread_notifications_lackey_dll_deinit_callback shall set the callback maintained by the module to NULL. ]*/
TEST_FUNCTION(thread_notifications_lackey_dll_deinit_callback_when_not_initialized_returns)
{
    // arrange

    // act
    thread_notifications_lackey_dll_deinit_callback();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* DllMain */

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_005: [ If fdwReason is DLL_PROCESS_ATTACH: ]*/
    /* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_006: [ DllMain shall call logger_init to initialize logging for the module. ]*/
    /* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_007: [ Otherwise, DllMain shall initialize the callback maintained by the module with NULL and return TRUE. ]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_DLL_PROCESS_ATTACH_returns_TRUE)
{
    // arrange
    STRICT_EXPECTED_CALL(mock_logger_init());

    // act
    BOOL result = DllMain_under_test(test_module_handle, DLL_PROCESS_ATTACH, NULL);

    // assert
    ASSERT_IS_TRUE(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_008: [ If logger_init fails, DllMain shall return FALSE. ]*/
TEST_FUNCTION(when_underlying_calls_fail_thread_notifications_lackey_DllMain_with_DLL_PROCESS_ATTACH_returns_FALSE)
{
    // arrange
    STRICT_EXPECTED_CALL(mock_logger_init());

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (umock_c_negative_tests_can_call_fail(i))
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            BOOL result = DllMain_under_test(test_module_handle, DLL_PROCESS_ATTACH, NULL);

            // assert
            ASSERT_IS_FALSE(result);
        }
    }
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_009: [ If fdwReason is DLL_PROCESS_DETACH: ]*/
    /* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_010: [DllMain shall call logger_deinit and return TRUE.]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_DLL_PROCESS_DETACH_returns_TRUE)
{
    // arrange
    STRICT_EXPECTED_CALL(mock_logger_deinit());

    // act
    BOOL result = DllMain_under_test(test_module_handle, DLL_PROCESS_DETACH, NULL);

    // assert
    ASSERT_IS_TRUE(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_011: [ If fdwReason is DLL_THREAD_ATTACH: ]*/
    /* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_012: [ DllMain shall return TRUE. ]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_DLL_THREAD_ATTACH_with_no_callback_set_returns_TRUE)
{
    // arrange

    // act
    BOOL result = DllMain_under_test(test_module_handle, DLL_THREAD_ATTACH, NULL);

    // assert
    ASSERT_IS_TRUE(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_013: [ If a callback was passed by the user through a thread_notifications_lackey_dll_init_callback call, the callback shall be called with THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH. ]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_DLL_THREAD_ATTACH_with_callback_set_returns_calls_the_callback_and_returns_TRUE)
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_1));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_thread_notifications_callback_1(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH));

    // act
    BOOL result = DllMain_under_test(test_module_handle, DLL_THREAD_ATTACH, NULL);

    // assert
    ASSERT_IS_TRUE(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    DllMain_under_test(test_module_handle, DLL_THREAD_DETACH, NULL);
    thread_notifications_lackey_dll_deinit_callback();
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_013: [ If a callback was passed by the user through a thread_notifications_lackey_dll_init_callback call, the callback shall be called with THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH. ]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_DLL_THREAD_ATTACH_with_callback_set_returns_calls_the_callback_twice)
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_1));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_thread_notifications_callback_1(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH));
    STRICT_EXPECTED_CALL(test_thread_notifications_callback_1(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH));

    // act
    ASSERT_IS_TRUE(DllMain_under_test(test_module_handle, DLL_THREAD_ATTACH, NULL));
    ASSERT_IS_TRUE(DllMain_under_test(test_module_handle, DLL_THREAD_ATTACH, NULL));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    thread_notifications_lackey_dll_deinit_callback();
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_014: [ If fdwReason is DLL_THREAD_DETACH: ]*/
    /* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_015: [ DllMain shall return TRUE. ]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_DLL_THREAD_DETACH_with_no_callback_set_returns_TRUE)
{
    // arrange

    // act
    BOOL result = DllMain_under_test(test_module_handle, DLL_THREAD_DETACH, NULL);

    // assert
    ASSERT_IS_TRUE(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_016: [ If a callback was passed by the user through a thread_notifications_lackey_dll_init_callback call, the callback shall be called with THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH. ]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_DLL_THREAD_DETACH_with_callback_set_returns_calls_the_callback_and_returns_TRUE)
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_1));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_thread_notifications_callback_1(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH));

    // act
    BOOL result = DllMain_under_test(test_module_handle, DLL_THREAD_DETACH, NULL);

    // assert
    ASSERT_IS_TRUE(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    thread_notifications_lackey_dll_deinit_callback();
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_016: [ If a callback was passed by the user through a thread_notifications_lackey_dll_init_callback call, the callback shall be called with THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH. ]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_DLL_THREAD_DETACH_with_callback_set_returns_calls_the_callback_twice)
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_1));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_thread_notifications_callback_1(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH));
    STRICT_EXPECTED_CALL(test_thread_notifications_callback_1(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH));

    // act
    ASSERT_IS_TRUE(DllMain_under_test(test_module_handle, DLL_THREAD_DETACH, NULL));
    ASSERT_IS_TRUE(DllMain_under_test(test_module_handle, DLL_THREAD_DETACH, NULL));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    thread_notifications_lackey_dll_deinit_callback();
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_013: [ If a callback was passed by the user through a thread_notifications_lackey_dll_init_callback call, the callback shall be called with THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH. ]*/
/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_016: [ If a callback was passed by the user through a thread_notifications_lackey_dll_init_callback call, the callback shall be called with THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH. ]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_DLL_THREAD_ATTACH_and_THREAD_DETACH_with_callback_set_returns_calls_the_callback)
{
    // arrange
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_lackey_dll_init_callback(test_thread_notifications_callback_1));
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_thread_notifications_callback_1(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH));
    STRICT_EXPECTED_CALL(test_thread_notifications_callback_1(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_DETACH));

    // act
    ASSERT_IS_TRUE(DllMain_under_test(test_module_handle, DLL_THREAD_ATTACH, NULL));
    ASSERT_IS_TRUE(DllMain_under_test(test_module_handle, DLL_THREAD_DETACH, NULL));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    thread_notifications_lackey_dll_deinit_callback();
}

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_017: [ If fdwReason is any other value, DllMain shall terminate the process. ]*/
TEST_FUNCTION(thread_notifications_lackey_DllMain_with_an_unknown_value_terminates_the_process)
{
    // arrange
    STRICT_EXPECTED_CALL(ps_util_terminate_process());

    // act
    ASSERT_IS_FALSE(DllMain_under_test(test_module_handle, MU_DIFFERENT(DLL_THREAD_ATTACH, DLL_THREAD_DETACH, DLL_PROCESS_ATTACH, DLL_PROCESS_DETACH), NULL));

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
