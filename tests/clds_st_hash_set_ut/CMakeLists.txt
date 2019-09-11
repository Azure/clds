#Licensed under the MIT license. See LICENSE file in the project root for full license information.

compileAsC11()
set(theseTestsName clds_st_hash_set_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
    ../../src/clds_st_hash_set.c
)

set(${theseTestsName}_h_files
    ../../inc/clds/clds_st_hash_set.h
)

build_c_tests(${theseTestsName} ON "tests/clds_tests")
