#Licensed under the MIT license. See LICENSE file in the project root for full license information.

compileAsC11()
set(theseTestsName clds_hash_table_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/clds_hash_table.c
../reals/real_clds_st_hash_set.c
../reals/real_clds_hazard_pointers.c
../reals/real_clds_sorted_list.c
)

set(${theseTestsName}_h_files
../../inc/clds/clds_hash_table.h
../reals/real_clds_st_hash_set.h
../reals/real_clds_st_hash_set_renames.h
../reals/real_clds_hazard_pointers.h
../reals/real_clds_hazard_pointers_renames.h
../reals/real_clds_sorted_list.h
)

build_c_test_artifacts(${theseTestsName} ON "tests/clds_tests")
