// Copyright (C) Microsoft Corporation. All rights reserved.

#ifndef INACTIVE_HP_THREAD_QUEUE_H
#define INACTIVE_HP_THREAD_QUEUE_H

#ifdef __cplusplus
#include <cstdint>
#else // __cplusplus
#include <stdint.h>
#endif // __cplusplus

#include "macro_utils/macro_utils.h"

#include "c_pal/thandle.h"

#include "c_util/tqueue.h"

#include "clds/clds_hazard_pointers.h"

#include "umock_c/umock_c_prod.h"

typedef struct CLDS_HP_INACTIVE_THREAD_TAG
{
    int64_t current_epoch_value;
    // unfortunately this is not a THANDLE ... someday
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hp_thread_handle;
} CLDS_HP_INACTIVE_THREAD;

TQUEUE_DEFINE_STRUCT_TYPE(CLDS_HP_INACTIVE_THREAD);
THANDLE_TYPE_DECLARE(TQUEUE_TYPEDEF_NAME(CLDS_HP_INACTIVE_THREAD));
TQUEUE_TYPE_DECLARE(CLDS_HP_INACTIVE_THREAD);

#endif // INACTIVE_HP_THREAD_QUEUE_H
