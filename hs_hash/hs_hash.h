/*
 *The MIT License (MIT)
 *
 * Copyright (c) <2020> <Stephan Gatzka>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * General description. This file implements a hash table based on an
 * algorithm called hopscotch hashing. Hopscotch hashing is an closed
 * hashing algorithm with open addressing. The main idea is to encode
 * the distance to the calculated hash position in a bitmap associated
 * with each hash index. When inserting a key/value pair the algorithm
 * tries to reorder hash table entries that each entry is in the maximum
 * distance the bitmap can contain. (hop_range in this implementation).
 *
 * Concurrency: the hash table supports one writer and readers that
 * interrupt the writer. Every HS_HASHTABLE_PUT or HS_HASHTABLE_REMOVE
 * call can be interrupted at any time by HS_HASHTABLE_GET calls, e.g.
 * from an interrupt handler. Because HS_HASHTABLE_GET does not alter the
 * hash table, concurrent calls to HS_HASHTABLE_GET are also allowed.
 *
 * The opposite is NOT supported: an HS_HASHTABLE_GET call must not be
 * interrupted by HS_HASHTABLE_PUT or HS_HASHTABLE_REMOVE. While the GET
 * is suspended, the writer might move or remove entries, so the GET
 * might report a present key as not found. For string tables it might
 * also access a key string that was removed and freed in the meantime.
 * For the same reason, readers must run on the same CPU as the writer,
 * not in parallel on another CPU.
 *
 * Please be aware that concurrent calls to HS_HASHTABLE_REMOVE or
 * HS_HASHTABLE_PUT are not allowed and must be synchronized elsewhere.
 *
 * Please use only the macros DECLARE_HS_HASHTABLE_STRING,
 * DECLARE_HS_HASHTABLE_UINT32, DECLARE_HS_HASHTABLE_UINT64,
 * HS_HASHTABLE_TYPE, HS_HASHTABLE_CREATE, HS_HASHTABLE_DELETE,
 * HS_HASHTABLE_GET, HS_HASHTABLE_REMOVE and HS_HASHTABLE_PUT.
 * Do not call the other functions or use the other types directly.
 *
 * Please note that in the Linux kernel HS_HASHTABLE_CREATE shall only
 * be called from process context, never from IRQ context, because it
 * allocates memory with kvmalloc(GFP_KERNEL). Large tables may come
 * from vmalloc() and are not physically contiguous then. This requires
 * Linux 4.12 or newer. HS_HASHTABLE_DELETE must not be called from NMI
 * context.
 *
 * The typical usage for example is:
 * DECLARE_HS_HASHTABLE_UINT32(CANHARDWARE_HASHTABLE, 13)
 * hashtable = HS_HASHTABLE_CREATE(CANHARDWARE_HASHTABLE);
 * HS_HASHTABLE_PUT(CANHARDWARE_HASHTABLE, hashtable, key, value, prev_value);
 * value = HS_HASHTABLE_GET(CANHARDWARE_HASHTABLE, hashtable, key);
 */

#ifndef HS_HASHTABLE_H
#define HS_HASHTABLE_H

/*
 * Platform abstraction. Outside the Linux kernel, the following macros
 * can be defined before including this file to replace the defaults:
 *
 * HS_HASH_MALLOC(size): allocates the hash table (default: malloc).
 * HS_HASH_FREE(ptr):    frees the hash table (default: free).
 * HS_HASH_WMB():        write barrier that keeps all stores issued
 *                       before it ordered before all stores issued
 *                       after it (default: a release fence).
 */
#ifdef __KERNEL__
	#include <linux/mm.h>
	#include <linux/slab.h>
	#include <linux/string.h>
	#include <linux/types.h>

	/* kmalloc() can't allocate more than KMALLOC_MAX_SIZE (typically */
	/* 4 MiB) and large physically contiguous allocations may fail once */
	/* memory is fragmented. kvmalloc() falls back to vmalloc() then. */
	#define HS_HASH_MALLOC(size) kvmalloc((size), GFP_KERNEL)
	#define HS_HASH_FREE(ptr) kvfree(ptr)
	#define HS_HASH_WMB() wmb()
#else
	#include <stdint.h>
	#include <stdlib.h>
	#include <string.h>

	#ifndef HS_HASH_MALLOC
		#define HS_HASH_MALLOC(size) malloc(size)
	#endif

	#ifndef HS_HASH_FREE
		#define HS_HASH_FREE(ptr) free(ptr)
	#endif

	#ifndef HS_HASH_WMB
		#if defined(__GNUC__) || defined(__clang__)
			#define HS_HASH_WMB() __atomic_thread_fence(__ATOMIC_RELEASE)
		#elif defined(_MSC_VER)
			#include <intrin.h>
			#if defined(_M_ARM64)
				#define HS_HASH_WMB() __dmb(_ARM64_BARRIER_ISHST)
			#elif defined(_M_ARM)
				#define HS_HASH_WMB() __dmb(_ARM_BARRIER_ISHST)
			#else
				/* x86 and x64 never reorder stores with other stores. */
				#define HS_HASH_WMB() _ReadWriteBarrier()
			#endif
		#else
			#error "hs_hash: no write barrier known for this compiler, please define HS_HASH_WMB()"
		#endif
	#endif
#endif

/*
 * HS_HASH_ADD_RANGE: how far from a key's bucket PUT searches for a free
 * slot (at most half the table size). This bounds the time a single PUT
 * can take. Define it before including this file to change the default.
 */
#ifndef HS_HASH_ADD_RANGE
	#define HS_HASH_ADD_RANGE 1024
#endif
#if (HS_HASH_ADD_RANGE) < 1
	#error "hs_hash: HS_HASH_ADD_RANGE must be at least 1"
#endif

/*
 * The functions declared by the DECLARE_HS_HASHTABLE_* macros are
 * expanded in the user's source file, so clang reports every one the
 * program doesn't call (-Wunused-function), unlike static inline
 * functions defined in a header.
 */
#if defined(__GNUC__) || defined(__clang__)
	#define HS_HASH_UNUSED __attribute__((unused))
#else
	#define HS_HASH_UNUSED
#endif

/* Return values. */
#define HS_HASHTABLE_SUCCESS 0
/* GET, REMOVE: the key is not in the table. Same value as */
/* HS_HASHTABLE_INVALIDENTRY, which GET and REMOVE returned before. */
#define HS_HASHTABLE_NOTFOUND (-1)
/* PUT: the key is HS_HASHTABLE_INVALIDENTRY, which can't be stored. */
#define HS_HASHTABLE_KEYINVAL (-2)
/* PUT: no free slot near the key's bucket. */
#define HS_HASHTABLE_FULL (-3)

/* Key value that marks an empty slot. It can't be used as a key. */
#define HS_HASHTABLE_INVALIDENTRY (-1)

static inline uint32_t hs_hash32(uint32_t key, unsigned int order)
{
	key = (key ^ 61) ^ (key >> 16);
	key = key + (key << 3);
	key = key ^ (key >> 4);
	key = key * 0x27d4eb2d;
	key = key ^ (key >> 15);
	return (key >> (32 - (order)));
}

static inline uint32_t hs_hash6432shift(uint64_t key, unsigned int order)
{
	key = (~key) + (key << 18);
	key = key ^ (key >> 31);
	key = key * 21;
	key = key ^ (key >> 11);
	key = key + (key << 6);
	key = key ^ (key >> 22);
	return ((uint32_t)key) >> (32 - (order));
}

/*
 * s1 is the key stored in the table, s2 the key searched for. Within the
 * supported concurrency model, s1 is never HS_HASHTABLE_INVALIDENTRY.
 * The check is a safety net for an HS_HASHTABLE_GET that is (wrongly)
 * interrupted by the writer and then follows its stale hop_info to a
 * slot that was invalidated meanwhile: report "not equal" instead of
 * calling strcmp() on an invalid pointer.
 */
static inline int hs_is_equal_string(const char *s1, const char *s2)
{
	return (s1 != (const char *)HS_HASHTABLE_INVALIDENTRY) && !strcmp(s1, s2);
}

static inline int hs_is_equal_uint32_t(uint32_t a, uint32_t b)
{
	return a == b;
}

static inline int hs_is_equal_uint64_t(uint64_t a, uint64_t b)
{
	return a == b;
}

/*
 * Declares a hash table of name "name" and size 2^order. This macro is
 * just for internal use. Do not use it to declare a hash table, use
 * DECLARE_HASHTABLE_STRING, DECLARE_HASHTABLE_UINT32 or
 * DECLARE_HASHTABLE_UINT64 instead.
 *
 * There are two different macro parameters required for the type of the
 * hash table: type_name and type. type_name denotes the unique name
 * that must be appended to all typed functions, type denotes the
 * underlying C type that is used in the hash table. This is only
 * required for the case of a string hash table. Here we have "type_name
 * string" and type "const char *". Please see also
 * DECLARE_HASHTABLE_STRING.
 */
#define DECLARE_HS_HASHTABLE(name, order, type_name, type, value_entries) \
\
/* Compile time check (C99 compatible): an invalid order gives a negative array size. */ \
typedef char hs_hash_order_of_##name##_must_be_between_1_and_31[((order) >= 1 && (order) <= 31) ? 1 : -1]; \
static const uint32_t add_range_##name = ((((uint32_t)1) << ((order) - 1)) < (uint32_t)(HS_HASH_ADD_RANGE)) ? \
	(((uint32_t)1) << ((order) - 1)) : (uint32_t)(HS_HASH_ADD_RANGE); \
static const uint32_t table_size_##name = (((uint32_t)1) << (order)); \
/* Compile time check: the table must fit into the largest object the */ \
/* compiler supports (half the range of size_t), otherwise the size */ \
/* passed to HS_HASH_MALLOC wraps around on targets with a small size_t. */ \
typedef char hs_hash_table_##name##_exceeds_max_object_size[ \
	((((size_t)-1) >> 1) / sizeof(struct hs_hashtable_##name) >= (((uint32_t)1) << (order))) ? 1 : -1]; \
\
static inline HS_HASH_UNUSED uint32_t wrap_pos##name(uint32_t pos) \
{ \
	return pos & (table_size_##name - 1); \
} \
\
static inline HS_HASH_UNUSED struct hs_hashtable_##name *hs_hashtable_create_##name(void) \
{ \
	struct hs_hashtable_##name *table = (struct hs_hashtable_##name *)HS_HASH_MALLOC((size_t)table_size_##name * sizeof(struct hs_hashtable_##name)); \
	if (table != NULL) { \
		size_t i; \
		for (i = 0; i < table_size_##name; ++i) { \
			memset(&table[i], 0, sizeof(table[0])); \
			table[i].key = (type)HS_HASHTABLE_INVALIDENTRY; \
		} \
	} \
	return table; \
} \
\
static inline HS_HASH_UNUSED void hs_hashtable_delete_##name(struct hs_hashtable_##name *table) \
{ \
	HS_HASH_FREE(table); \
	return; \
} \
\
static inline HS_HASH_UNUSED int hs_hashtable_get_##name(struct hs_hashtable_##name *table, type key, struct value_##name *value) \
{ \
	uint32_t hash_pos = hash_func_##name##_##type_name(key); \
	uint32_t pos = hash_pos; \
	uint32_t hop_info = table[hash_pos].hop_info; \
	while (hop_info != 0) { \
/* Read the stored key exactly once (volatile), so the comparison */ \
/* checks and uses the very same value even if the writer changes it. */ \
		if (((hop_info & 0x1) == 1) && (hs_is_equal_##type_name(*(type const volatile *)&table[pos].key, key))) { \
			*value = table[pos].value; \
			return HS_HASHTABLE_SUCCESS; \
		} \
		hop_info = hop_info >> 1; \
		pos = wrap_pos##name(pos + 1); \
	} \
	return HS_HASHTABLE_NOTFOUND; \
} \
\
static inline HS_HASH_UNUSED uint32_t hop_range_##name(void) \
{ \
	return sizeof(((struct hs_hashtable_##name*)0)->hop_info) * 8; \
}\
\
static inline HS_HASH_UNUSED uint32_t find_closer_entry_##name(struct hs_hashtable_##name *table, uint32_t free_position) \
{ \
	uint32_t check_distance = (hop_range_##name() - 1); \
	while (check_distance > 0) { \
		unsigned int i; \
		uint32_t check_position = wrap_pos##name(free_position - check_distance); \
		uint32_t check_hop_info = table[check_position].hop_info; \
		uint32_t mask = 1; \
		uint32_t hop_position = 0xffffffff; \
		for (i = 0; i < check_distance; ++i) { \
			if ((mask & check_hop_info) != 0) { \
				hop_position = wrap_pos##name(check_position + i); \
				break; \
			} \
			mask = mask << 1; \
		} \
		if (hop_position != 0xffffffff) { \
/* We found a table entry to swap the free entry with. */ \
			table[free_position].key = table[hop_position].key; \
			table[free_position].value = table[hop_position].value; \
			HS_HASH_WMB(); \
/* Publish the copy first, then unpublish the original, with separate */ \
/* stores that change a single bit each. In between, both (identical) */ \
/* copies are reachable, so readers never miss the entry, even on CPUs */ \
/* that store a uint32_t in several parts. */ \
			check_hop_info = check_hop_info | (((uint32_t)1) << check_distance); \
			table[check_position].hop_info = check_hop_info; \
			HS_HASH_WMB(); \
			table[check_position].hop_info = check_hop_info & ~(mask); \
/* hop_position is no longer referenced. Make sure that is visible before the slot gets reused. */ \
			HS_HASH_WMB(); \
			return hop_position ;\
		} \
		--check_distance; \
	} \
	return 0xffffffff; \
} \
\
static inline HS_HASH_UNUSED int hs_hashtable_put_##name(struct hs_hashtable_##name *table, type key, struct value_##name value, struct value_##name *prev_value) \
{ \
	uint32_t hash_pos; \
	uint32_t pos; \
	uint32_t hop_info; \
	uint32_t free_distance; \
	uint32_t free_pos; \
	uint32_t closer_pos; \
	uint32_t old_pos = 0xffffffff; \
	uint32_t old_distance; \
	uint32_t release_pos; \
	if (prev_value != NULL) { \
		memset(prev_value, 0, sizeof(*prev_value)); \
	} \
	if (key == (type)HS_HASHTABLE_INVALIDENTRY) { \
		return HS_HASHTABLE_KEYINVAL; \
	} \
\
/* Look if the key is already in the table. */ \
	hash_pos = hash_func_##name##_##type_name(key); \
	pos = hash_pos; \
	hop_info = table[hash_pos].hop_info; \
	while (hop_info != 0) { \
		if (((hop_info & 0x1) == 1) && (hs_is_equal_##type_name(table[pos].key, key))) { \
			old_pos = pos; \
/* Keep the stored key (for string tables the stored pointer), only the value gets replaced. */ \
			key = table[pos].key; \
			break; \
		} \
		hop_info = hop_info >> 1; \
		pos = wrap_pos##name(pos + 1); \
	} \
\
/* Write the entry to a free slot. This is also done if the key is */ \
/* already in the table: the new value goes to a free slot and the */ \
/* hop_info bits are switched over afterwards (and back, see below). */ \
/* Overwriting the value in place would let an interrupting */ \
/* HS_HASHTABLE_GET read a mix of the old and the new value. */ \
/* Make linear search from hash_pos to add_range_##name to find an empty table entry. */ \
	free_distance = 0; \
	pos = hash_pos; \
	while (free_distance < add_range_##name) { \
		if (table[pos].key == (type)HS_HASHTABLE_INVALIDENTRY) { \
			break; \
		} \
		++free_distance; \
		pos = wrap_pos##name(pos + 1); \
	} \
	free_pos = pos; \
	if (free_distance < add_range_##name) { \
		for (;;) { \
			if (free_distance < hop_range_##name()) { \
				table[free_pos].value = value; \
				table[free_pos].key = key; \
				HS_HASH_WMB(); \
				hop_info = table[hash_pos].hop_info; \
				hop_info = hop_info | (((uint32_t)1) << free_distance); \
				table[hash_pos].hop_info = hop_info; \
				if (old_pos != 0xffffffff) { \
/* Now the old and the new entry are both reachable, each of them complete. */ \
/* Unpublish the old one with a separate store, so each hop_info store */ \
/* changes a single bit. Then readers never miss the key, even on CPUs */ \
/* that store a uint32_t in several parts. */ \
					old_distance = wrap_pos##name(old_pos - hash_pos); \
					HS_HASH_WMB(); \
					hop_info = hop_info & ~(((uint32_t)1) << old_distance); \
					table[hash_pos].hop_info = hop_info; \
					HS_HASH_WMB(); \
					if (prev_value != NULL) { \
						*prev_value = table[old_pos].value; \
					} \
					release_pos = old_pos; \
					if (old_distance < free_distance) { \
/* The old slot is closer to hash_pos. Move the entry back, otherwise */ \
/* updates push entries away from their bucket and HS_HASHTABLE_GET */ \
/* gets slower. Nothing references the old slot now, so it can be */ \
/* overwritten safely before it gets published again. */ \
						table[old_pos].value = value; \
						HS_HASH_WMB(); \
						hop_info = hop_info | (((uint32_t)1) << old_distance); \
						table[hash_pos].hop_info = hop_info; \
						HS_HASH_WMB(); \
						table[hash_pos].hop_info = hop_info & ~(((uint32_t)1) << free_distance); \
						HS_HASH_WMB(); \
						release_pos = free_pos; \
					} \
					table[release_pos].key = (type)HS_HASHTABLE_INVALIDENTRY; \
					memset(&table[release_pos].value, 0, sizeof(table[release_pos].value)); \
				} \
				return HS_HASHTABLE_SUCCESS; \
			} \
/* Now we must try to swap some entries */ \
			closer_pos = find_closer_entry_##name(table, free_pos); \
			if (closer_pos == 0xffffffff) { \
/* Nothing can be moved into free_pos. If free_pos was vacated by an */ \
/* earlier move, it still holds a stale copy of the moved entry. Mark it */ \
/* empty again, otherwise the slot is lost forever. No hop_info bit */ \
/* refers to free_pos, so concurrent readers can't observe this. */ \
				table[free_pos].key = (type)HS_HASHTABLE_INVALIDENTRY; \
				memset(&table[free_pos].value, 0, sizeof(table[free_pos].value)); \
				break; \
			} \
/* If the entry to be updated was moved into free_pos, track its new position. */ \
			if (closer_pos == old_pos) { \
				old_pos = free_pos; \
			} \
			free_pos = closer_pos; \
			free_distance = wrap_pos##name(free_pos - hash_pos); \
		} \
	} \
	return HS_HASHTABLE_FULL; \
} \
\
static inline HS_HASH_UNUSED int hs_hashtable_remove_##name(struct hs_hashtable_##name *table, type key, struct value_##name *value) \
{ \
	uint32_t hash_pos = hash_func_##name##_##type_name(key); \
	uint32_t pos = hash_pos; \
	uint32_t hop_info = table[hash_pos].hop_info; \
	uint32_t check_hop_info = hop_info; \
	while (check_hop_info != 0) { \
		if (((check_hop_info & 0x1) == 1) && (hs_is_equal_##type_name(table[pos].key, key))) { \
			uint32_t distance; \
			if (value != NULL) { \
				*value = table[pos].value; \
			} \
			distance = wrap_pos##name(pos - hash_pos); \
/* Unpublish the entry before invalidating it. Otherwise a reader might */ \
/* follow a still set hop_info bit to an invalidated key (which crashes */ \
/* strcmp() for string tables). */ \
			table[hash_pos].hop_info = hop_info & ~(((uint32_t)1) << distance); \
			HS_HASH_WMB(); \
			table[pos].key = (type)HS_HASHTABLE_INVALIDENTRY; \
			memset(&table[pos].value, 0, sizeof(table[pos].value)); \
			return HS_HASHTABLE_SUCCESS; \
		} \
		check_hop_info = check_hop_info >> 1; \
		pos = wrap_pos##name(pos + 1); \
	} \
	return HS_HASHTABLE_NOTFOUND; \
}



#define DECLARE_HS_HASHTABLE_STRING(name, order, value_entries) \
struct value_##name { \
	void *vals[value_entries]; \
};\
struct hs_hashtable_##name { \
	uint32_t hop_info; \
	const char *key; \
	struct value_##name value; \
}; \
static inline HS_HASH_UNUSED uint32_t hash_func_##name##_string(const char* key) \
{ \
	uint32_t hash = 0; \
	uint32_t c = *key; \
	++key; \
	while (c != 0) { \
		hash = ((c + (hash << 6U)) + (hash << 16U)) - hash; \
		c = *key; \
		++key; \
	} \
	return hs_hash32(hash, order); \
} \
DECLARE_HS_HASHTABLE(name, order, string, const char *, value_entries)



#define DECLARE_HS_HASHTABLE_UINT32(name, order, value_entries) \
struct value_##name { \
	void *vals[value_entries]; \
};\
struct hs_hashtable_##name { \
	uint32_t hop_info; \
	uint32_t key; \
	struct value_##name value; \
}; \
static inline HS_HASH_UNUSED uint32_t hash_func_##name##_uint32_t(uint32_t key) \
{ \
	return hs_hash32(key, order); \
} \
\
DECLARE_HS_HASHTABLE(name, order, uint32_t, uint32_t, value_entries)



#define DECLARE_HS_HASHTABLE_UINT64(name, order, value_entries) \
struct value_##name { \
	void *vals[value_entries]; \
};\
struct hs_hashtable_##name { \
	uint32_t hop_info; \
	uint64_t key; \
	struct value_##name value; \
}; \
static inline HS_HASH_UNUSED uint32_t hash_func_##name##_uint64_t(uint64_t key) \
{ \
	return hs_hash6432shift(key, order); \
} \
\
DECLARE_HS_HASHTABLE(name, order, uint64_t, uint64_t, value_entries)



/*
 * The type of the hash table declared as "name". Use it to declare
 * pointers to the table, e.g.
 * HS_HASHTABLE_TYPE(name) *table = HS_HASHTABLE_CREATE(name);
 */
#define HS_HASHTABLE_TYPE(name) \
	struct hs_hashtable_##name

/*
 * Creates a hash table.
 */
#define HS_HASHTABLE_CREATE(name) \
	hs_hashtable_create_##name()

/*
 * Destroys a hash table.
 */
#define HS_HASHTABLE_DELETE(name, table) \
	do { \
		hs_hashtable_delete_##name((table)); \
		(table) = NULL; \
	} while (0)

/*
 * Maps the key to the value in table. Return values:
 * HS_HASHTABLE_FULL: no space left in the table. This can also happen if
 *                    key is already in the table, because the new value
 *                    is written to a free slot before the old one is
 *                    released. The old value stays in the table then.
 * HS_HASHTABLE_KEYINVAL: invalid key (HS_HASHTABLE_INVALIDENTRY)
 * HS_HASHTABLE_SUCCESS: everything o.k.
 *
 * If prev_value != NULL, the previous value stored for this key is
 * assigned to prev_value, if the key was in the table and the call
 * succeeded. Otherwise prev_value is zeroed.
 */
#define HS_HASHTABLE_PUT(name, table, key, value, prev_value) \
	hs_hashtable_put_##name((table), (key), (value), (prev_value))

/*
 * Looks up key in table. Return values:
 * HS_HASHTABLE_SUCCESS: key was found, its value is assigned to value.
 * HS_HASHTABLE_NOTFOUND: key is not in the table, value is unchanged.
 */
#define HS_HASHTABLE_GET(name, table, key, value) \
	hs_hashtable_get_##name((table), (key), (value))

/*
 * Removes key and its value from table. Return values:
 * HS_HASHTABLE_SUCCESS: key was removed. If value != NULL, the removed
 *                       value is assigned to value.
 * HS_HASHTABLE_NOTFOUND: key is not in the table, value is unchanged.
 */
#define HS_HASHTABLE_REMOVE(name, table, key, value) \
	hs_hashtable_remove_##name((table), (key), (value))

#endif
