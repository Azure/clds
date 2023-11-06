#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(theseTestsName clds_singly_linked_list_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/clds_singly_linked_list.c
)

set(${theseTestsName}_h_files
../../inc/clds/clds_singly_linked_list.h
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_pal c_pal_reals clds_reals)
