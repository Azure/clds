// Copyright (c) Microsoft. All rights reserved.

#include <stddef.h>
#include "testrunnerswitcher.h"

int main(void)
{
    size_t failedTestCount = 0;
    RUN_TEST_SUITE(mpsc_lock_free_queue_inttests, failedTestCount);
    return failedTestCount;
}
