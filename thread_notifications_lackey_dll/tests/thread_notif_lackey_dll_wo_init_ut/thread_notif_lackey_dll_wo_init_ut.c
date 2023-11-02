// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

// This UT suite only exists because we want to test thread_notifications_lackey_dll_deinit_callback without initializing it

#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>

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

MOCK_FUNCTION_WITH_CODE(, int, mock_logger_init);
MOCK_FUNCTION_END(0)
MOCK_FUNCTION_WITH_CODE(, void, mock_logger_deinit);
MOCK_FUNCTION_END()

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

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

/* thread_notifications_lackey_dll_deinit_callback */

/* Tests_SRS_THREAD_NOTIFICATIONS_LACKEY_DLL_01_004: [ thread_notifications_lackey_dll_deinit_callback shall set the callback maintained by the module to NULL. ]*/
TEST_FUNCTION(thread_notifications_lackey_dll_deinit_callback_when_not_initialized_returns)
{
    // arrange

    // act
    thread_notifications_lackey_dll_deinit_callback();

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
