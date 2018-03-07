#Licensed under the MIT license. See LICENSE file in the project root for full license information.

set(clds_hash_table_perf_h_files
    clds_hash_table_perf.h
)

set(clds_hash_table_perf_c_files
    main.c
    clds_hash_table_perf.c
)

set(lock_free_tree_perf_rc_files
    ${LOGGING_RC_FILE}
)

add_executable(clds_hash_table_perf ${clds_hash_table_perf_h_files} ${clds_hash_table_perf_c_files} ${clds_hash_table_perf_rc_files})
target_link_libraries(clds_hash_table_perf clds)
