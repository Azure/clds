#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(clds_reals_c_files
    real_clds_hazard_pointers.c
    real_clds_hash_table.c
    real_clds_singly_linked_list.c
    real_clds_sorted_list.c
    real_clds_st_hash_set.c
    real_lock_free_set.c
    real_mpsc_lock_free_queue.c
)

if (WIN32)
    set(clds_reals_c_files
        ${clds_reals_c_files}
        real_clds_hazard_pointers_thread_helper.c # Windows only until there is a PAL for thread local storage
        real_tcall_dispatcher_thread_notification_call.c
        real_thread_notifications_dispatcher.c
    )
endif()

set(clds_reals_h_files
    real_clds_hazard_pointers.h
    real_clds_hazard_pointers_renames.h
    real_clds_hash_table.h
    real_clds_hash_table_renames.h
    real_clds_singly_linked_list.h
    real_clds_singly_linked_list_renames.h
    real_clds_sorted_list.h
    real_clds_sorted_list_renames.h
    real_clds_st_hash_set.h
    real_clds_st_hash_set_renames.h
    real_lock_free_set.h
    real_lock_free_set_renames.h
    real_mpsc_lock_free_queue.h
    real_mpsc_lock_free_queue_renames.h
)

if (WIN32)
    set(clds_reals_h_files
        ${clds_reals_h_files}
        real_clds_hazard_pointers_thread_helper.h
        real_clds_hazard_pointers_thread_helper_renames.h # Windows only until there is a PAL for thread local storage
        real_tcall_dispatcher_thread_notification_call.h
        real_tcall_dispatcher_thread_notification_call_renames.h
        real_thread_notifications_dispatcher.h
        real_thread_notifications_dispatcher_renames.h
    )
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR}/../../src)
add_library(clds_reals ${clds_reals_c_files} ${clds_reals_h_files})
target_link_libraries(clds_reals c_pal_reals c_util_reals)
if (WIN32)
    target_link_libraries(clds_reals thread_notifications_lackey_dll)
endif()
target_include_directories(clds_reals PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests/reals>)
