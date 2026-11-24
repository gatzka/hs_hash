/*
 * Helpers shared by the hs_hash tests.
 */

#ifndef HS_HASH_TEST_H
#define HS_HASH_TEST_H

#include <stdint.h>
#include <stdio.h>

static unsigned long test_failures;

/* Reports a failed check and continues. */
#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
			test_failures++; \
		} \
	} while (0)

/* Reports a failed check and returns from the current (void) function. */
#define REQUIRE(cond) \
	do { \
		if (!(cond)) { \
			printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
			test_failures++; \
			return; \
		} \
	} while (0)

#define TEST_PTR(x) ((void *)(uintptr_t)(x))

/* xorshift32. Unlike rand(), it gives the same numbers on every platform. */
static uint32_t test_random_state = 1;

static inline void test_random_seed(uint32_t seed)
{
	test_random_state = (seed == 0) ? 1 : seed;
}

static inline uint32_t test_random(void)
{
	uint32_t x = test_random_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	test_random_state = x;
	return x;
}

#endif
