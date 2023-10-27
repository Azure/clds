// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

// This UT suite exists in order to test APis when the module has not been initialized

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

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
