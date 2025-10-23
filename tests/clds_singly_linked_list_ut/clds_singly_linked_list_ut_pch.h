// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.See LICENSE file in the project root for full license information.

// Precompiled header for clds_singly_linked_list_ut

#ifndef CLDS_SINGLY_LINKED_LIST_UT_PCH_H
#define CLDS_SINGLY_LINKED_LIST_UT_PCH_H

#include <stdlib.h>

#include "macro_utils/macro_utils.h"
#include "testrunnerswitcher.h"

#include "real_gballoc_ll.h"

#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_stdint.h"

#include "umock_c/umock_c_ENABLE_MOCKS.h" // ============================== ENABLE_MOCKS

#include "c_pal/gballoc_hl.h"
#include "c_pal/gballoc_hl_redirect.h"
#include "clds/clds_st_hash_set.h"
#include "clds/clds_hazard_pointers.h"

#include "umock_c/umock_c_DISABLE_MOCKS.h" // ============================== DISABLE_MOCKS

#include "real_gballoc_hl.h"
#include "../reals/real_clds_st_hash_set.h"
#include "../reals/real_clds_hazard_pointers.h"

#include "clds/clds_singly_linked_list.h"

#endif // CLDS_SINGLY_LINKED_LIST_UT_PCH_H
