#ifndef CLDS_HASH_TABLE_H
#define CLDS_HASH_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "azure_c_shared_utility/umock_c_prod.h"

typedef struct CLDS_HASH_TABLE_TAG* CLDS_HASH_TABLE_HANDLE;

MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create);
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);

#ifdef __cplusplus
}
#endif

#endif /* CLDS_HASH_TABLE_H */
