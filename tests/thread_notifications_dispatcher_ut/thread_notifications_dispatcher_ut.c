// Copyright (c) Microsoft. All rights reserved.

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "umock_c/umock_c.h"
#include "umock_c/umock_c_negative_tests.h"

#include "c_pal/thandle.h"

#define ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "clds/tcall_dispatcher_thread_notification_call.h"

#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"
#include "real_tcall_dispatcher_thread_notification_call.h"

#include "clds/thread_notifications_dispatcher.h"

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

TEST_DEFINE_ENUM_TYPE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_VALUES);

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, real_gballoc_hl_init(NULL, NULL));

    ASSERT_ARE_EQUAL(int, 0, umock_c_init(on_umock_c_error));

    REGISTER_TCALL_DISPATCHER_THREAD_NOTIFICATION_CALL_GLOBAL_MOCK_HOOK();

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(malloc, NULL);

    REGISTER_UMOCK_ALIAS_TYPE(THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL), void*);

    REGISTER_TYPE(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON);

    ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_negative_tests_deinit();
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

/* thread_notifications_dispatcher_init */

/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_002: [ Otherwise, thread_notifications_dispatcher_init shall initialize the module: ]*/
/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_003: [ thread_notifications_dispatcher_init shall create a TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL). ]*/
/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_004: [ thread_notifications_dispatcher_init shall store in a global variable the TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL). ]*/
/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_005: [ thread_notifications_dispatcher_init shall call thread_notifications_lackey_dll_init_callback to register thread_notifications_lackey_dll_cb as the thread notifications callback. ]*/
/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_006: [ thread_notifications_dispatcher_init shall succeed and return 0. ]*/
TEST_FUNCTION(thread_notifications_dispatcher_init_initializes_the_module)
{
    // arrange
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_CREATE(THREAD_NOTIFICATION_CALL)());
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_INITIALIZE_MOVE(THREAD_NOTIFICATION_CALL)(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(thread_notifications_lackey_dll_init_callback(IGNORED_ARG));

    // act
    int result = thread_notifications_dispatcher_init();

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    thread_notifications_dispatcher_deinit();
}

static void setup_thread_notifications_dispatcher_init(void)
{
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_CREATE(THREAD_NOTIFICATION_CALL)());
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_INITIALIZE_MOVE(THREAD_NOTIFICATION_CALL)(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(thread_notifications_lackey_dll_init_callback(IGNORED_ARG));
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_dispatcher_init());
    umock_c_reset_all_calls();
}

/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_001: [ If the module is already initialized, thread_notifications_dispatcher_init shall fail and return a non-zero value. ]*/
TEST_FUNCTION(thread_notifications_dispatcher_init_when_already_initialized_fails)
{
    // arrange
    setup_thread_notifications_dispatcher_init();

    // act
    int result = thread_notifications_dispatcher_init();

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    thread_notifications_dispatcher_deinit();
}

/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_007: [ If any error occurrs, thread_notifications_dispatcher_init shall fail and return a non-zero value. ]*/
TEST_FUNCTION(when_underlying_calls_fail_thread_notifications_dispatcher_init_also_fails)
{
    // arrange
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_CREATE(THREAD_NOTIFICATION_CALL)());
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_INITIALIZE_MOVE(THREAD_NOTIFICATION_CALL)(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(thread_notifications_lackey_dll_init_callback(IGNORED_ARG));

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (umock_c_negative_tests_can_call_fail(i))
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            int result = thread_notifications_dispatcher_init();

            // assert
            ASSERT_ARE_NOT_EQUAL(int, 0, result);
        }
    }
}

/* thread_notifications_dispatcher_deinit */

/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_008: [ If the module is not initialized, thread_notifications_dispatcher_deinit shall return. ]*/
TEST_FUNCTION(thread_notifications_dispatcher_deinit_when_not_initialized_returns)
{
    // arrange

    // act
    thread_notifications_dispatcher_deinit();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_009: [ Otherwise, thread_notifications_dispatcher_init shall call thread_notifications_lackey_dll_deinit_callback to clear the thread notifications callback. ]*/
/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_010: [ thread_notifications_dispatcher_deinit shall release the reference to the TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL). ]*/
/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_011: [ thread_notifications_dispatcher_deinit shall de-initialize the module. ]*/
TEST_FUNCTION(thread_notifications_dispatcher_deinit_deinitializes_the_module)
{
    // arrange
    setup_thread_notifications_dispatcher_init();

    STRICT_EXPECTED_CALL(thread_notifications_lackey_dll_deinit_callback());
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(IGNORED_ARG, IGNORED_ARG));

    // act
    thread_notifications_dispatcher_deinit();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* thread_notifications_dispatcher_get_call_dispatcher */

/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_012: [ TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) shall return the TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) created in thread_notifications_dispatcher_init. ]*/
TEST_FUNCTION(thread_notifications_dispatcher_get_call_dispatcher_when_not_initialized)
{
    // arrange
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(IGNORED_ARG, IGNORED_ARG));

    // act
    TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) call_dispatcher = thread_notifications_dispatcher_get_call_dispatcher();

    // assert
    ASSERT_IS_NULL(call_dispatcher);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_012: [ TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) shall return the TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) created in thread_notifications_dispatcher_init. ]*/
TEST_FUNCTION(thread_notifications_dispatcher_get_call_dispatcher_returns_the_call_dispatcher)
{
    // arrange
    setup_thread_notifications_dispatcher_init();

    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(IGNORED_ARG, IGNORED_ARG));

    // act
    TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) call_dispatcher = thread_notifications_dispatcher_get_call_dispatcher();

    // assert
    ASSERT_IS_NOT_NULL(call_dispatcher);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(&call_dispatcher, NULL);
    thread_notifications_dispatcher_deinit();
}

/* thread_notifications_lackey_dll_cb */

/* Tests_SRS_THREAD_NOTIFICATIONS_DISPATCHER_01_013: [ thread_notifications_lackey_dll_cb shall dispatch the call by calling TCALL_DISPATCHER_DISPATCH_CALL(THREAD_NOTIFICATION_CALL) with reason as argument. ]*/
TEST_FUNCTION(thread_notifications_lackey_dll_cb)
{
    // arrange
    THREAD_NOTIFICATIONS_LACKEY_DLL_CALLBACK_FUNC thread_notifications_cb;
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_CREATE(THREAD_NOTIFICATION_CALL)());
    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_INITIALIZE_MOVE(THREAD_NOTIFICATION_CALL)(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(thread_notifications_lackey_dll_init_callback(IGNORED_ARG))
        .CaptureArgumentValue_thread_notifications_cb(&thread_notifications_cb);
    ASSERT_ARE_EQUAL(int, 0, thread_notifications_dispatcher_init());
    TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) call_dispatcher = thread_notifications_dispatcher_get_call_dispatcher();
    ASSERT_IS_NOT_NULL(call_dispatcher);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(TCALL_DISPATCHER_DISPATCH_CALL(THREAD_NOTIFICATION_CALL)(call_dispatcher, THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH));

    // act
    thread_notifications_cb(THREAD_NOTIFICATIONS_LACKEY_DLL_REASON_THREAD_ATTACH);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TCALL_DISPATCHER_ASSIGN(THREAD_NOTIFICATION_CALL)(&call_dispatcher, NULL);
    thread_notifications_dispatcher_deinit();
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
