// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "clds/inactive_hp_thread_queue.h"

THANDLE_TYPE_DEFINE(TQUEUE_TYPEDEF_NAME(CLDS_HP_INACTIVE_THREAD));
TQUEUE_TYPE_DEFINE(CLDS_HP_INACTIVE_THREAD);
