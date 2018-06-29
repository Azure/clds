#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

if (NOT TARGET azure_macro_utils_c)
	add_subdirectory(azure-macro-utils-c)
	include_directories(${CMAKE_CURRENT_LIST_DIR}/azure-macro-utils-c/inc)
endif()

if (NOT TARGET ctest)
	add_subdirectory(azure-ctest)
	include_directories(${CTEST_INC_FOLDER})
endif()

if (NOT TARGET testrunnerswitcher)
	add_subdirectory(azure-c-testrunnerswitcher)
	include_directories(${TESTRUNNERSWITCHER_INC_FOLDER})
endif()

if (NOT TARGET umock_c)
	add_subdirectory(umock-c)
	include_directories(${UMOCK_C_INC_FOLDER})
endif()

if (NOT TARGET aziotsharedutil)
	add_subdirectory(azure-c-shared-utility)
	include_directories(${SHARED_UTIL_INC_FOLDER})
endif()
