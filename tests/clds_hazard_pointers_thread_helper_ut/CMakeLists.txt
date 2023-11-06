#Copyright (c) Microsoft. All rights reserved.

set(theseTestsName clds_hazard_pointers_thread_helper_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
clds_hazard_pointers_thread_helper_mocked.c
)

set(${theseTestsName}_h_files
../../inc/clds/clds_hazard_pointers_thread_helper.h
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_pal_reals c_pal_umocktypes thread_notifications_lackey_dll clds_reals)
