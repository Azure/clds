// Copyright (c) Microsoft. All rights reserved.

#define GBALLOC_H

#define clds_hazard_pointers_create real_clds_hazard_pointers_create
#define clds_hazard_pointers_destroy real_clds_hazard_pointers_destroy
#define clds_hazard_pointers_register_thread real_clds_hazard_pointers_register_thread
#define clds_hazard_pointers_unregister_thread real_clds_hazard_pointers_unregister_thread
#define clds_hazard_pointers_acquire real_clds_hazard_pointers_acquire
#define clds_hazard_pointers_release real_clds_hazard_pointers_release
#define clds_hazard_pointers_reclaim real_clds_hazard_pointers_reclaim

#include "../src/clds_hazard_pointers.cpp"
