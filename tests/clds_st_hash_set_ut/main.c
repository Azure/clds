// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "testrunnerswitcher.h"

int main(void)
{
    size_t failedTestCount = 0;
    RUN_TEST_SUITE(clds_st_hash_set_unittests, failedTestCount);

#ifdef VLD_OPT_REPORT_TO_STDOUT
    failedTestCount = (failedTestCount>0)?failedTestCount:-(int)VLDGetLeaksCount();
#endif

    return failedTestCount;
}
