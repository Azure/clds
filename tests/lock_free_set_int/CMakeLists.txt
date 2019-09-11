#Copyright (c) Microsoft. All rights reserved.

cmake_minimum_required(VERSION 2.8.11)

compileAsC11()
set(theseTestsName lock_free_set_int)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/lock_free_set.c
)

set(${theseTestsName}_h_files
)

build_c_tests(${theseTestsName} ON "tests/clds_tests" ADDITIONAL_LIBS aziotsharedutil)
