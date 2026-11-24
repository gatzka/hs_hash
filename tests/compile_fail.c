/*
 * Declarations that must not compile. CMake builds this file with
 * different values for COMPILE_FAIL_ORDER and COMPILE_FAIL_VALUES and
 * checks the error message.
 */

#include <hs_hash/hs_hash.h>

DECLARE_HS_HASHTABLE_UINT32(X, COMPILE_FAIL_ORDER, COMPILE_FAIL_VALUES)
