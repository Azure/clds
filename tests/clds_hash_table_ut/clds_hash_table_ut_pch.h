// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

// Precompiled header for clds_hash_table_ut

#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "real_gballoc_ll.h"

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_stdint.h"
#include "umock_c/umocktypes_bool.h"
#include "umock_c/umock_c_negative_tests.h"

#include "c_pal/interlocked.h"

#define ENABLE_MOCKS
#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "c_pal/thandle.h"

#include "c_util/cancellation_token.h"

#include "clds/clds_sorted_list.h"
#include "clds/clds_st_hash_set.h"
#include "clds/clds_hazard_pointers.h"

#undef ENABLE_MOCKS
#include "umock_c/umock_c_prod.h"

#include "real_gballoc_hl.h"

#include "../reals/real_clds_st_hash_set.h"
#include "../reals/real_clds_hazard_pointers.h"
#include "../reals/real_clds_sorted_list.h"

#include "real_cancellation_token.h"

#include "clds/clds_hash_table.h"
