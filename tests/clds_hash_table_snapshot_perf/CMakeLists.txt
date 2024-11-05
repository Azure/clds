#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(theseTestsName clds_hash_table_snapshot_perf)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
)

set(${theseTestsName}_h_files
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS clds)

if("${building}" STREQUAL "exe")
    copy_thread_notifications_lackey_outputs(${theseTestsName}_exe_${CMAKE_PROJECT_NAME} "$<TARGET_FILE_DIR:${theseTestsName}_exe_${CMAKE_PROJECT_NAME}>")
endif()

if("${building}" STREQUAL "dll")
    copy_thread_notifications_lackey_outputs(${theseTestsName}_dll_${CMAKE_PROJECT_NAME} "$<TARGET_FILE_DIR:${theseTestsName}_exe_${CMAKE_PROJECT_NAME}>")
endif()

