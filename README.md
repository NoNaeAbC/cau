# cau

## custom allocator utility

This project provides a allocator that wraps around another slower one.
The concrete goal of this allocator is, to provide a allocator that can segment up a file.
The standard C++ allocator isn't able to do this.
Therefore, I currently don't care about performance, but rather about functionality.
Instead, the primary goal is, that the ABI is stable and platform independent.
But only 64-bit pointers are supported.

Usage example is scatterd between test/test.cpp and include/generic_unsync_allocator.h.
