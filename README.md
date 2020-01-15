# hs_hash
A C based implementation of a Hopscotch hash table.

This is an C-based implementation of the [hopscotch hashtable
alrgorithm](https://people.csail.mit.edu/shanir/publications/disc2008_submission_98.pdf).

The implementation is focused to be used in small microcontroller based
systems or the Linux kernel. Resizing of the hashmap is not implemented.
One of the major advantages is that you can make HS\_HASHTABLE\_GET
operations ans concurrently HS\_HASHTABLE\_REMOVE and HS\_HASHTABLE\_PUT
operations, so the GET operation might be interrupted by PUT and/or
REMOVE.

Three different key types are supported:
- uint32_t
- uint64_t
- zero terminated strings (char *)

To avoid code duplication and because C has no template engine, the
whole implementation is a basically a big macro. So don't expect good
debug experience if you step into this code and be prepared for strange
and nasty compiler warnings/errors if you don't use the hashtable
correctly.
