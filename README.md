BAAR
====

Binary Acceleration at Runtime

### Building with CMake

First, LLVM has to be built from source with CMake (See: [Building LLVM with CMake](http://llvm.org/docs/CMake.html)). Afterwards, checkout this repository into another directory and run the following statements:

    $ cmake -DLLVM_SRC_DIR=/path/to/llvm-3.4/source -DLLVM_BIN_DIR=/path/to/llvm-3.4/build .
    $ make

This should do the trick or help you out with telling you which libraries are missing, if any. All binaries are generated in the `out` directory.
