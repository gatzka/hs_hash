/*
 * A table declared in a header and used by two source files.
 */

#ifndef SHARED_TABLE_H
#define SHARED_TABLE_H

#include <hs_hash/hs_hash.h>

DECLARE_HS_HASHTABLE_STRING(shared, 8, 1)

extern HS_HASHTABLE_TYPE(shared) *shared_table;

int shared_table_fill(void);

#endif
