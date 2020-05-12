#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

project(clds)

include(CTest)

option(run_e2e_tests "set run_e2e_tests to ON to run e2e tests (default is OFF)" OFF)
option(run_unittests "set run_unittests to ON to run unittests (default is OFF)" OFF)
option(run_traceability "run traceability tool (default is ON)" ON)

#do not add or build any tests of the dependencies
set(original_run_e2e_tests ${run_e2e_tests})
set(original_run_int_tests ${run_int_tests})
set(original_run_unittests ${run_unittests})
set(original_run_perf_tests ${run_perf_tests})
set(original_run_traceability ${run_traceability})

set(run_e2e_tests OFF)
set(run_int_tests OFF)
set(run_unittests OFF)
set(run_perf_tests OFF)
set(run_traceability OFF)

if ((NOT TARGET azure_c_build_tools) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-build-tools/CMakeLists.txt))
        add_subdirectory(deps/azure-c-build-tools)
endif()
set_default_build_options()

#smhasher generates some warnings with /W3 (default)
if(WIN32)
    add_definitions(/W2)
endif(WIN32)

if ((NOT TARGET SMHasherSupport) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/smhasher/src/CMakeLists.txt))
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/smhasher/src)
    include_directories(${CMAKE_CURRENT_LIST_DIR}/deps/smhasher/src)
endif()

if(WIN32)
    remove_definitions(/W2)
endif(WIN32)

if ((NOT TARGET azure_macro_utils_c) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-macro-utils-c/CMakeLists.txt))
        add_subdirectory(deps/azure-macro-utils-c)
        include_directories(${MACRO_UTILS_INC_FOLDER})
endif()

if ((NOT TARGET azure_c_logging) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-logging/CMakeLists.txt))
        add_subdirectory(deps/azure-c-logging)
        include_directories(deps/azure-c-logging/inc)
endif()

if ((NOT TARGET ctest) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-ctest/CMakeLists.txt))
        add_subdirectory(deps/azure-ctest)
        include_directories(${CTEST_INC_FOLDER})
endif()

if ((NOT TARGET testrunnerswitcher) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-testrunnerswitcher/CMakeLists.txt))
        add_subdirectory(deps/azure-c-testrunnerswitcher)
        include_directories(${TESTRUNNERSWITCHER_INC_FOLDER})
endif()

if ((NOT TARGET umock_c) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/umock-c/CMakeLists.txt))
        add_subdirectory(deps/umock-c)
        include_directories(${UMOCK_C_INC_FOLDER})
endif()

if ((NOT TARGET azure_c_util) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-util/CMakeLists.txt))
        add_subdirectory(deps/azure-c-util)
        include_directories(${C_UTIL_INC_FOLDER})

        set_platform_files(${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-util)
endif()

set(run_e2e_tests ${original_run_e2e_tests})
set(run_int_tests ${original_run_int_tests})
set(run_unittests ${original_run_unittests})
set(run_traceability ${original_run_traceability})

if((WIN32) AND (${run_traceability}))
    #add traceability custom target
    add_custom_target(clds_traceability ALL
        COMMAND ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-build-tools/traceabilitytool/traceability_tool/bin/$<CONFIG>/traceabilitytool.exe -buildcheck -e ${CMAKE_CURRENT_LIST_DIR}/deps -i ${CMAKE_CURRENT_LIST_DIR})
    add_dependencies(clds_traceability traceabilitytool)
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR}/inc)

set(clds_h_files
    ./inc/clds/clds_atomics.h
    ./inc/clds/lock_free_set.h
)

if (WIN32)
set(clds_h_files
    ${clds_h_files}
    ./inc/clds/clds_hash_table.h
    ./inc/clds/clds_st_hash_set.h
    ./inc/clds/clds_hazard_pointers.h
    ./inc/clds/clds_singly_linked_list.h
    ./inc/clds/clds_sorted_list.h
)
endif()

set(clds_c_files
    ./src/lock_free_set.c
)

if (WIN32)
set(clds_c_files
    ${clds_c_files}
    ./src/clds_hash_table.c
    ./src/clds_st_hash_set.c
    ./src/clds_hazard_pointers.c
    ./src/clds_singly_linked_list.c
    ./src/clds_sorted_list.c
)
endif()

FILE(GLOB clds_md_files "devdoc/*.md")
SOURCE_GROUP(devdoc FILES ${clds_md_files})

add_library(clds
    ${clds_c_files}
    ${clds_h_files}
    ${clds_md_files}
    )

target_link_libraries(clds azure_c_util synchronization)

add_subdirectory(tests)
