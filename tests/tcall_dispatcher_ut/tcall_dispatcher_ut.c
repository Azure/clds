// Copyright (c) Microsoft. All rights reserved.

#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "umock_c/umock_c.h"

#include "c_pal/thandle.h"

#define ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"

#include "clds/tcall_dispatcher.h"

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

/* Final example usage should look like:
TCALL_DISPATCHER_DEFINE_CALL_TYPE(FOO, int, x);
THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(FOO))
TCALL_DISPATCHER_TYPE_DECLARE(FOO, int, x);

THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(FOO));
TCALL_DISPATCHER_TYPE_DEFINE(FOO, int, x); */

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

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

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
