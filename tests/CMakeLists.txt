#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#unittests
add_subdirectory(clds_hash_table_ut)
add_subdirectory(clds_hazard_pointers_ut)
add_subdirectory(clds_singly_linked_list_ut)
add_subdirectory(clds_sorted_list_ut)

#integration tests

#perf tests only running on Windows for now
if(WIN32)
        add_subdirectory(clds_hash_table_perf)
        add_subdirectory(clds_singly_linked_list_perf)
        add_subdirectory(clds_sorted_list_perf)
endif()