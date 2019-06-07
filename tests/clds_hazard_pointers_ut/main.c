// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "testrunnerswitcher.h"

int main(void)
{
    size_t failedTestCount = 0;
    RUN_TEST_SUITE(clds_hazard_pointers_unittests, failedTestCount);
    return failedTestCount;
}
