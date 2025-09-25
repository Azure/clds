#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(theseTestsName clds_sorted_list_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/clds_sorted_list.c
)

set(${theseTestsName}_h_files
../../inc/clds/clds_sorted_list.h
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_pal c_pal_reals clds_reals
    ENABLE_TEST_FILES_PRECOMPILED_HEADERS "${CMAKE_CURRENT_LIST_DIR}/clds_sorted_list_ut_pch.h")
