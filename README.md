Implementation of lld's Driver.h interface via invoking the binaries
====================================================================

Some applications call directly into [lld](https://lld.llvm.org/)'s component libraries, but not every distribution wants to ship them.
They can ship this stub instead that provides the same interface by invoking the `lld` binaries.

Building
========

Before building, one needs to download [lld/include/lld/Common/Driver.h](https://github.com/llvm/llvm-project/tree/main/lld/include/lld/Common/Driver.h) of the correct version.
That's the interface being implemented here.

As local dependencies, a C++ compiler with standard library and an LLVM development package should be enough.
The build uses `make` and is controlled by environment variables `CXX`, `CXXFLAGS` and `LDFLAGS`.
There are two targets: `liblld-stub.so` and `liblld-stub.a`.
