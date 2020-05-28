#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(theseTestsName clds_hazard_pointers_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/clds_hazard_pointers.c
../../src/clds_st_hash_set.c
)

set(${theseTestsName}_h_files
../../inc/clds/clds_hazard_pointers.h
)

build_c_tests(${theseTestsName} ON "tests/clds_tests")
