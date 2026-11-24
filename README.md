# hs_hash
A C-based implementation of a Hopscotch hash table.

This is a header-only C99 implementation of the [hopscotch hash table
algorithm](https://people.csail.mit.edu/shanir/publications/disc2008_submission_98.pdf).

The implementation is meant to be used in small microcontroller-based
systems or the Linux kernel. Resizing of the hash table is not
implemented, so you can't change the size after declaring the hash
table.

One of the major advantages is that HS\_HASHTABLE\_GET can be called
while an HS\_HASHTABLE\_PUT or HS\_HASHTABLE\_REMOVE operation is in
progress, for example from an interrupt handler that interrupts the
writer. The opposite is not supported: a GET must not be interrupted by
PUT or REMOVE, and readers must run on the same CPU as the writer.
Concurrent PUT and REMOVE calls must be serialized by the caller. See
the comment at the top of `hs_hash/hs_hash.h` for details.

Three different key types are supported:
- uint32_t
- uint64_t
- zero-terminated strings (char *). The table stores the pointer, not a
  copy of the string, so the string must stay valid and unchanged while
  the key is in the table.

To avoid code duplication and because C has no template engine, the
whole implementation is basically a big macro. So don't expect a good
debugging experience if you step into this code, and be prepared for
strange and nasty compiler warnings/errors if you don't use the hash
table correctly.

## Usage

```c
#include <hs_hash/hs_hash.h>

/* A table named can_ids with 2^13 slots, uint32_t keys and one pointer per key. */
DECLARE_HS_HASHTABLE_UINT32(can_ids, 13, 1)

int example(void *device)
{
    HS_HASHTABLE_TYPE(can_ids) *table = HS_HASHTABLE_CREATE(can_ids);
    struct value_can_ids value;
    int ret;

    if (table == NULL) {
        return -1;
    }

    value.vals[0] = device;
    ret = HS_HASHTABLE_PUT(can_ids, table, 0x123, value, NULL);
    if (ret == HS_HASHTABLE_SUCCESS) {
        ret = HS_HASHTABLE_GET(can_ids, table, 0x123, &value);
        /* On success, value.vals[0] is device again. */
        HS_HASHTABLE_REMOVE(can_ids, table, 0x123, NULL);
    }

    HS_HASHTABLE_DELETE(can_ids, table);
    return ret;
}
```

The table has 2^order slots. PUT can fail with HS\_HASHTABLE\_FULL
before all slots are used, so choose the order such that the table is
at most about 75% full.

Return values:

| Value | Meaning |
|---|---|
| `HS_HASHTABLE_SUCCESS` | The operation succeeded. |
| `HS_HASHTABLE_NOTFOUND` | GET, REMOVE: the key is not in the table. |
| `HS_HASHTABLE_FULL` | PUT: no free slot near the key's bucket. |
| `HS_HASHTABLE_KEYINVAL` | PUT: the key is the reserved empty-slot marker (`UINT32_MAX`, `UINT64_MAX` or `(const char *)-1`). |

The comment at the top of `hs_hash/hs_hash.h` and the comments at the
macros at its end describe all parameters and return values.

## Integration

The library is a single header. Add the repository root to the include
path and include `<hs_hash/hs_hash.h>`.

With CMake (3.14 or newer), link against the `hs_hash::hs_hash` target.
It sets the include path and requires C99:

```cmake
# hs_hash as a subdirectory of your project
add_subdirectory(hs_hash)

# or fetched at configure time
include(FetchContent)
FetchContent_Declare(hs_hash
    GIT_REPOSITORY https://github.com/gatzka/hs_hash.git
    GIT_TAG master
)
FetchContent_MakeAvailable(hs_hash)

target_link_libraries(my_app PRIVATE hs_hash::hs_hash)
```

hs\_hash can also be installed and found with `find_package()`:

```sh
cmake -S hs_hash -B build
cmake --install build --prefix /usr/local
```

```cmake
find_package(hs_hash REQUIRED)
target_link_libraries(my_app PRIVATE hs_hash::hs_hash)
```

Install rules are only generated if hs\_hash is the top-level project.
A parent project that uses `add_subdirectory()` or `FetchContent` can
enable them by setting the CMake option `HS_HASH_INSTALL` to `ON`.

## Configuration

The following macros can be defined before including the header, for
example on the compiler command line, so that all source files see the
same settings:

| Macro | Default | Purpose |
|---|---|---|
| `HS_HASH_MALLOC(size)` | `malloc(size)` | Allocates the hash table. |
| `HS_HASH_FREE(ptr)` | `free(ptr)` | Frees the hash table. |
| `HS_HASH_WMB()` | release fence | Write barrier that orders the stores of PUT and REMOVE. Must be defined for compilers other than GCC, clang and MSVC. |
| `HS_HASH_ADD_RANGE` | `1024` | How far PUT searches for a free slot. Bounds the run time of a single PUT. |

In the Linux kernel, the table is always allocated with `kvmalloc()`
and the write barrier is `wmb()`; only `HS_HASH_ADD_RANGE` can be
changed there.
