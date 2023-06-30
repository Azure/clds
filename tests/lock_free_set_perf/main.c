// Copyright (c) Microsoft. All rights reserved.

#include <stdio.h>

#include "c_logging/logger.h"

#include "lock_free_set_perf.h"

int main(void)
{
    (void)logger_init();

    lock_free_set_perf_main();

    logger_deinit();

    return 0;
}
