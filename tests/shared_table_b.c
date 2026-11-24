#include "shared_table.h"
#include "test.h"

int main(void)
{
	struct value_shared out;

	CHECK(shared_table_fill() == HS_HASHTABLE_SUCCESS);
	if (shared_table != NULL) {
		CHECK(HS_HASHTABLE_GET(shared, shared_table, "key", &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == (void *)1);
		CHECK(HS_HASHTABLE_REMOVE(shared, shared_table, "key", NULL) == HS_HASHTABLE_SUCCESS);
		HS_HASHTABLE_DELETE(shared, shared_table);
	}

	if (test_failures != 0) {
		printf("shared_table: %lu checks failed\n", test_failures);
		return 1;
	}
	printf("shared_table: all checks passed\n");
	return 0;
}
