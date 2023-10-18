#Licensed under the MIT license. See LICENSE file in the project root for full license information.

if(NOT DEFINED CMAKE_MINIMUM_REQUIRED_VERSION)
    cmake_minimum_required(VERSION 3.18)
endif()

# canon way of using clds from another repo is below. It assumes the using repo has placed clds in "deps"

#if ((NOT TARGET clds) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/clds/CMakeLists.txt))
#    add_subdirectory(deps/clds)
#    include_directories(${CMAKE_CURRENT_LIST_DIR}/deps/clds/inc)
#endif() 

project(clds)

include(CTest)

option(run_e2e_tests "set run_e2e_tests to ON to run e2e tests (default is OFF)" OFF)
option(run_unittests "set run_unittests to ON to run unittests (default is OFF)" OFF)
option(run_perf_tests "set run_perf_tests to ON to build performance tests (default is OFF)." OFF)
option(run_reals_check "set run_reals_check to ON to run reals check (default is OFF)." OFF)
option(run_traceability "run traceability tool (default is ON)" ON)
option(use_cppunittest "set use_cppunittest to ON to build CppUnitTest tests on Windows (default is OFF)" OFF)

#bring in dependencies
#do not add or build any tests of the dependencies
set(original_run_e2e_tests ${run_e2e_tests})
set(original_run_unittests ${run_unittests})
set(original_run_int_tests ${run_int_tests})
set(original_run_traceability ${run_traceability})
set(original_run_perf_tests ${run_perf_tests})
set(original_run_reals_check ${run_reals_check})

set(run_e2e_tests OFF)
set(run_unittests OFF)
set(run_int_tests OFF)
set(run_traceability OFF)
set(run_perf_tests OFF)
set(run_reals_check OFF)

if(WIN32)
else()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()

if ((NOT TARGET c_build_tools) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/c-build-tools/CMakeLists.txt))
    add_subdirectory(deps/c-build-tools)
    set_default_build_options()
endif() 

if ((NOT TARGET macro_utils_c) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/macro-utils-c/CMakeLists.txt))
    add_subdirectory(deps/macro-utils-c)
    include_directories(${MACRO_UTILS_INC_FOLDER})
endif() 

if ((NOT TARGET c_logging) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/c-logging/CMakeLists.txt))
    add_subdirectory(deps/c-logging)
    include_directories(deps/c-logging/v2/inc)
endif() 

if ((NOT TARGET ctest) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/ctest/CMakeLists.txt))
    add_subdirectory(deps/ctest)
    include_directories(${CTEST_INC_FOLDER})
endif() 

if ((NOT TARGET testrunnerswitcher) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/c-testrunnerswitcher/CMakeLists.txt))
    add_subdirectory(deps/c-testrunnerswitcher)
    include_directories(${TESTRUNNERSWITCHER_INC_FOLDER})
endif() 

if ((NOT TARGET umock_c) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/umock-c/CMakeLists.txt))
    add_subdirectory(deps/umock-c)
    include_directories(${UMOCK_C_INC_FOLDER})
endif() 

if ((NOT TARGET c_pal) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/c-pal/CMakeLists.txt))
    add_subdirectory(deps/c-pal)
    include_directories(${C_PAL_INC_FOLDER})
endif() 

if (
    (NOT TARGET mimalloc-obj) AND 
    (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/mimalloc/CMakeLists.txt)
    )
        set(MI_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE) #not building a dll allows building on 32 bit, otherwise there's some errors on init.c about not finding a imported symbol
        set(MI_BUILD_TESTS OFF CACHE BOOL "Build test executables" FORCE) 
        #for mimalloc disable this warning: Warning C4459: declaration of 'os_page_size' hides global declaration
        #for mimalloc disable this warning: Warning C4100: 'try_alignment': unreferenced formal parameter
        #for mimalloc disable this warning: warning C4505: 'mi_os_get_aligned_hint': unreferenced local function has been removed

        set(PREV_CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
        set(PREV_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
        if(WIN32)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4459 /wd4100 /wd4505")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4459 /wd4100 /wd4505")
        endif()

        add_subdirectory(deps/mimalloc)
        include_directories(deps/mimalloc/include)
        
        set(CMAKE_C_FLAGS ${PREV_CMAKE_C_FLAGS})
        set(CMAKE_CXX_FLAGS ${PREV_CMAKE_CXX_FLAGS})
endif()

if (
    (WIN32) AND
    (NOT TARGET jemalloc) AND 
    (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/jemalloc/README)
    )
    set(PREV_CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
    set(PREV_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    set(PREV_CMAKE_STATIC_LINKER_FLAGS ${CMAKE_STATIC_LINKER_FLAGS})

    if(WIN32)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4100 /wd4127 /wd4459 /wd4201 /wd4456 /wd4457 /wd4702 /wd4244 /wd4701 /wd4706 /wd4703 /wd4189 /wd4267 /wd4204 /wd4565 /wd4310 /wd4334 /wd4028 /DJEMALLOC_NO_PRIVATE_NAMESPACE /D_REENTRANT /DJEMALLOC_EXPORT= /D_LIB")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100 /wd4127 /wd4459 /wd4201 /wd4456 /wd4457 /wd4702 /wd4244 /wd4701 /wd4706 /wd4703 /wd4189 /wd4267 /wd4204 /wd4565 /wd4310 /wd4334 /wd4028 /DJEMALLOC_NO_PRIVATE_NAMESPACE /D_REENTRANT /DJEMALLOC_EXPORT= /D_LIB")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /IGNORE:4221")
    endif()

    add_subdirectory(jemalloc_build)

    set(CMAKE_C_FLAGS ${PREV_CMAKE_C_FLAGS})
    set(CMAKE_CXX_FLAGS ${PREV_CMAKE_CXX_FLAGS})
    set(CMAKE_STATIC_LINKER_FLAGS ${PREV_CMAKE_STATIC_LINKER_FLAGS})

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DJEMALLOC_NO_PRIVATE_NAMESPACE /D_REENTRANT /DJEMALLOC_EXPORT= /D_LIB")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DJEMALLOC_NO_PRIVATE_NAMESPACE /D_REENTRANT /DJEMALLOC_EXPORT= /D_LIB")

endif()

if ((NOT TARGET c_util) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/c-util/CMakeLists.txt))
    add_subdirectory(deps/c-util)
    include_directories(${C_UTIL_INC_FOLDER})
endif() 

set(run_e2e_tests ${original_run_e2e_tests})
set(run_unittests ${original_run_unittests})
set(run_int_tests ${original_run_int_tests})
set(run_traceability ${original_run_traceability})
set(run_perf_tests ${original_run_perf_tests})
set(run_reals_check ${original_run_reals_check})

if((WIN32) AND (${run_traceability}))
    #add traceability custom target
    add_custom_target(clds_traceability ALL
        COMMAND traceabilitytool -buildcheck -e ${CMAKE_CURRENT_LIST_DIR}/deps -i ${CMAKE_CURRENT_LIST_DIR})
    add_dependencies(clds_traceability traceabilitytool)
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR}/inc)

set(clds_h_files
    ./inc/clds/clds_hazard_pointers.h
    ./inc/clds/clds_sorted_list.h
    ./inc/clds/clds_st_hash_set.h
    ./inc/clds/lock_free_set.h
    ./inc/clds/clds_hash_table.h
    ./inc/clds/clds_singly_linked_list.h
    ./inc/clds/mpsc_lock_free_queue.h
)

if (WIN32)
    set(clds_h_files
        ${clds_h_files}
        ./inc/clds/clds_hazard_pointers_thread_helper.h # Windows only until there is a PAL for thread local storage
    )
endif()

set(clds_c_files
    ./src/clds_hazard_pointers.c
    ./src/clds_sorted_list.c
    ./src/clds_st_hash_set.c
    ./src/lock_free_set.c
    ./src/clds_hash_table.c
    ./src/clds_singly_linked_list.c
    ./src/mpsc_lock_free_queue.c
)

if (WIN32)
    set(clds_c_files
        ${clds_c_files}
        ./src/clds_hazard_pointers_thread_helper.c # Windows only until there is a PAL for thread local storage
    )
endif()

FILE(GLOB clds_md_files "devdoc/*.md")
SOURCE_GROUP(devdoc FILES ${clds_md_files})

add_library(clds
    ${clds_c_files}
    ${clds_h_files}
    ${clds_md_files}
    )

if((WIN32)
    # maybe this should make its way to c-pal at some point ... maybe
    add_subdirectory(thread_notifications_lackey_dll)
    target_link_libraries(clds c_util c_pal thread_notifications_lackey_dll)
endif()

function(copy_clds_outputs target output_directory)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE_DIR:thread_notifications_lackey_dll>/thread_notifications_lackey_dll.pdb;$<TARGET_FILE_DIR:thread_notifications_lackey_dll>/thread_notifications_lackey_dll.dll
        ${output_directory} )
    add_dependencies(${target} thread_notifications_lackey_dll)
endfunction()

# make an install target so we can produce a Linux native client package.
install_library_with_prefix(clds clds ${clds_h_files})

add_subdirectory(tests)

if(${run_reals_check})
    add_reals_check_target()
endif()
