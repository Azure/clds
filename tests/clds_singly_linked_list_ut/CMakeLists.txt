#Licensed under the MIT license. See LICENSE file in the project root for full license information.

compileAsC11()
set(theseTestsName clds_singly_linked_list_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/clds_singly_linked_list.c
../reals/real_clds_st_hash_set.c
../reals/real_clds_hazard_pointers.c
)

set(${theseTestsName}_h_files
../../inc/clds/clds_singly_linked_list.h
../reals/real_clds_st_hash_set.h
../reals/real_clds_st_hash_set_renames.h
../reals/real_clds_hazard_pointers.h
../reals/real_clds_hazard_pointers_renames.h
)

build_c_tests(${theseTestsName} ON "tests/clds_tests")
