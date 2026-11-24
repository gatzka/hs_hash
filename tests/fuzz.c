/*
 * Random operations, checked against a simple reference model.
 *
 * HS_HASH_WMB() is replaced by a simulated interrupt that looks up every
 * key with HS_HASHTABLE_GET while PUT or REMOVE is in progress:
 * - Keys that are not being changed must be found with their current
 *   value, or not be found if they aren't in the table.
 * - The key being changed may be seen with its old or its new value, but
 *   never with a mix of both, and an update must never make it disappear.
 *
 * After each operation, the table invariants are checked: every hop_info
 * bit points to an occupied slot whose key hashes to that bucket, no slot
 * is claimed by two buckets, every occupied slot is claimed by a bucket
 * and empty slots have zeroed values.
 *
 * The key type is chosen with FUZZ_KEY_UINT32, FUZZ_KEY_UINT64 or
 * FUZZ_KEY_STRING. With FUZZ_SPLIT_PIECE defined, the test is built
 * against a copy of the header in which every hop_info store is done in
 * pieces of FUZZ_SPLIT_PIECE bytes, with a simulated interrupt after each
 * piece, like on an 8 or 16 bit CPU. FUZZ_SPLIT_REVERSE stores the pieces
 * from the last byte to the first.
 *
 * Usage: fuzz [operations [seed]]
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void simulated_interrupt(void);
#define HS_HASH_WMB() simulated_interrupt()

#ifdef FUZZ_SPLIT_PIECE
	#ifndef FUZZ_SPLIT_REVERSE
		#define FUZZ_SPLIT_REVERSE 0
	#endif
static void split_store(uint32_t *hop_info, uint32_t value);
	#define HS_TEST_STORE_HOP(ptr, value) split_store((ptr), (value))
#endif

#include <hs_hash/hs_hash.h>

#include "test.h"

#define ORDER 8
#define SLOTS (1u << ORDER)
/* Twice as many keys as slots: the table runs full and PUT often fails. */
#define KEYS (2 * SLOTS)

#if defined(FUZZ_KEY_UINT32)
DECLARE_HS_HASHTABLE_UINT32(fuzz, ORDER, 2)
typedef uint32_t key_type;
	#define KEY_HASH(key) hs_hash_func_fuzz_uint32_t(key)
static uint32_t key_values[KEYS];
static void init_keys(void)
{
	unsigned i;
	for (i = 0; i < KEYS; i++) {
		key_values[i] = (uint32_t)i * 2654435761u + 1;
	}
}
	#define STORED_KEY(i) key_values[i]
	#define LOOKUP_KEY(i) key_values[i]
#elif defined(FUZZ_KEY_UINT64)
DECLARE_HS_HASHTABLE_UINT64(fuzz, ORDER, 2)
typedef uint64_t key_type;
	#define KEY_HASH(key) hs_hash_func_fuzz_uint64_t(key)
static uint64_t key_values[KEYS];
static void init_keys(void)
{
	unsigned i;
	/* Keys that differ only in their upper 32 bits. */
	for (i = 0; i < KEYS; i++) {
		key_values[i] = ((uint64_t)(i + 1) << 32) | 7u;
	}
}
	#define STORED_KEY(i) key_values[i]
	#define LOOKUP_KEY(i) key_values[i]
#elif defined(FUZZ_KEY_STRING)
DECLARE_HS_HASHTABLE_STRING(fuzz, ORDER, 2)
typedef const char *key_type;
	#define KEY_HASH(key) hs_hash_func_fuzz_string(key)
/* PUT stores pointers into stored_names, GET and REMOVE look up equal */
/* strings in lookup_names. */
static char stored_names[KEYS][16];
static char lookup_names[KEYS][16];
static void init_keys(void)
{
	unsigned i;
	for (i = 0; i < KEYS; i++) {
		snprintf(stored_names[i], sizeof(stored_names[i]), "key%u", i);
		memcpy(lookup_names[i], stored_names[i], sizeof(lookup_names[i]));
	}
}
	#define STORED_KEY(i) ((const char *)stored_names[i])
	#define LOOKUP_KEY(i) ((const char *)lookup_names[i])
#else
	#error "define FUZZ_KEY_UINT32, FUZZ_KEY_UINT64 or FUZZ_KEY_STRING"
#endif

#define EMPTY_KEY ((key_type)HS_HASHTABLE_INVALIDENTRY)

static HS_HASHTABLE_TYPE(fuzz) *table;

/* Reference model. */
static int present[KEYS];
static uintptr_t current_value[KEYS];

/* The operation in progress. */
static int interrupts_enabled;
static int in_flight = -1;
static int had_old_value;
static uintptr_t old_value;
static int has_new_value;
static uintptr_t new_value;

static unsigned long operation;
static unsigned long interrupts;
static unsigned long puts_full;

static void report(const char *what, unsigned key)
{
	if (test_failures < 20) {
		printf("operation %lu, key %u: %s\n", operation, key, what);
	}
	test_failures++;
}

/* Both value words are written together, the second is the complement. */
static void make_value(struct value_fuzz *v, uintptr_t x)
{
	v->vals[0] = (void *)x;
	v->vals[1] = (void *)~x;
}

static int is_torn(const struct value_fuzz *v)
{
	return (uintptr_t)v->vals[1] != ~(uintptr_t)v->vals[0];
}

static void simulated_interrupt(void)
{
	unsigned i;

	if (!interrupts_enabled) {
		return;
	}
	interrupts++;

	for (i = 0; i < KEYS; i++) {
		struct value_fuzz out;
		int rc;
		uintptr_t seen;

		out.vals[0] = NULL;
		out.vals[1] = NULL;
		rc = HS_HASHTABLE_GET(fuzz, table, LOOKUP_KEY(i), &out);
		seen = (uintptr_t)out.vals[0];
		if (rc == HS_HASHTABLE_SUCCESS && is_torn(&out)) {
			report("interrupt: torn value", i);
		} else if ((int)i == in_flight) {
			if (rc == HS_HASHTABLE_SUCCESS) {
				if (!((had_old_value && seen == old_value) || (has_new_value && seen == new_value))) {
					report("interrupt: key being changed has an unexpected value", i);
				}
			} else if (had_old_value && has_new_value) {
				report("interrupt: key being updated is missing", i);
			}
		} else if (present[i]) {
			if (rc != HS_HASHTABLE_SUCCESS) {
				report("interrupt: key is missing", i);
			} else if (seen != current_value[i]) {
				report("interrupt: key has a wrong value", i);
			}
		} else if (rc != HS_HASHTABLE_NOTFOUND) {
			report("interrupt: removed key is found", i);
		}
	}

#ifndef FUZZ_KEY_STRING
	{
		struct value_fuzz out;
		if (HS_HASHTABLE_GET(fuzz, table, EMPTY_KEY, &out) != HS_HASHTABLE_NOTFOUND) {
			report("interrupt: the empty-slot marker is found", 0);
		}
	}
#endif
}

#ifdef FUZZ_SPLIT_PIECE
static void split_store(uint32_t *hop_info, uint32_t value)
{
	unsigned char bytes[sizeof(uint32_t)];
	unsigned char *target = (unsigned char *)hop_info;
	unsigned i;

	memcpy(bytes, &value, sizeof(bytes));
	for (i = 0; i < sizeof(bytes); i += FUZZ_SPLIT_PIECE) {
		unsigned offset = FUZZ_SPLIT_REVERSE ? (unsigned)sizeof(bytes) - FUZZ_SPLIT_PIECE - i : i;
		memcpy(target + offset, bytes + offset, FUZZ_SPLIT_PIECE);
		simulated_interrupt();
	}
}
#endif

static void check_invariants(void)
{
	static unsigned claimed[SLOTS];
	unsigned occupied = 0;
	unsigned live = 0;
	unsigned bucket;
	unsigned slot;
	unsigned i;

	memset(claimed, 0, sizeof(claimed));
	for (bucket = 0; bucket < SLOTS; bucket++) {
		uint32_t hop_info = table[bucket].hop_info;
		unsigned distance;
		for (distance = 0; distance < 32; distance++) {
			if ((hop_info & ((uint32_t)1 << distance)) != 0) {
				slot = (bucket + distance) & (SLOTS - 1);
				claimed[slot]++;
				if (table[slot].key == EMPTY_KEY) {
					report("hop_info bit points to an empty slot", slot);
				} else if (KEY_HASH(table[slot].key) != bucket) {
					report("hop_info bit points to a key of another bucket", slot);
				}
			}
		}
	}

	for (slot = 0; slot < SLOTS; slot++) {
		if (claimed[slot] > 1) {
			report("slot is claimed by several buckets", slot);
		}
		if (table[slot].key == EMPTY_KEY) {
			if (table[slot].value.vals[0] != NULL || table[slot].value.vals[1] != NULL) {
				report("empty slot has a value", slot);
			}
		} else {
			occupied++;
			if (claimed[slot] == 0) {
				report("occupied slot is not claimed by any bucket", slot);
			}
		}
	}

	for (i = 0; i < KEYS; i++) {
		struct value_fuzz out;
		int rc = HS_HASHTABLE_GET(fuzz, table, LOOKUP_KEY(i), &out);
		if (present[i]) {
			live++;
			if (rc != HS_HASHTABLE_SUCCESS || (uintptr_t)out.vals[0] != current_value[i] || is_torn(&out)) {
				report("key is missing or has a wrong value", i);
			}
		} else if (rc != HS_HASHTABLE_NOTFOUND) {
			report("removed key is found", i);
		}
	}
	if (occupied != live) {
		report("number of occupied slots differs from the number of keys", occupied);
	}
}

static void put(unsigned i)
{
	struct value_fuzz v;
	struct value_fuzz prev;
	uintptr_t x = ((uintptr_t)test_random() << 1) | 1; /* never 0 */
	int rc;

	has_new_value = 1;
	new_value = x;
	make_value(&v, x);
	prev.vals[0] = (void *)1;
	prev.vals[1] = (void *)1;

	rc = HS_HASHTABLE_PUT(fuzz, table, STORED_KEY(i), v, &prev);
	if (rc == HS_HASHTABLE_SUCCESS) {
		if (had_old_value) {
			if ((uintptr_t)prev.vals[0] != old_value || is_torn(&prev)) {
				report("update: wrong previous value", i);
			}
		} else if (prev.vals[0] != NULL || prev.vals[1] != NULL) {
			report("insert: previous value not zeroed", i);
		}
		present[i] = 1;
		current_value[i] = x;
	} else if (rc == HS_HASHTABLE_FULL) {
		puts_full++;
		if (prev.vals[0] != NULL || prev.vals[1] != NULL) {
			report("full: previous value not zeroed", i);
		}
	} else {
		report("PUT returned an unexpected value", i);
	}
}

static void remove_key(unsigned i)
{
	struct value_fuzz out;
	int rc = HS_HASHTABLE_REMOVE(fuzz, table, LOOKUP_KEY(i), &out);

	if (had_old_value) {
		if (rc != HS_HASHTABLE_SUCCESS || (uintptr_t)out.vals[0] != old_value || is_torn(&out)) {
			report("remove: key not removed or wrong value", i);
		}
		present[i] = 0;
	} else if (rc != HS_HASHTABLE_NOTFOUND) {
		report("remove: missing key reported as removed", i);
	}
}

int main(int argc, char **argv)
{
	unsigned long operations = (argc > 1) ? strtoul(argv[1], NULL, 10) : 20000;
	uint32_t seed = (argc > 2) ? (uint32_t)strtoul(argv[2], NULL, 10) : 1;

	init_keys();
	{
		unsigned i;
		for (i = 0; i < KEYS; i++) {
			CHECK(STORED_KEY(i) != EMPTY_KEY);
		}
	}
	test_random_seed(seed);
	table = HS_HASHTABLE_CREATE(fuzz);
	if (table == NULL) {
		printf("fuzz: out of memory\n");
		return 1;
	}

	interrupts_enabled = 1;
	for (operation = 0; operation < operations; operation++) {
		unsigned i = test_random() % KEYS;

		in_flight = (int)i;
		had_old_value = present[i];
		old_value = current_value[i];
		has_new_value = 0;
		if (test_random() % 3 != 0) {
			put(i);
		} else {
			remove_key(i);
		}
		in_flight = -1;

		interrupts_enabled = 0;
		check_invariants();
		interrupts_enabled = 1;
	}

	/* Make sure the interesting paths were taken. */
	CHECK(interrupts > operations);
	CHECK(puts_full > 0);

	HS_HASHTABLE_DELETE(fuzz, table);
	printf("fuzz: %lu operations, %lu FULL, %lu interrupts, %lu failures\n", operations, puts_full, interrupts, test_failures);
	return (test_failures != 0) ? 1 : 0;
}
