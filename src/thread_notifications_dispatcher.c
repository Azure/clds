// Copyright (C) Microsoft Corporation. All rights reserved.

#include <stdlib.h>

#include "macro_utils/macro_utils.h"

#include "c_logging/logger.h"

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"

#include "clds/thread_notifications_dispatcher.h"

int thread_notifications_dispatcher_init(void)
{
    return MU_FAILURE;
}

void thread_notifications_dispatcher_deinit(void)
{
}

TCALL_DISPATCHER(THREAD_NOTIFICATION_CALL) thread_notifications_dispatcher_get_call_dispatcher(void)
{
    return NULL;
}
