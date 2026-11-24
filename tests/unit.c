/*
 * Unit tests for hs_hash. They use the default HS_HASH_MALLOC, HS_HASH_FREE
 * and HS_HASH_WMB, so they also cover the platform's default write barrier.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <hs_hash/hs_hash.h>

#include "test.h"

DECLARE_HS_HASHTABLE_UINT32(u32, 8, 1)
DECLARE_HS_HASHTABLE_UINT64(u64, 8, 2)
DECLARE_HS_HASHTABLE_STRING(str, 8, 2)
/* 8 slots and a search range of 4 slots: easy to fill completely. */
DECLARE_HS_HASHTABLE_UINT32(tiny, 3, 1)
/* 4096 slots: the search range is limited by HS_HASH_ADD_RANGE (1024). */
DECLARE_HS_HASHTABLE_UINT32(range, 12, 1)
DECLARE_HS_HASHTABLE_UINT32(load13, 13, 1)
DECLARE_HS_HASHTABLE_UINT32(load16, 16, 1)

static void test_uint32_keys(void)
{
	HS_HASHTABLE_TYPE(u32) *t = HS_HASHTABLE_CREATE(u32);
	struct value_u32 v;
	struct value_u32 out;

	REQUIRE(t != NULL);
	v.vals[0] = TEST_PTR(1);
	CHECK(HS_HASHTABLE_PUT(u32, t, 42, v, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_GET(u32, t, 42, &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(1));

	/* A miss leaves the value untouched. */
	out.vals[0] = TEST_PTR(77);
	CHECK(HS_HASHTABLE_GET(u32, t, 43, &out) == HS_HASHTABLE_NOTFOUND && out.vals[0] == TEST_PTR(77));

	CHECK(HS_HASHTABLE_REMOVE(u32, t, 42, &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(1));
	out.vals[0] = TEST_PTR(77);
	CHECK(HS_HASHTABLE_GET(u32, t, 42, &out) == HS_HASHTABLE_NOTFOUND && out.vals[0] == TEST_PTR(77));
	CHECK(HS_HASHTABLE_REMOVE(u32, t, 42, &out) == HS_HASHTABLE_NOTFOUND && out.vals[0] == TEST_PTR(77));
	CHECK(HS_HASHTABLE_REMOVE(u32, t, 42, NULL) == HS_HASHTABLE_NOTFOUND);

	/* 0 is a valid key, the empty-slot marker isn't. */
	CHECK(HS_HASHTABLE_PUT(u32, t, 0, v, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_GET(u32, t, 0, &out) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_PUT(u32, t, UINT32_MAX, v, NULL) == HS_HASHTABLE_KEYINVAL);
	CHECK(HS_HASHTABLE_GET(u32, t, UINT32_MAX, &out) == HS_HASHTABLE_NOTFOUND);

	HS_HASHTABLE_DELETE(u32, t);
	CHECK(t == NULL);
}

static void test_uint64_keys(void)
{
	HS_HASHTABLE_TYPE(u64) *t = HS_HASHTABLE_CREATE(u64);
	/* Same lower 32 bits: the upper bits must count. */
	const uint64_t k1 = 1;
	const uint64_t k2 = (UINT64_C(1) << 32) | 1;
	struct value_u64 v1;
	struct value_u64 v2;
	struct value_u64 out;

	REQUIRE(t != NULL);
	v1.vals[0] = TEST_PTR(1);
	v1.vals[1] = TEST_PTR(2);
	v2.vals[0] = TEST_PTR(3);
	v2.vals[1] = TEST_PTR(4);
	CHECK(HS_HASHTABLE_PUT(u64, t, k1, v1, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_PUT(u64, t, k2, v2, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_GET(u64, t, k1, &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(1) && out.vals[1] == TEST_PTR(2));
	CHECK(HS_HASHTABLE_GET(u64, t, k2, &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(3) && out.vals[1] == TEST_PTR(4));
	CHECK(HS_HASHTABLE_REMOVE(u64, t, k1, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_GET(u64, t, k1, &out) == HS_HASHTABLE_NOTFOUND);
	CHECK(HS_HASHTABLE_GET(u64, t, k2, &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(3));
	CHECK(HS_HASHTABLE_PUT(u64, t, UINT64_MAX, v1, NULL) == HS_HASHTABLE_KEYINVAL);

	HS_HASHTABLE_DELETE(u64, t);
}

static unsigned used_slots_str(const HS_HASHTABLE_TYPE(str) *t)
{
	unsigned used = 0;
	unsigned i;

	for (i = 0; i < (1u << 8); i++) {
		if (t[i].key != (const char *)HS_HASHTABLE_INVALIDENTRY) {
			used++;
		}
	}
	return used;
}

static void test_string_keys(void)
{
	HS_HASHTABLE_TYPE(str) *t = HS_HASHTABLE_CREATE(str);
	static const char stored[] = "abc";
	static const char non_ascii[] = "Gr\xc3\xb6\xc3\x9f" "e";
	char copy[8];
	struct value_str v1;
	struct value_str v2;
	struct value_str prev;
	struct value_str out;

	REQUIRE(t != NULL);
	v1.vals[0] = TEST_PTR(1);
	v1.vals[1] = TEST_PTR(2);
	v2.vals[0] = TEST_PTR(3);
	v2.vals[1] = TEST_PTR(4);

	/* Insert: prev_value is zeroed. */
	prev.vals[0] = TEST_PTR(99);
	CHECK(HS_HASHTABLE_PUT(str, t, stored, v1, &prev) == HS_HASHTABLE_SUCCESS && prev.vals[0] == NULL);

	/* Update through a different pointer to an equal string: prev_value is the old value. */
	memcpy(copy, stored, sizeof(stored));
	CHECK(HS_HASHTABLE_PUT(str, t, copy, v2, &prev) == HS_HASHTABLE_SUCCESS);
	CHECK(prev.vals[0] == TEST_PTR(1) && prev.vals[1] == TEST_PTR(2));
	CHECK(used_slots_str(t) == 1);

	/* The table keeps the pointer of the first PUT, not the one of the update. */
	memset(copy, 'x', sizeof(copy) - 1);
	copy[sizeof(copy) - 1] = '\0';
	CHECK(HS_HASHTABLE_GET(str, t, "abc", &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(3) && out.vals[1] == TEST_PTR(4));
	CHECK(HS_HASHTABLE_GET(str, t, copy, &out) == HS_HASHTABLE_NOTFOUND);

	CHECK(HS_HASHTABLE_REMOVE(str, t, "abc", &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(3));
	CHECK(HS_HASHTABLE_GET(str, t, "abc", &out) == HS_HASHTABLE_NOTFOUND);
	CHECK(used_slots_str(t) == 0);

	/* The empty string and non-ASCII strings are valid keys. */
	CHECK(HS_HASHTABLE_PUT(str, t, "", v1, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_PUT(str, t, non_ascii, v2, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_GET(str, t, "", &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(1));
	CHECK(HS_HASHTABLE_GET(str, t, non_ascii, &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(3));

	/* Characters are hashed as unsigned char, whether char is signed or not. */
	CHECK(hs_hash_func_str_string("\xc3") == hs_hash32(0xc3, 8));

	CHECK(HS_HASHTABLE_PUT(str, t, (const char *)HS_HASHTABLE_INVALIDENTRY, v1, NULL) == HS_HASHTABLE_KEYINVAL);

	HS_HASHTABLE_DELETE(str, t);
}

static void test_return_codes(void)
{
	CHECK(HS_HASHTABLE_SUCCESS == 0);
	CHECK(HS_HASHTABLE_SUCCESS != HS_HASHTABLE_NOTFOUND);
	CHECK(HS_HASHTABLE_SUCCESS != HS_HASHTABLE_KEYINVAL);
	CHECK(HS_HASHTABLE_SUCCESS != HS_HASHTABLE_FULL);
	CHECK(HS_HASHTABLE_NOTFOUND != HS_HASHTABLE_KEYINVAL);
	CHECK(HS_HASHTABLE_NOTFOUND != HS_HASHTABLE_FULL);
	CHECK(HS_HASHTABLE_KEYINVAL != HS_HASHTABLE_FULL);
	/* Code that compares GET's result with HS_HASHTABLE_INVALIDENTRY keeps working. */
	CHECK(HS_HASHTABLE_NOTFOUND == HS_HASHTABLE_INVALIDENTRY);
}

/* An update needs a free slot. If there is none, the old value must stay. */
static void test_update_in_full_table(void)
{
	HS_HASHTABLE_TYPE(tiny) *t = HS_HASHTABLE_CREATE(tiny);
	struct value_tiny a;
	struct value_tiny b;
	struct value_tiny prev;
	struct value_tiny out;
	uint32_t keys[8];
	unsigned n = 0;
	unsigned updated = 0;
	uint32_t k;
	unsigned i;

	REQUIRE(t != NULL);
	a.vals[0] = TEST_PTR(10);
	b.vals[0] = TEST_PTR(20);
	for (k = 0; n < 8 && k < 100000; k++) {
		if (HS_HASHTABLE_PUT(tiny, t, k, a, NULL) == HS_HASHTABLE_SUCCESS) {
			keys[n++] = k;
		}
	}
	REQUIRE(n == 8);

	prev.vals[0] = TEST_PTR(99);
	CHECK(HS_HASHTABLE_PUT(tiny, t, keys[3], b, &prev) == HS_HASHTABLE_FULL && prev.vals[0] == NULL);
	CHECK(HS_HASHTABLE_GET(tiny, t, keys[3], &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(10));

	/* With one free slot, updates work again. */
	CHECK(HS_HASHTABLE_REMOVE(tiny, t, keys[0], NULL) == HS_HASHTABLE_SUCCESS);
	for (i = 1; i < 8; i++) {
		if (HS_HASHTABLE_PUT(tiny, t, keys[i], b, NULL) == HS_HASHTABLE_SUCCESS) {
			updated++;
			CHECK(HS_HASHTABLE_GET(tiny, t, keys[i], &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(20));
		}
	}
	CHECK(updated >= 1);

	HS_HASHTABLE_DELETE(tiny, t);
}

static void test_delete_is_one_statement(void)
{
	HS_HASHTABLE_TYPE(u32) *t = HS_HASHTABLE_CREATE(u32);
	int delete_it = 0;

	REQUIRE(t != NULL);
	if (delete_it)
		HS_HASHTABLE_DELETE(u32, t);
	CHECK(t != NULL);

	if (t != NULL)
		HS_HASHTABLE_DELETE(u32, t);
	else
		CHECK(0);
	CHECK(t == NULL);
}

static void test_const_table(void)
{
	HS_HASHTABLE_TYPE(u32) *t = HS_HASHTABLE_CREATE(u32);
	const HS_HASHTABLE_TYPE(u32) *readonly = t;
	struct value_u32 v;
	struct value_u32 out;

	REQUIRE(t != NULL);
	v.vals[0] = TEST_PTR(5);
	CHECK(HS_HASHTABLE_PUT(u32, t, 5, v, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_GET(u32, readonly, 5, &out) == HS_HASHTABLE_SUCCESS && out.vals[0] == TEST_PTR(5));
	HS_HASHTABLE_DELETE(u32, t);
}

static void test_independent_tables(void)
{
	HS_HASHTABLE_TYPE(u32) *a = HS_HASHTABLE_CREATE(u32);
	HS_HASHTABLE_TYPE(tiny) *b = HS_HASHTABLE_CREATE(tiny);
	struct value_u32 va;
	struct value_u32 oa;
	struct value_tiny vb;
	struct value_tiny ob;

	REQUIRE(a != NULL && b != NULL);
	va.vals[0] = TEST_PTR(1);
	vb.vals[0] = TEST_PTR(2);
	CHECK(HS_HASHTABLE_PUT(u32, a, 7, va, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_PUT(tiny, b, 7, vb, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_GET(u32, a, 7, &oa) == HS_HASHTABLE_SUCCESS && oa.vals[0] == TEST_PTR(1));
	CHECK(HS_HASHTABLE_GET(tiny, b, 7, &ob) == HS_HASHTABLE_SUCCESS && ob.vals[0] == TEST_PTR(2));
	CHECK(HS_HASHTABLE_REMOVE(u32, a, 7, NULL) == HS_HASHTABLE_SUCCESS);
	CHECK(HS_HASHTABLE_GET(tiny, b, 7, &ob) == HS_HASHTABLE_SUCCESS);
	HS_HASHTABLE_DELETE(u32, a);
	HS_HASHTABLE_DELETE(tiny, b);
}

/*
 * PUT searches for a free slot at most HS_HASH_ADD_RANGE slots away from
 * the key's bucket, even if the table has free slots further away.
 */
static void test_search_range(void)
{
	enum { FIRST = 100, RUN = 1100 }; /* RUN > HS_HASH_ADD_RANGE */
	static uint32_t keys[RUN];
	static unsigned char found[RUN];
	HS_HASHTABLE_TYPE(range) *t = HS_HASHTABLE_CREATE(range);
	struct value_range v;
	unsigned n = 0;
	uint32_t k;
	unsigned i;

	REQUIRE(t != NULL);
	v.vals[0] = TEST_PTR(1);

	/* One key for each bucket FIRST to FIRST + RUN - 1. */
	for (k = 0; n < RUN; k++) {
		uint32_t bucket = hs_hash_func_range_uint32_t(k);
		if (bucket >= FIRST && bucket < FIRST + RUN && !found[bucket - FIRST]) {
			found[bucket - FIRST] = 1;
			keys[bucket - FIRST] = k;
			n++;
		}
	}
	/* Each key lands in its own bucket: slots FIRST to FIRST + RUN - 1 are occupied. */
	for (i = 0; i < RUN; i++) {
		CHECK(HS_HASHTABLE_PUT(range, t, keys[i], v, NULL) == HS_HASHTABLE_SUCCESS);
	}

	/* The next free slot for another key of bucket FIRST is RUN slots away. */
	for (k = keys[0] + 1; hs_hash_func_range_uint32_t(k) != FIRST; k++) {
	}
	CHECK(HS_HASHTABLE_PUT(range, t, k, v, NULL) == HS_HASHTABLE_FULL);

	HS_HASHTABLE_DELETE(range, t);
}

/*
 * The documentation recommends filling a table to at most about 75%.
 * With random keys, PUT must not fail before that.
 */
#define FILL_TO_75_PERCENT(name, order) \
	do { \
		uint32_t seed_; \
		for (seed_ = 1; seed_ <= 10; seed_++) { \
			HS_HASHTABLE_TYPE(name) *t_ = HS_HASHTABLE_CREATE(name); \
			struct value_##name v_; \
			uint32_t n_ = 0; \
			int rc_ = HS_HASHTABLE_SUCCESS; \
			REQUIRE(t_ != NULL); \
			v_.vals[0] = NULL; \
			test_random_seed(seed_ * 2654435761u); \
			while (n_ < (3u << (order)) / 4 && rc_ == HS_HASHTABLE_SUCCESS) { \
				uint32_t k_ = test_random(); \
				struct value_##name o_; \
				if (k_ == UINT32_MAX || HS_HASHTABLE_GET(name, t_, k_, &o_) == HS_HASHTABLE_SUCCESS) { \
					continue; \
				} \
				rc_ = HS_HASHTABLE_PUT(name, t_, k_, v_, NULL); \
				n_++; \
			} \
			if (rc_ != HS_HASHTABLE_SUCCESS) { \
				printf("order %d, seed %u: PUT failed with %u of %u slots used\n", (order), (unsigned)seed_, (unsigned)(n_ - 1), 1u << (order)); \
			} \
			CHECK(rc_ == HS_HASHTABLE_SUCCESS); \
			HS_HASHTABLE_DELETE(name, t_); \
		} \
	} while (0)

static void test_load(void)
{
	FILL_TO_75_PERCENT(load13, 13);
	FILL_TO_75_PERCENT(load16, 16);
}

int main(void)
{
	test_uint32_keys();
	test_uint64_keys();
	test_string_keys();
	test_return_codes();
	test_update_in_full_table();
	test_delete_is_one_statement();
	test_const_table();
	test_independent_tables();
	test_search_range();
	test_load();

	if (test_failures != 0) {
		printf("unit: %lu checks failed\n", test_failures);
		return 1;
	}
	printf("unit: all checks passed\n");
	return 0;
}
