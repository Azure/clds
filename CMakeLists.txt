#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)
project(clds)

include(deps/azure-c-shared-utility/configs/azure_iot_build_rules.cmake)

option(run_e2e_tests "set run_e2e_tests to ON to run e2e tests (default is OFF)" OFF)
option(run_unittests "set run_unittests to ON to run unittests (default is OFF)" OFF)

#do not add or build any tests of the dependencies
set(original_run_e2e_tests ${run_e2e_tests})
set(original_run_int_tests ${run_int_tests})
set(original_run_unittests ${run_unittests})

set(run_e2e_tests OFF)
set(run_int_tests OFF)
set(run_unittests OFF)

add_subdirectory(deps/azure-macro-utils-c)
include_directories(${MACRO_UTILS_INC_FOLDER})

add_subdirectory(deps/azure-ctest)
include_directories(${CTEST_INC_FOLDER})

add_subdirectory(deps/azure-c-testrunnerswitcher)
include_directories(${TESTRUNNERSWITCHER_INC_FOLDER})

add_subdirectory(deps/umock-c)
include_directories(${UMOCK_C_INC_FOLDER})

add_subdirectory(deps/azure-c-shared-utility)
include_directories(${SHARED_UTIL_INC_FOLDER})

set(run_e2e_tests ${original_run_e2e_tests})
set(run_int_tests ${original_run_int_tests})
set(run_unittests ${original_run_unittests})

include_directories(${CMAKE_CURRENT_LIST_DIR}/inc)

set(clds_h_files
    ./inc/clds/clds_hash_table.h
    ./inc/clds/clds_atomics.h
)

set(clds_c_files
    ./src/clds_hash_table.c
)

add_library(clds
    ${clds_c_files}
    ${clds_h_files}
    )

target_link_libraries(clds aziotsharedutil)

if (${run_unittests})
    add_subdirectory(tests)
endif()
