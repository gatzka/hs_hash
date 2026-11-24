# hs_hash
A C based implementation of a Hopscotch hash table.

This is an C-based implementation of the [hopscotch hashtable
alrgorithm](https://people.csail.mit.edu/shanir/publications/disc2008_submission_98.pdf).

The implementation is focused to be used in small microcontroller based
systems or the Linux kernel. Resizing of the hashmap is not implemented,
so you can't change the size after declaring the hashtable.

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
- zero terminated strings (char *)

To avoid code duplication and because C has no template engine, the
whole implementation is a basically a big macro. So don't expect good
debug experience if you step into this code and be prepared for strange
and nasty compiler warnings/errors if you don't use the hashtable
correctly.
