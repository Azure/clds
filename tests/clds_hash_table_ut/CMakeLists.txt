#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(theseTestsName clds_hash_table_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/clds_hash_table.c
)

set(${theseTestsName}_h_files
../../inc/clds/clds_hash_table.h
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_pal_reals c_pal clds_reals)
