#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(theseTestsName lru_cache_ut)

set(${theseTestsName}_test_files
${theseTestsName}.c
)

set(${theseTestsName}_c_files
../../src/lru_cache.c
)

set(${theseTestsName}_h_files
../../inc/clds/lru_cache.h
)

build_test_artifacts(${theseTestsName} "tests/clds" ADDITIONAL_LIBS c_pal_reals c_pal clds_reals)
