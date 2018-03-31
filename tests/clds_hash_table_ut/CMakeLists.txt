#Licensed under the MIT license. See LICENSE file in the project root for full license information.

compileAsC11()
set(theseTestsName clds_hash_table_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/clds_hash_table.c
../reals/real_clds_hazard_pointers.c
)

set(${theseTestsName}_h_files
../reals/real_clds_hazard_pointers.h
)

build_c_test_artifacts(${theseTestsName} ON "tests/clds_tests")
