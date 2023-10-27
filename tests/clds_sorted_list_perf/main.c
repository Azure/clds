// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdio.h>

#include "c_logging/logger.h"

#include "clds_sorted_list_perf.h"

int main(void)
{
    (void)logger_init();

    clds_sorted_list_perf_main();

    logger_init();

    return 0;
}
