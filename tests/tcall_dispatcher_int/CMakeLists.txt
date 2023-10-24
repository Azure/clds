#Copyright (c) Microsoft. All rights reserved.

set(theseTestsName tcall_dispatcher_int)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
tcall_dispatcher_foo.c
)

set(${theseTestsName}_h_files
../../inc/clds/tcall_dispatcher.h
tcall_dispatcher_foo.h
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS clds c_pal c_pal_reals c_util c_util_reals)
