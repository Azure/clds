#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(theseTestsName clds_hash_table_with_memory_limit_int)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
)

set(${theseTestsName}_h_files
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS clds)
