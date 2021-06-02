#Copyright (c) Microsoft. All rights reserved.

set(theseTestsName lock_free_set_int)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/lock_free_set.c
)

set(${theseTestsName}_h_files
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_util c_pal)
