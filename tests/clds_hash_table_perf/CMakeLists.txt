#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(clds_hash_table_perf_h_files
    clds_hash_table_perf.h
    test_hash_func.h
)

set(clds_hash_table_perf_c_files
    main.c
    clds_hash_table_perf.c
    test_hash_func.cpp
)

set(clds_hash_table_perf_rc_files
    ${LOGGING_RC_FILE}
)

if (TARGET SMHasherSupport)
    add_executable(clds_hash_table_perf ${clds_hash_table_perf_h_files} ${clds_hash_table_perf_c_files} ${clds_hash_table_perf_rc_files})
    target_link_libraries(clds_hash_table_perf clds SMHasherSupport c_logging)
else()
    # if we don't have SMHasher, just add a lib so that we compile (for the purposes of Ninja+iwyu)
    add_library(clds_hash_table_perf ${clds_hash_table_perf_h_files} ${clds_hash_table_perf_c_files} ${clds_hash_table_perf_rc_files})
endif()
