// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>

#include "c_logging/logger.h"

#include "clds_hash_table_perf.h"

int main(void)
{
    (void)logger_init();

    clds_hash_table_perf_main();

    logger_deinit();

    return 0;
}
