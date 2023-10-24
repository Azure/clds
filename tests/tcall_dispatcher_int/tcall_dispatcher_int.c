// Copyright (c) Microsoft. All rights reserved.

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "c_pal/interlocked.h"
#include "c_pal/thandle.h"
#include "c_pal/threadapi.h"

#include "clds/tcall_dispatcher.h"
#include "tcall_dispatcher_foo.h"

// TCALL_DISPATCHER_(FOO) is used for most int tests
// It is declared and defined in its own .h/.c files in order to emulate usage in the wilderness

static void test_target(void* context, int32_t x)
{
    ASSERT_ARE_EQUAL(void_ptr, (void*)0x4242, context);
    ASSERT_ARE_EQUAL(int32_t, 42, x);
}

static void test_target_no_x_check(void* context, int32_t x)
{
    // Not validating anything specifically
    (void)context;
    (void)x;
}

// This is a call dispatcher with no args
TCALL_DISPATCHER_DEFINE_CALL_TYPE(CALL_DISPATCHER_NO_ARGS);
THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(CALL_DISPATCHER_NO_ARGS))
TCALL_DISPATCHER_TYPE_DECLARE(CALL_DISPATCHER_NO_ARGS);

THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(CALL_DISPATCHER_NO_ARGS));
TCALL_DISPATCHER_TYPE_DEFINE(CALL_DISPATCHER_NO_ARGS);

static void test_target_no_args(void* context)
{
    // Not validating anything specifically
    (void)context;
}

// This is a call dispatcher with 3 args (out of which one is a const char* just for kicks)
TCALL_DISPATCHER_DEFINE_CALL_TYPE(CALL_DISPATCHER_3_ARGS, bool, arg1, double, arg2, const char*, arg3);
THANDLE_TYPE_DECLARE(TCALL_DISPATCHER_TYPEDEF_NAME(CALL_DISPATCHER_3_ARGS))
TCALL_DISPATCHER_TYPE_DECLARE(CALL_DISPATCHER_3_ARGS, bool, arg1, double, arg2, const char*, arg3);

THANDLE_TYPE_DEFINE(TCALL_DISPATCHER_TYPEDEF_NAME(CALL_DISPATCHER_3_ARGS));
TCALL_DISPATCHER_TYPE_DEFINE(CALL_DISPATCHER_3_ARGS, bool, arg1, double, arg2, const char*, arg3);

static void test_target_3_args(void* context, bool arg1, double arg2, const char* arg3)
{
    // Not validating anything specifically
    (void)context;
    (void)arg1;
    (void)arg2;
    (void)arg3;
}

TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);

BEGIN_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)

TEST_SUITE_INITIALIZE(suite_init)
{
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
}

TEST_FUNCTION_INITIALIZE(method_init)
{
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
}

TEST_FUNCTION(TCALL_DISPATCHER_DISPATCH_CALL_calls_one_target)
{
    // arrange
    TCALL_DISPATCHER(FOO) call_dispatcher = TCALL_DISPATCHER_CREATE(FOO)();
    ASSERT_IS_NOT_NULL(call_dispatcher);
    TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_handle = TCALL_DISPATCHER_REGISTER_TARGET(FOO)(call_dispatcher, test_target, (void*)0x4242);
    ASSERT_IS_NOT_NULL(target_handle);

    // act
    int result = TCALL_DISPATCHER_DISPATCH_CALL(FOO)(call_dispatcher, 42);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);

    // clean
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(call_dispatcher, target_handle));
    TCALL_DISPATCHER_ASSIGN(FOO)(&call_dispatcher, NULL);
}

TEST_FUNCTION(TCALL_DISPATCHER_DISPATCH_CALL_calls_a_target_with_no_args)
{
    // arrange
    TCALL_DISPATCHER(CALL_DISPATCHER_NO_ARGS) call_dispatcher = TCALL_DISPATCHER_CREATE(CALL_DISPATCHER_NO_ARGS)();
    TCALL_DISPATCHER_TARGET_HANDLE(CALL_DISPATCHER_NO_ARGS) target_1_handle = TCALL_DISPATCHER_REGISTER_TARGET(CALL_DISPATCHER_NO_ARGS)(call_dispatcher, test_target_no_args, (void*)0x4242);

    // act
    int result = TCALL_DISPATCHER_DISPATCH_CALL(CALL_DISPATCHER_NO_ARGS)(call_dispatcher);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);

    // clean
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(CALL_DISPATCHER_NO_ARGS)(call_dispatcher, target_1_handle));
    TCALL_DISPATCHER_ASSIGN(CALL_DISPATCHER_NO_ARGS)(&call_dispatcher, NULL);
}

TEST_FUNCTION(TCALL_DISPATCHER_DISPATCH_CALL_calls_a_target_with_3_args)
{
    // arrange
    const char* test_string = "muma padurilor";
    TCALL_DISPATCHER(CALL_DISPATCHER_3_ARGS) call_dispatcher = TCALL_DISPATCHER_CREATE(CALL_DISPATCHER_3_ARGS)();
    TCALL_DISPATCHER_TARGET_HANDLE(CALL_DISPATCHER_3_ARGS) target_1_handle = TCALL_DISPATCHER_REGISTER_TARGET(CALL_DISPATCHER_3_ARGS)(call_dispatcher, test_target_3_args, (void*)0x4242);

    // act
    int result = TCALL_DISPATCHER_DISPATCH_CALL(CALL_DISPATCHER_3_ARGS)(call_dispatcher, true, 0.42, test_string);

    // assert
    ASSERT_ARE_EQUAL(int, 0, result);

    // clean
    ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(CALL_DISPATCHER_3_ARGS)(call_dispatcher, target_1_handle));
    TCALL_DISPATCHER_ASSIGN(CALL_DISPATCHER_3_ARGS)(&call_dispatcher, NULL);
}

#define N_THREADS 4
#ifdef USE_VALGRIND
#define CHAOS_TEST_RUNTIME 5000 // ms
#else // USE_VALGRIND
#define CHAOS_TEST_RUNTIME 1000 // ms
#endif
#define MAX_CALL_TARGET_HANDLES (1 * 1024 * 1024)

static volatile_atomic int32_t terminate_test;

#define TCALL_DISPATCHER_ACTION_TYPE_VALUES \
    TCALL_DISPATCHER_ACTION_TYPE_REGISTER, \
    TCALL_DISPATCHER_ACTION_TYPE_UNREGISTER, \
    TCALL_DISPATCHER_ACTION_TYPE_CALL

MU_DEFINE_ENUM_WITHOUT_INVALID(TCALL_DISPATCHER_ACTION_TYPE, TCALL_DISPATCHER_ACTION_TYPE_VALUES);
MU_DEFINE_ENUM_STRINGS_WITHOUT_INVALID(TCALL_DISPATCHER_ACTION_TYPE, TCALL_DISPATCHER_ACTION_TYPE_VALUES);

typedef struct TCALL_DISPATCHER_CHAOS_TEST_CONTEXT_TAG
{
    TCALL_DISPATCHER(FOO) call_dispatcher;
    volatile_atomic TCALL_DISPATCHER_TARGET_HANDLE(FOO)* call_target_handles;
    volatile_atomic int64_t target_handles_head;
    volatile_atomic int64_t target_handles_tail;
    volatile_atomic int32_t call_number;
} TCALL_DISPATCHER_CHAOS_TEST_CONTEXT;

static int tcall_dispatcher_chaos_thread_func(void* arg)
{
    TCALL_DISPATCHER_CHAOS_TEST_CONTEXT* test_context = arg;

    while (interlocked_add(&terminate_test, 0) == 0)
    {
        TCALL_DISPATCHER_ACTION_TYPE action_type = (TCALL_DISPATCHER_ACTION_TYPE)(rand() * ((MU_COUNT_ARG(TCALL_DISPATCHER_ACTION_TYPE_VALUES)) - 1) / RAND_MAX);

        switch (action_type)
        {
            default:
                ASSERT_FAIL("Unexpected action type %" PRI_MU_ENUM "", MU_ENUM_VALUE(TCALL_DISPATCHER_ACTION_TYPE, action_type));
                break;

            case TCALL_DISPATCHER_ACTION_TYPE_REGISTER:
            {
                int64_t current_head = interlocked_add_64(&test_context->target_handles_head, 0);
                int64_t current_tail = interlocked_add_64(&test_context->target_handles_tail, 0);
                if (current_head == (current_tail + MAX_CALL_TARGET_HANDLES))
                {
                    // array is full, do nothing
                }
                else
                {
                    if (interlocked_compare_exchange_64(&test_context->target_handles_head, current_head + 1, current_head) != current_head)
                    {
                        // head changed, do nothing
                    }
                    else
                    {
                        // register
                        uint32_t index = (uint64_t)current_head % MAX_CALL_TARGET_HANDLES;
                        TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_handle = TCALL_DISPATCHER_REGISTER_TARGET(FOO)(test_context->call_dispatcher, test_target_no_x_check, NULL);
                        ASSERT_IS_NOT_NULL(target_handle);

                        (void)interlocked_exchange_pointer((void* volatile_atomic*) &test_context->call_target_handles[index], target_handle);
                    }
                }
                break;
            }
            case TCALL_DISPATCHER_ACTION_TYPE_UNREGISTER:
            {
                int64_t current_head = interlocked_add_64(&test_context->target_handles_head, 0);
                int64_t current_tail = interlocked_add_64(&test_context->target_handles_tail, 0);
                if (current_head >= current_tail)
                {
                    // array is empty or things changed, do nothing
                }
                else
                {
                    if (interlocked_compare_exchange_64(&test_context->target_handles_tail, current_tail + 1, current_tail) != current_tail)
                    {
                        // tail changed, do nothing
                    }
                    else
                    {
                        // unregister
                        uint32_t index = (uint64_t)current_tail % MAX_CALL_TARGET_HANDLES;

                        do
                        {
                            TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_handle = interlocked_exchange_pointer((void* volatile_atomic*) &test_context->call_target_handles[index], NULL);
                            if (target_handle != NULL)
                            {
                                ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(test_context->call_dispatcher, target_handle));
                                break;;
                            }
                        } while (1);
                    }
                }

                break;
            }
            case TCALL_DISPATCHER_ACTION_TYPE_CALL:
            {
                ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_DISPATCH_CALL(FOO)(test_context->call_dispatcher, interlocked_increment(&test_context->call_number)));
                break;
            }
        }

#ifdef USE_VALGRIND
        // yield
        ThreadAPI_Sleep(0);
#endif
    }

    return 0;
}

TEST_FUNCTION(TCALL_DISPATCHER_chaos_knight_test)
{
    // This test starts N_THREADS threads
    // All threads keep executing random actions
    // Actions can be:
    // - register target
    // - unregister target
    // - call
    // arrange
    TCALL_DISPATCHER_CHAOS_TEST_CONTEXT test_context;
    TCALL_DISPATCHER(FOO) call_dispatcher = TCALL_DISPATCHER_CREATE(FOO)();
    ASSERT_IS_NOT_NULL(call_dispatcher);

    TCALL_DISPATCHER_INITIALIZE_MOVE(FOO)(&test_context.call_dispatcher, &call_dispatcher);

    // this is a classic structure where we have a queue as an array
    // we should have this as a template for the times it was used by now
    test_context.call_target_handles = malloc_2(MAX_CALL_TARGET_HANDLES, sizeof(TCALL_DISPATCHER_TARGET_HANDLE(FOO)));
    ASSERT_IS_NOT_NULL(test_context.call_target_handles);

    (void)interlocked_exchange_64(&test_context.target_handles_head, 0);
    (void)interlocked_exchange_64(&test_context.target_handles_tail, 0);

    for (uint32_t i = 0; i < MAX_CALL_TARGET_HANDLES; i++)
    {
        (void)interlocked_exchange_pointer((void* volatile_atomic*) &test_context.call_target_handles[i], NULL);
    }

    (void)interlocked_exchange(&terminate_test, 0);

    // act
    // assert
    THREAD_HANDLE thread_handles[N_THREADS];
    for (uint32_t i = 0; i < N_THREADS; i++)
    {
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Create(&thread_handles[i], tcall_dispatcher_chaos_thread_func, &test_context));
    }

    ThreadAPI_Sleep(CHAOS_TEST_RUNTIME);

    // terminate test
    (void)interlocked_exchange(&terminate_test, 1);

    for (uint32_t i = 0; i < N_THREADS; i++)
    {
        int dont_care;
        ASSERT_ARE_EQUAL(THREADAPI_RESULT, THREADAPI_OK, ThreadAPI_Join(thread_handles[i], &dont_care));
    }

    // clean
    // unregister all call targets
    do
    {
        int64_t current_head = interlocked_add_64(&test_context.target_handles_head, 0);
        int64_t current_tail = interlocked_add_64(&test_context.target_handles_tail, 0);
        if (current_head == current_tail)
        {
            // array is empty, done
            break;
        }
        else
        {
            (void)interlocked_exchange_64(&test_context.target_handles_tail, current_tail + 1);

            // unregister
            uint32_t index = (uint64_t)current_tail % MAX_CALL_TARGET_HANDLES;
            TCALL_DISPATCHER_TARGET_HANDLE(FOO) target_handle = interlocked_exchange_pointer((void* volatile_atomic*)&test_context.call_target_handles[index], NULL);
            ASSERT_ARE_EQUAL(int, 0, TCALL_DISPATCHER_UNREGISTER_TARGET(FOO)(test_context.call_dispatcher, target_handle));
        }
    } while (1);

    free((void*)test_context.call_target_handles);
    TCALL_DISPATCHER_ASSIGN(FOO)(&test_context.call_dispatcher, NULL);
}

END_TEST_SUITE(TEST_SUITE_NAME_FROM_CMAKE)
