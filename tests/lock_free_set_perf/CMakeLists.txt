#Copyright (C) Microsoft Corporation. All rights reserved.

set(lock_free_set_perf_h_files
    lock_free_set_perf.h
)

set(lock_free_set_perf_c_files
    main.c
    lock_free_set_perf.c
)

set(lock_free_set_perf_rc_files
    ${LOGGING_RC_FILE}
)

add_executable(lock_free_set_perf ${lock_free_set_perf_h_files} ${lock_free_set_perf_c_files} ${lock_free_set_perf_rc_files})

target_link_libraries(lock_free_set_perf clds c_logging_v2)
