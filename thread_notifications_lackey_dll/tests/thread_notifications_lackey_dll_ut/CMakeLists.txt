#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(theseTestsName thread_notifications_lackey_dll_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
thread_notifications_lackey_dll_mocked.c
)

set(${theseTestsName}_h_files
../../inc/thread_notifications_lackey_dll/thread_notifications_lackey_dll.h
)

include_directories(../../inc)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_logging_v2 c_pal c_pal_reals)
