#include "shared_table.h"

HS_HASHTABLE_TYPE(shared) *shared_table;

int shared_table_fill(void)
{
	struct value_shared v;

	shared_table = HS_HASHTABLE_CREATE(shared);
	if (shared_table == NULL) {
		return -1;
	}
	v.vals[0] = (void *)1;
	return HS_HASHTABLE_PUT(shared, shared_table, "key", v, NULL);
}
