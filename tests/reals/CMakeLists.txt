#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

set(clds_reals_c_files
    real_lock_free_set.c
)

set(clds_reals_h_files
    real_lock_free_set.h
    real_lock_free_set_renames.h
)

if (WIN32)
set(clds_reals_c_files ${clds_reals_c_files}
    real_clds_hazard_pointers.c
    real_clds_hash_table.c
    real_clds_singly_linked_list.c
    real_clds_sorted_list.c
    real_clds_st_hash_set.c
)

set(clds_reals_h_files ${clds_reals_h_files}
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
)
endif()
include_directories(${CMAKE_CURRENT_LIST_DIR}/../../src)
add_library(clds_reals ${clds_reals_c_files} ${clds_reals_h_files})
target_include_directories(clds_reals PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests/reals>)
