#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#reals
add_subdirectory(reals)

#unittests
if(${run_unittests})
    build_test_folder(reals_ut)
    build_test_folder(clds_hazard_pointers_ut)
    build_test_folder(clds_st_hash_set_ut)
    build_test_folder(lock_free_set_ut)
    build_test_folder(clds_singly_linked_list_ut)
    build_test_folder(clds_sorted_list_ut)
    build_test_folder(clds_hash_table_ut)
endif()

if(${run_int_tests})
#integration tests
    build_test_folder(lock_free_set_int)
if(WIN32)
    build_test_folder(clds_sorted_list_int)
    build_test_folder(clds_singly_linked_list_int)
    build_test_folder(clds_hash_table_int)
endif()
endif()

if(${run_perf_tests})
#perf tests only running on Windows for now
if(WIN32)
    add_subdirectory(clds_hash_table_perf)
    add_subdirectory(clds_singly_linked_list_perf)
    add_subdirectory(clds_sorted_list_perf)
    add_subdirectory(lock_free_set_perf)
endif()
endif()