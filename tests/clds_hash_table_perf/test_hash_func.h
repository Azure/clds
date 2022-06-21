// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef TEST_HASH_FUNC_H
#define TEST_HASH_FUNC_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif
    
uint64_t test_compute_hash(void* key);

#ifdef __cplusplus
}
#endif

#endif /* TEST_HASH_FUNC_H */
