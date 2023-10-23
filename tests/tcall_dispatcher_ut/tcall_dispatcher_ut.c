// Copyright (c) Microsoft. All rights reserved.

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "umock_c/umock_c.h"
#include "umock_c/umock_c_negative_tests.h"
#include "umock_c/umocktypes_bool.h"
#include "umock_c/umocktypes_charptr.h"

#include "c_pal/thandle.h"

#define ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "c_pal/srw_lock_ll.h"
#include "c_util/doublylinkedlist.h"

#undef ENABLE_MOCKS

#include "real_gballoc_hl.h"

#include "real_srw_lock_ll.h"
#include "real_doublylinkedlist.h"

#include "clds/tcall_dispatcher.h"

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

// This is the call dispatcher used for most tests
TCALL_DISPATCHER_DEFINE_CALL_TYPE(FOO, int, x);
THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(FOO))
TCALL_DISPATCHER_TYPE_DECLARE(FOO, int, x);

THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(FOO));
TCALL_DISPATCHER_TYPE_DEFINE(FOO, int, x);

MOCK_FUNCTION_WITH_CODE(, void, test_target_1, void*, context, int, x)
MOCK_FUNCTION_END()
MOCK_FUNCTION_WITH_CODE(, void, test_target_2, void*, context, int, x)
MOCK_FUNCTION_END()

// This is a call dispatcher with no args
TCALL_DISPATCHER_DEFINE_CALL_TYPE(CALL_DISPATCHER_NO_ARGS);
THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(CALL_DISPATCHER_NO_ARGS))
TCALL_DISPATCHER_TYPE_DECLARE(CALL_DISPATCHER_NO_ARGS);

THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(CALL_DISPATCHER_NO_ARGS));
TCALL_DISPATCHER_TYPE_DEFINE(CALL_DISPATCHER_NO_ARGS);

MOCK_FUNCTION_WITH_CODE(, void, test_target_no_args, void*, context)
MOCK_FUNCTION_END()

// This is a call dispatcher with 3 args (out of which one is a const char* just for kicks)
TCALL_DISPATCHER_DEFINE_CALL_TYPE(CALL_DISPATCHER_3_ARGS, bool, arg1, double, arg2, const char*, arg3);
THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(CALL_DISPATCHER_3_ARGS))
TCALL_DISPATCHER_TYPE_DECLARE(CALL_DISPATCHER_3_ARGS, bool, arg1, double, arg2, const char*, arg3);

THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(CALL_DISPATCHER_3_ARGS));
TCALL_DISPATCHER_TYPE_DEFINE(CALL_DISPATCHER_3_ARGS, bool, arg1, double, arg2, const char*, arg3);

MOCK_FUNCTION_WITH_CODE(, void, test_target_3_args, void*, context, bool, arg1, double, arg2, const char*, arg3)
MOCK_FUNCTION_END()

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%" PRI_MU_ENUM "", MU_ENUM_VALUE(UMOCK_C_ERROR_CODE, error_code));
}

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
    ASSERT_ARE_EQUAL(int, 0, real_gballoc_hl_init(NULL, NULL));

    ASSERT_ARE_EQUAL(int, 0, umock_c_init(on_umock_c_error));
    ASSERT_ARE_EQUAL(int, 0, umocktypes_bool_register_types(), "umocktypes_bool_register_types failed");
    ASSERT_ARE_EQUAL(int, 0, umocktypes_charptr_register_types(), "umocktypes_charptr_register_types failed");

    REGISTER_GBALLOC_HL_GLOBAL_MOCK_HOOK();
    REGISTER_SRW_LOCK_LL_GLOBAL_MOCK_HOOK();
    REGISTER_DOUBLYLINKEDLIST_GLOBAL_MOCK_HOOKS();

    REGISTER_GLOBAL_MOCK_FAIL_RETURN(malloc, NULL);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(srw_lock_ll_init, MU_FAILURE);

    REGISTER_UMOCK_ALIAS_TYPE(PDLIST_ENTRY, void*);

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

/* TCALL_DISPATCHER_CREATE */

/* Tests_SRS_TCALL_DISPATCHER_01_001: [ TCALL_DISPATCHER_CREATE(T) shall call THANDLE_MALLOC to allocate the result. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_002: [ TCALL_DISPATCHER_CREATE(T) shall call srw_lock_ll_init to initialize the lock used by the TCALL_DISPATCHER instance. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_003: [ TCALL_DISPATCHER_CREATE(T) shall call DList_InitializeListHead to initialize the doubly linked list that holds the target call registrations. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_004: [ TCALL_DISPATCHER_CREATE(T) shall succeed and return a non-NULL value. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_CREATE_succeeds)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG)); /* the call dispatcher */
    STRICT_EXPECTED_CALL(srw_lock_ll_init(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_ARG));

    // act
    TCALL_DISPATCHER(FOO) call_dispatcher = TCALL_DISPATCHER_CREATE(FOO)();

    // assert
    ASSERT_IS_NOT_NULL(call_dispatcher);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_005: [ If there are any failures then TCALL_DISPATCHER_CREATE(T) shall fail and return NULL. ]*/
TEST_FUNCTION(when_underlying_calls_fail_TCALL_DISPATCHER_CREATE_also_fails)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG)); /* the call dispatcher */
    STRICT_EXPECTED_CALL(srw_lock_ll_init(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_ARG));

    umock_c_negative_tests_snapshot();

    for (uint32_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (umock_c_negative_tests_can_call_fail(i))
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            TCALL_DISPATCHER(FOO) call_dispatcher = TCALL_DISPATCHER_CREATE(FOO)();

            // assert
            ASSERT_IS_NULL(call_dispatcher, "test failed for call %" PRIu32 "", i);
        }
    }
}

/* TCALL_DISPATCHER_REGISTER_TARGET */

static TCALL_DISPATCHER(FOO) test_create_tcall_dispatcher(void)
{
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG)); /* the call dispatcher */
    STRICT_EXPECTED_CALL(srw_lock_ll_init(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_ARG));
    TCALL_DISPATCHER(FOO) call_dispatcher = TCALL_DISPATCHER_CREATE(FOO)();
    ASSERT_IS_NOT_NULL(call_dispatcher);
    umock_c_reset_all_calls();

    return call_dispatcher;
}

/* Tests_SRS_TCALL_DISPATCHER_01_006: [ If tcall_dispatcher is NULL then TCALL_DISPATCHER_REGISTER_TARGET(T) shall fail and return NULL. ] */
TEST_FUNCTION(TCALL_DISPATCHER_REGISTER_TARGET_with_NULL_tcall_dispatcher_fails)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();

    // act
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_handle = TCALL_DISPATCHER_REGISTER_TARGET(FOO)(NULL, test_target_1, (void*)0x4242);

    // assert
    ASSERT_IS_NULL(target_handle);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_007: [ If function_to_call is NULL then TCALL_DISPATCHER_REGISTER_TARGET(T) shall fail and return NULL. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_REGISTER_TARGET_with_NULL_function_to_call_fails)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();

    // act
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_handle = TCALL_DISPATCHER_REGISTER_TARGET(FOO)(call_dispatcher, NULL, (void*)0x4242);

    // assert
    ASSERT_IS_NULL(target_handle);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_008: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall allocate memory for a TCALL_DISPATCHER_TARGET_HANDLE(T) that holds function_to_call and context. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_009: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall acquire exclusivly the lock. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_010: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall add the new TCALL_DISPATCHER_TARGET_HANDLE(T) containing function_to_call and context in the doubly linked list. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_011: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall release the lock. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_012: [ TCALL_DISPATCHER_REGISTER_TARGET(T) shall succeed and return the TCALL_DISPATCHER_TARGET_HANDLE(T). ] */
TEST_FUNCTION(TCALL_DISPATCHER_REGISTER_TARGET_succeeds)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    // act
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_1_handle = TCALL_DISPATCHER_REGISTER_TARGET(FOO)(call_dispatcher, test_target_1, (void*)0x4242);

    // assert
    ASSERT_IS_NOT_NULL(target_1_handle);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(call_dispatcher, target_1_handle);
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_013: [ If there are any failures then TCALL_DISPATCHER_REGISTER_TARGET(T) shall fail and return NULL. ] */
TEST_FUNCTION(when_underlying_calls_fail_TCALL_DISPATCHER_REGISTER_TARGET_also_fails)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    umock_c_negative_tests_snapshot();

    for (uint32_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (umock_c_negative_tests_can_call_fail(i))
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_1_handle = TCALL_DISPATCHER_REGISTER_TARGET(FOO)(call_dispatcher, test_target_1, (void*)0x4242);

            // assert
            ASSERT_IS_NULL(target_1_handle, "test failed for call %" PRIu32 "", i);
        }
    }

    // clean
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* TCALL_DISPATCHER_UNREGISTER_TARGET */

static TCALL_DISPATCHER_TARGET_HANDLE(FOO) test_register_target(TCALL_DISPATCHER(FOO) call_dispatcher, TCALL_DISPATCHER_TARGET_FUNC(FOO) function_to_call, void* call_context)
{
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    TCALL_DISPATCHER_TARGET_HANDLE(FOO) result = TCALL_DISPATCHER_REGISTER_TARGET(FOO)(call_dispatcher, function_to_call, call_context);
    umock_c_reset_all_calls();

    return result;
}

/* Tests_SRS_TCALL_DISPATCHER_01_014: [ If tcall_dispatcher is NULL then TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall fail and return a non-zero value. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_UNREGISTER_TARGET_with_NULL_tcall_dispatcher_fails)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_1_handle = test_register_target(call_dispatcher, test_target_1, (void*)0x4242) ;

    // act
    int result = TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(NULL, target_1_handle);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(call_dispatcher, target_1_handle));
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_015: [ If call_target is NULL then TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall fail and return a non-zero value. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_UNREGISTER_TARGET_with_NULL_call_target_fails)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();

    // act
    int result = TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(call_dispatcher, NULL);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_016: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall acquire exclusivly the lock. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_017: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall remove from the doubly linked list the call target call_target. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_018: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall release the lock. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_019: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall free the call_target resources. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_020: [ TCALL_DISPATCHER_UNREGISTER_TARGET(T) shall succeed and return 0. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_UNREGISTER_TARGET_succeeds)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_1_handle = test_register_target(call_dispatcher, test_target_1, (void*)0x4242);

    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_ARG));

    // act
    int result = TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(call_dispatcher, target_1_handle);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* TCALL_DISPATCHER_DISPATCH_CALL */

/* Tests_SRS_TCALL_DISPATCHER_01_021: [ If tcall_dispatcher is NULL then TCALL_DISPATCHER_DISPATCH_CALL(T) shall fail and return a non-zero value. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_DISPATCH_CALL_with_NULL_tcall_dispatcher_fails)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_1_handle = test_register_target(call_dispatcher, test_target_1, (void*)0x4242);

    // act
    int result = TCALL_DISPATCHER_DISPATCH_CALL(FOO)(NULL, 42);

    // assert
    ASSERT_ARE_NOT_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(call_dispatcher, target_1_handle));
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_022: [ Otherwise, TCALL_DISPATCHER_DISPATCH_CALL(T) shall acquire the lock in shared mode. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_023: [ For each call target that was registered, TCALL_DISPATCHER_DISPATCH_CALL(T) shall call the function_to_call with the associated context and the parameters in .... ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_024: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall release the lock. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_025: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall succeed and return 0. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_DISPATCH_CALL_calls_one_target)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_1_handle = test_register_target(call_dispatcher, test_target_1, (void*)0x4242);

    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_shared(IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_target_1((void*)0x4242, 42));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_shared(IGNORED_ARG));

    // act
    int result = TCALL_DISPATCHER_DISPATCH_CALL(FOO)(call_dispatcher, 42);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(call_dispatcher, target_1_handle));
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_022: [ Otherwise, TCALL_DISPATCHER_DISPATCH_CALL(T) shall acquire the lock in shared mode. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_023: [ For each call target that was registered, TCALL_DISPATCHER_DISPATCH_CALL(T) shall call the function_to_call with the associated context and the parameters in .... ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_024: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall release the lock. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_025: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall succeed and return 0. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_DISPATCH_CALL_calls_2_targets)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_1_handle = test_register_target(call_dispatcher, test_target_1, (void*)0x4242);
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_2_handle = test_register_target(call_dispatcher, test_target_2, NULL);

    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_shared(IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_target_1((void*)0x4242, 41));
    STRICT_EXPECTED_CALL(test_target_2(NULL, 41));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_shared(IGNORED_ARG));

    // act
    int result = TCALL_DISPATCHER_DISPATCH_CALL(FOO)(call_dispatcher, 41);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(call_dispatcher, target_1_handle));
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(call_dispatcher, target_2_handle));
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_022: [ Otherwise, TCALL_DISPATCHER_DISPATCH_CALL(T) shall acquire the lock in shared mode. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_024: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall release the lock. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_025: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall succeed and return 0. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_DISPATCH_CALL_with_no_targets_succeeds)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = test_create_tcall_dispatcher();

    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_shared(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_shared(IGNORED_ARG));

    // act
    int result = TCALL_DISPATCHER_DISPATCH_CALL(FOO)(call_dispatcher, 41);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_022: [ Otherwise, TCALL_DISPATCHER_DISPATCH_CALL(T) shall acquire the lock in shared mode. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_023: [ For each call target that was registered, TCALL_DISPATCHER_DISPATCH_CALL(T) shall call the function_to_call with the associated context and the parameters in .... ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_024: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall release the lock. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_025: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall succeed and return 0. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_DISPATCH_CALL_calls_a_target_with_no_args)
{
    // arrange
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG)); /* the call dispatcher */
    STRICT_EXPECTED_CALL(srw_lock_ll_init(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_ARG));
    TCALL_DISPATCHER(CALL_DISPATCHER_NO_ARGS) call_dispatcher = TCALL_DISPATCHER_CREATE(CALL_DISPATCHER_NO_ARGS)();
    ASSERT_IS_NOT_NULL(call_dispatcher);

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    TCALL_DISPATCHER_TARGET_HANDLE(CALL_DISPATCHER_NO_ARGS) target_1_handle = TCALL_DISPATCHER_REGISTER_TARGET(CALL_DISPATCHER_NO_ARGS)(call_dispatcher, test_target_no_args, (void*)0x4242);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_shared(IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_target_no_args((void*)0x4242));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_shared(IGNORED_ARG));

    // act
    int result = TCALL_DISPATCHER_DISPATCH_CALL(CALL_DISPATCHER_NO_ARGS)(call_dispatcher);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(CALL_DISPATCHER_NO_ARGS)(call_dispatcher, target_1_handle));
    TCALL_DISPATCHER_ASSIGN(CALL_DISPATCHER_NO_ARGS)(&call_dispatcher, NULL);
}

/* Tests_SRS_TCALL_DISPATCHER_01_022: [ Otherwise, TCALL_DISPATCHER_DISPATCH_CALL(T) shall acquire the lock in shared mode. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_023: [ For each call target that was registered, TCALL_DISPATCHER_DISPATCH_CALL(T) shall call the function_to_call with the associated context and the parameters in .... ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_024: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall release the lock. ]*/
/* Tests_SRS_TCALL_DISPATCHER_01_025: [ TCALL_DISPATCHER_DISPATCH_CALL(T) shall succeed and return 0. ]*/
TEST_FUNCTION(TCALL_DISPATCHER_DISPATCH_CALL_calls_a_target_with_3_args)
{
    // arrange
    const char* test_string = "muma padurilor";
    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG)); /* the call dispatcher */
    STRICT_EXPECTED_CALL(srw_lock_ll_init(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_ARG));
    TCALL_DISPATCHER(CALL_DISPATCHER_3_ARGS) call_dispatcher = TCALL_DISPATCHER_CREATE(CALL_DISPATCHER_3_ARGS)();
    ASSERT_IS_NOT_NULL(call_dispatcher);

    STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_exclusive(IGNORED_ARG));
    STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_exclusive(IGNORED_ARG));

    TCALL_DISPATCHER_TARGET_HANDLE(CALL_DISPATCHER_3_ARGS) target_1_handle = TCALL_DISPATCHER_REGISTER_TARGET(CALL_DISPATCHER_3_ARGS)(call_dispatcher, test_target_3_args, (void*)0x4242);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(srw_lock_ll_acquire_shared(IGNORED_ARG));
    STRICT_EXPECTED_CALL(test_target_3_args((void*)0x4242, true, 0.42, test_string));
    STRICT_EXPECTED_CALL(srw_lock_ll_release_shared(IGNORED_ARG));

    // act
    int result = TCALL_DISPATCHER_DISPATCH_CALL(CALL_DISPATCHER_3_ARGS)(call_dispatcher, true, 0.42, test_string);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // clean
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(CALL_DISPATCHER_3_ARGS)(call_dispatcher, target_1_handle));
    TCALL_DISPATCHER_ASSIGN(CALL_DISPATCHER_3_ARGS)(&call_dispatcher, NULL);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
