/*
 * Uses every operation of every key type, to check that the header
 * compiles in the Linux kernel.
 */

#include <linux/module.h>

#include <hs_hash/hs_hash.h>

DECLARE_HS_HASHTABLE_UINT32(k32, 8, 1)
DECLARE_HS_HASHTABLE_UINT64(k64, 8, 1)
DECLARE_HS_HASHTABLE_STRING(kstr, 8, 1)

static int __init hs_hash_test_init(void)
{
	HS_HASHTABLE_TYPE(k32) *a = HS_HASHTABLE_CREATE(k32);
	HS_HASHTABLE_TYPE(k64) *b = HS_HASHTABLE_CREATE(k64);
	HS_HASHTABLE_TYPE(kstr) *c = HS_HASHTABLE_CREATE(kstr);
	struct value_k32 va = { { NULL } };
	struct value_k64 vb = { { NULL } };
	struct value_kstr vc = { { NULL } };

	if (a != NULL) {
		HS_HASHTABLE_PUT(k32, a, 1, va, NULL);
		HS_HASHTABLE_GET(k32, a, 1, &va);
		HS_HASHTABLE_REMOVE(k32, a, 1, NULL);
	}
	if (b != NULL) {
		HS_HASHTABLE_PUT(k64, b, 1, vb, NULL);
		HS_HASHTABLE_GET(k64, b, 1, &vb);
		HS_HASHTABLE_REMOVE(k64, b, 1, NULL);
	}
	if (c != NULL) {
		HS_HASHTABLE_PUT(kstr, c, "key", vc, NULL);
		HS_HASHTABLE_GET(kstr, c, "key", &vc);
		HS_HASHTABLE_REMOVE(kstr, c, "key", NULL);
	}
	HS_HASHTABLE_DELETE(k32, a);
	HS_HASHTABLE_DELETE(k64, b);
	HS_HASHTABLE_DELETE(kstr, c);
	return 0;
}

static void __exit hs_hash_test_exit(void)
{
}

module_init(hs_hash_test_init);
module_exit(hs_hash_test_exit);
MODULE_DESCRIPTION("Compile test for hs_hash");
MODULE_LICENSE("Dual MIT/GPL");
