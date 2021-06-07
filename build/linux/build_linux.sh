#!/bin/bash
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE file in the project root for full license information.

set -e

build_root=$(cd "$1" && pwd)
cd $build_root

build_folder=$build_root"/cmake_linux"

# Set the default cores
CORES=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)

rm -r -f $build_folder
mkdir -p $build_folder
pushd $build_folder
cmake -Drun_valgrind:BOOL=ON -Drun_helgrind:BOOL=ON -Drun_drd:BOOL=OFF $build_root -Drun_unittests:BOOL=ON -Drun_int_tests:BOOL=ON
make --jobs=$CORES

# /*reenable with this task Task 10086393: reenable sm_chaos (https://msazure.visualstudio.com/One/_workitems/edit/10086393)*/ 
ctest -j $CORES --output-on-failure -E clds_sorted_list_int_valgrind|clds_sorted_list_int_helgrind

popd 