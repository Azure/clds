// Copyright (c) Microsoft. All rights reserved.

#include "testrunnerswitcher.h"

int main(void)
{
    size_t failedTestCount = 0;
    RUN_TEST_SUITE(lock_free_set_unittests, failedTestCount);
    return failedTestCount;
}
