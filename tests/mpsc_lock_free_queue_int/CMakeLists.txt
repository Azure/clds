#Copyright (c) Microsoft. All rights reserved.

compileAsC11()
set(theseTestsName mpsc_lock_free_queue_int)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/mpsc_lock_free_queue.c
)

set(${theseTestsName}_h_files
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_util)
