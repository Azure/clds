#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#reals
add_subdirectory(reals)

#unittests
if(${run_unittests})
    build_test_folder(reals_ut)
    if(WIN32)
        build_test_folder(clds_hazard_pointers_thread_helper_ut) # Windows only until there is a PAL for thread local storage
    endif()
    build_test_folder(clds_hazard_pointers_ut)
    build_test_folder(clds_st_hash_set_ut)
    build_test_folder(lock_free_set_ut)
    build_test_folder(clds_singly_linked_list_ut)
    build_test_folder(clds_sorted_list_ut)
    build_test_folder(clds_hash_table_ut)
    build_test_folder(lru_cache_ut)
    build_test_folder(mpsc_lock_free_queue_ut)
endif()

#integration tests
if(${run_int_tests})
    build_test_folder(lock_free_set_int)
    build_test_folder(clds_singly_linked_list_int)
    build_test_folder(lru_cache_int)
if(WIN32)
        # this test has a problem on Linux, suspicion of badly written test
        build_test_folder(clds_hash_table_int)
endif()
    build_test_folder(clds_sorted_list_int)
    build_test_folder(mpsc_lock_free_queue_int)
endif()

#perf tests
if(${run_perf_tests})
    add_subdirectory(clds_hash_table_perf)
    add_subdirectory(clds_singly_linked_list_perf)
    add_subdirectory(clds_sorted_list_perf)
    add_subdirectory(lock_free_set_perf)
endif()