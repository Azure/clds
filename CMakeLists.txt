#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

# canon way of using clds from another repo is below. It assumes the using repo has placed clds in "deps"

#if ((NOT TARGET azure_c_util) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/clds/CMakeLists.txt))
#    add_subdirectory(deps/clds)
#    include_directories(${CMAKE_CURRENT_LIST_DIR}/deps/clds/inc)
#endif() 

project(clds)

include(CTest)

option(run_e2e_tests "set run_e2e_tests to ON to run e2e tests (default is OFF)" OFF)
option(run_unittests "set run_unittests to ON to run unittests (default is OFF)" OFF)
option(run_traceability "run traceability tool (default is ON)" ON)

#bring in dependencies
#do not add or build any tests of the dependencies
set(original_run_e2e_tests ${run_e2e_tests})
set(original_run_unittests ${run_unittests})
set(original_run_int_tests ${run_int_tests})
set(original_run_traceability ${run_traceability})
set(original_run_perf_tests ${run_perf_tests})

set(run_e2e_tests OFF)
set(run_unittests OFF)
set(run_int_tests OFF)
set(run_traceability OFF)
set(run_perf_tests OFF)


if ((NOT TARGET azure_c_build_tools) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-build-tools/CMakeLists.txt))
    add_subdirectory(deps/azure-c-build-tools)
    set_default_build_options()
endif() 

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

if ((NOT TARGET azure_c_pal) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-pal/CMakeLists.txt))
    add_subdirectory(deps/azure-c-pal)
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

if ((NOT TARGET azure_c_util) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-util/CMakeLists.txt))
    add_subdirectory(deps/azure-c-util)
    include_directories(${C_UTIL_INC_FOLDER})
endif() 

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

set(run_e2e_tests ${original_run_e2e_tests})
set(run_unittests ${original_run_unittests})
set(run_int_tests ${original_run_int_tests})
set(run_traceability ${original_run_traceability})
set(run_perf_tests ${original_run_perf_tests})

if((WIN32) AND (${run_traceability}))
    #add traceability custom target
    add_custom_target(clds_traceability ALL
        COMMAND traceabilitytool -buildcheck -e ${CMAKE_CURRENT_LIST_DIR}/deps -i ${CMAKE_CURRENT_LIST_DIR})
    add_dependencies(clds_traceability traceabilitytool)
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR}/inc)

set(clds_h_files
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

target_link_libraries(clds azure_c_util azure_c_pal)

add_subdirectory(tests)
