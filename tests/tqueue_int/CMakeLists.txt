#Copyright (c) Microsoft. All rights reserved.

set(theseTestsName tqueue_int)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
tqueue_foo.c
)

set(${theseTestsName}_h_files
../../inc/clds/tqueue.h
tqueue_foo.h
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS clds c_pal c_pal_reals c_util c_util_reals)
