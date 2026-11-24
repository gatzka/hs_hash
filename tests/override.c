/*
 * HS_HASH_MALLOC, HS_HASH_FREE and HS_HASH_WMB can be replaced by
 * defining them before including the header.
 */

#include <stdlib.h>

static unsigned allocations;
static unsigned frees;
static unsigned barriers;
static int fail_allocation;

static void *test_malloc(size_t size)
{
	allocations++;
	return fail_allocation ? NULL : malloc(size);
}

static void test_free(void *ptr)
{
	frees++;
	free(ptr);
}

#define HS_HASH_MALLOC(size) test_malloc(size)
#define HS_HASH_FREE(ptr) test_free(ptr)
#define HS_HASH_WMB() (barriers++)

#include <hs_hash/hs_hash.h>

#include "test.h"

DECLARE_HS_HASHTABLE_UINT32(t, 8, 1)

int main(void)
{
	HS_HASHTABLE_TYPE(t) *table = HS_HASHTABLE_CREATE(t);
	struct value_t v;

	CHECK(table != NULL && allocations == 1);
	if (table != NULL) {
		v.vals[0] = NULL;
		CHECK(HS_HASHTABLE_PUT(t, table, 1, v, NULL) == HS_HASHTABLE_SUCCESS);
		CHECK(HS_HASHTABLE_REMOVE(t, table, 1, NULL) == HS_HASHTABLE_SUCCESS);
		/* One barrier to publish the new entry, one to unpublish it. */
		CHECK(barriers == 2);
		HS_HASHTABLE_DELETE(t, table);
		CHECK(frees == 1);
	}

	/* CREATE returns NULL if the allocation fails. */
	fail_allocation = 1;
	table = HS_HASHTABLE_CREATE(t);
	CHECK(table == NULL && allocations == 2);

	if (test_failures != 0) {
		printf("override: %lu checks failed\n", test_failures);
		return 1;
	}
	printf("override: all checks passed\n");
	return 0;
}
