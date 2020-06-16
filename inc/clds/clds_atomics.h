// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef CLDS_ATOMICS_H
#define CLDS_ATOMICS_H

#ifdef __cplusplus
#else
#include <stdbool.h>
#endif

#include "umock_c/umock_c_prod.h"
#include "azure_macro_utils/macro_utils.h"

// This header has the purpose of switcing between C11 atomics and Windows atomic functions (InterlockedXXX)
// This is due to the fact that to date the MS compiler does not support atomics

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_VERSION__
// we have some STDC_VERSION, check if it is C11
#if (__STDC_VERSION__ >= 201112L)
#ifdef __STDC_NO_ATOMICS__
#define USE_C_ATOMICS 0
#else /* __STDC_NO_ATOMICS__ */
#define USE_C_ATOMICS 1
#endif /* __STDC_NO_ATOMICS__ */
#endif /* (__STDC_VERSION__ == 201112L) */

#else /* __STDC_VERSION__ */
#define USE_C_ATOMICS 0
#endif /* __STDC_VERSION__ */

#if USE_C_ATOMICS

#include <stdatomic.h>

#define clds_atomic_fetch_add_long(object, operand) \
    atomic_fetch_add((object), (operand))
#define clds_atomic_store_long(object, desired) \
    atomic_store((object), (desired))
#define clds_atomic_load_long(object) \
    atomic_load(object)
#define clds_atomic_compare_exchange_strong_long(object, expected, desired) \
    atomic_compare_exchange_strong((object), (expected), (desired))

#define clds_atomic_fetch_add_intptr_t(object, operand) \
    atomic_fetch_add_intptr_t((object), (operand))
#define clds_atomic_store_intptr_t(object, desired) \
    atomic_store((object), (desired))
#define clds_atomic_load_intptr_t(object) \
    atomic_load(object)
#define clds_atomic_compare_exchange_strong_intptr_t(object, expected, desired) \
    atomic_compare_exchange_strong((object), (expected), (desired))

#define CLDS_ATOMIC(atomic_type) MU_C2(atomic_,atomic_type)

#else

#ifdef _MSC_VER

#include "windows.h"

/* long */

typedef LONG clds_atomic_long;
typedef PVOID clds_atomic_intptr_t;

/* nice, we're on the C compiler that is 20 years behind, so we have to emulate whatever we can from the atomics
 behind our macros */
static clds_atomic_long clds_atomic_fetch_add_long(clds_atomic_long volatile *object, clds_atomic_long operand)
{
    return InterlockedAdd(object, operand) - operand;
}

#define clds_atomic_store_long(object, desired) \
    InterlockedExchange(object, desired)
#define clds_atomic_load_long(object) \
    InterlockedAdd(object, 0)

static bool clds_atomic_compare_exchange_strong_long(volatile clds_atomic_long* object, clds_atomic_long* expected, clds_atomic_long desired)
{
    clds_atomic_long expected_value = *expected;
    clds_atomic_long result_value = InterlockedCompareExchange(object, desired, expected_value);
    bool result = (result_value == expected_value);
    if (!result)
    {
        *expected = result_value;
    }
    return result;
}

/* int_ptr_t */

#define clds_atomic_store_intptr_t(object, desired) \
    InterlockedExchangePointer(object, desired)
#define clds_atomic_load_intptr_t(object) \
    InterlockedCompareExchangePointer((volatile PVOID*)object, NULL, NULL)

static bool clds_atomic_compare_exchange_strong_intptr_t(volatile clds_atomic_intptr_t* object, clds_atomic_intptr_t* expected, clds_atomic_intptr_t desired)
{
    clds_atomic_intptr_t expected_value = *expected;
    clds_atomic_intptr_t result_value = InterlockedCompareExchangePointer((volatile PVOID*)(PVOID)object, (PVOID)desired, (PVOID)expected_value);
    bool result = (result_value == expected_value);
    if (!result)
    {
        *expected = result_value;
    }
    return result;
}

#define CLDS_ATOMIC(atomic_type) MU_C2(clds_atomic_,atomic_type)

#else /* _MSC_VER */
#error no atomics support, sorry, get a better compiler
#endif /* _MSC_VER */

#endif

#ifdef __cplusplus
}
#endif

#endif /* CLDS_ATOMICS_H */
