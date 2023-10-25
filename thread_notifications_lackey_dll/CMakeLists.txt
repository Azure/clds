#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(thread_notifications_lackey_dll_c_files
    ./inc/thread_notifications_lackey_dll/thread_notifications_lackey_dll.h
)

set(thread_notifications_lackey_dll_h_files
    ./src/thread_notifications_lackey_dll.c
)

set(thread_notifications_lackey_dll_def_files
    ./src/thread_notifications_lackey_dll.def
)

add_library(thread_notifications_lackey_dll SHARED ${thread_notifications_lackey_dll_c_files} ${thread_notifications_lackey_dll_h_files} ${thread_notifications_lackey_dll_def_files})
target_include_directories(thread_notifications_lackey_dll PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>)
target_link_libraries(thread_notifications_lackey_dll c_logging_v2 c_pal)

function(copy_thread_notifications_lackey_outputs target output_directory)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE_DIR:thread_notifications_lackey_dll>/thread_notifications_lackey_dll.pdb;$<TARGET_FILE_DIR:thread_notifications_lackey_dll>/thread_notifications_lackey_dll.dll
        ${output_directory})
    add_dependencies(${target} thread_notifications_lackey_dll)
endfunction()

add_subdirectory(tests)
