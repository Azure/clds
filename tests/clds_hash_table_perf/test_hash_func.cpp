// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <cstring>

#include "MurmurHash2.h"
#include "test_hash_func.h"

uint64_t test_compute_hash(void* key)
{
    const char* test_key = (const char*)key;
    return (uint64_t)MurmurHash2(test_key, (int)strlen(test_key), 0);
}
