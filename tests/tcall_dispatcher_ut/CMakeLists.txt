#Copyright (c) Microsoft. All rights reserved.

set(theseTestsName tcall_dispatcher_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/tcall_dispatcher.c
)

set(${theseTestsName}_h_files
../../inc/clds/tcall_dispatcher.h
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_pal c_pal_reals c_util c_util_reals)
