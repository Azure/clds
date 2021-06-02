#Copyright (c) Microsoft. All rights reserved.

set(theseTestsName lock_free_set_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/lock_free_set.c
)

set(${theseTestsName}_h_files
../../inc/clds/lock_free_set.h
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_pal c_pal_reals)
