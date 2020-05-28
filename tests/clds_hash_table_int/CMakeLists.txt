#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(theseTestsName clds_hash_table_int)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
nothing.c
)

set(${theseTestsName}_h_files
)

build_c_tests(${theseTestsName} ON "tests/clds_tests" ADDITIONAL_LIBS clds)
