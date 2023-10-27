#Copyright (c) Microsoft. All rights reserved.

set(theseTestsName thread_notifications_dispatcher_int)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
)

set(${theseTestsName}_h_files
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_pal c_util clds)

if("${building}" STREQUAL "exe")
    copy_thread_notifications_lackey_outputs(${theseTestsName}_exe_${CMAKE_PROJECT_NAME} $<TARGET_FILE_DIR:${theseTestsName}_exe_${CMAKE_PROJECT_NAME}>)
endif()

if("${building}" STREQUAL "dll")
    copy_thread_notifications_lackey_outputs(${theseTestsName}_dll_${CMAKE_PROJECT_NAME} $<TARGET_FILE_DIR:${theseTestsName}_dll_${CMAKE_PROJECT_NAME}>)
endif()
