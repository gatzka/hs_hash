/*
 * Uses every operation of every key type, so that all code of the header
 * gets compiled. CMake builds this file with strict warnings, and the CI
 * also compiles it for microcontrollers, with other compilers and as C++.
 */

#include <hs_hash/hs_hash.h>

DECLARE_HS_HASHTABLE_UINT32(w32, 10, 1)
DECLARE_HS_HASHTABLE_UINT64(w64, 10, 3)
DECLARE_HS_HASHTABLE_STRING(wstr, 10, 2)

int warnings_uint32(uint32_t key, void *ptr);
int warnings_uint64(uint64_t key, void *ptr);
int warnings_string(const char *key, void *ptr);

int warnings_uint32(uint32_t key, void *ptr)
{
	HS_HASHTABLE_TYPE(w32) *table = HS_HASHTABLE_CREATE(w32);
	const HS_HASHTABLE_TYPE(w32) *readonly = table;
	struct value_w32 value;
	struct value_w32 prev;
	int ret;

	if (table == NULL) {
		return -1;
	}
	value.vals[0] = ptr;
	ret = HS_HASHTABLE_PUT(w32, table, key, value, &prev);
	if (ret == HS_HASHTABLE_SUCCESS) {
		ret = HS_HASHTABLE_GET(w32, readonly, key, &value);
	}
	if (ret == HS_HASHTABLE_SUCCESS) {
		ret = HS_HASHTABLE_REMOVE(w32, table, key, &value);
	}
	HS_HASHTABLE_DELETE(w32, table);
	return ret;
}

int warnings_uint64(uint64_t key, void *ptr)
{
	HS_HASHTABLE_TYPE(w64) *table = HS_HASHTABLE_CREATE(w64);
	struct value_w64 value;
	int ret;

	if (table == NULL) {
		return -1;
	}
	value.vals[0] = ptr;
	value.vals[1] = ptr;
	value.vals[2] = ptr;
	ret = HS_HASHTABLE_PUT(w64, table, key, value, NULL);
	if (ret == HS_HASHTABLE_SUCCESS) {
		ret = HS_HASHTABLE_GET(w64, table, key, &value);
	}
	if (ret == HS_HASHTABLE_SUCCESS) {
		ret = HS_HASHTABLE_REMOVE(w64, table, key, NULL);
	}
	HS_HASHTABLE_DELETE(w64, table);
	return ret;
}

int warnings_string(const char *key, void *ptr)
{
	HS_HASHTABLE_TYPE(wstr) *table = HS_HASHTABLE_CREATE(wstr);
	struct value_wstr value;
	int ret;

	if (table == NULL) {
		return -1;
	}
	value.vals[0] = ptr;
	value.vals[1] = ptr;
	ret = HS_HASHTABLE_PUT(wstr, table, key, value, NULL);
	if (ret == HS_HASHTABLE_SUCCESS) {
		ret = HS_HASHTABLE_GET(wstr, table, key, &value);
	}
	if (ret == HS_HASHTABLE_SUCCESS) {
		ret = HS_HASHTABLE_REMOVE(wstr, table, key, NULL);
	}
	HS_HASHTABLE_DELETE(wstr, table);
	return ret;
}
