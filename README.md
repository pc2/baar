BAAR
====

BAAR (Binary Acceleration at Runtime) is an LLVM-based framework for transparent acceleration of binary applications using massively parallel accelerators (currently Xeon Phi). To this end, BAAR analyzes an application in LLVM binary format, identifies the computationally expensive functions (hotspots), and generates a parallelized and vectorized implementations of the hotspots on-the-fly targeting the Intel Xeon Phi accelerator. Once the code generation has finished, the application is transparently modified to offload the hotspot to the accelerator.

The architecture of BAAR has been published in the following scientific papers:

* M. Damschen and C. Plessl. __Easy-to-use on-the-fly binary program acceleration on many-cores.__ In *Proc. Int. Workshop on Adaptive Self-tuning Computing Systems*, Jan. 2015.
* M. Damschen, H. Riebler, G. Vaz, and C. Plessl. __Transparent offloading of computational hotspots from binary code to Xeon Phi.__ In Proc. *Design, Automation and Test in Europe Conf. (DATE)*, pages 1078–1083. EDA Consortium, Mar. 2015.

We gratefully acknowledge the support of the research leading to BAAR by the European Commission as part of the [FP7 project SAVE](http://www.fp7-save.eu) and the German Research Foundation as part of the Collaborative Research Center [CRC 901 On-the-Fly Computing](http://sfb901.uni-paderborn.de).

(c) 2014–15 University of Paderborn, Paderborn Center for Parallel Computing

### Building with CMake

First, LLVM has to be built from source with CMake (See: [Building LLVM with CMake](http://llvm.org/docs/CMake.html)). Afterwards, checkout this repository into another directory and run the following statements:

    $ cmake -DLLVM_SRC_DIR=/path/to/llvm-3.4/source -DLLVM_BIN_DIR=/path/to/llvm-3.4/build .
    $ make

This should do the trick or help you out with telling you which libraries are missing, if any. All binaries are generated in the `out` directory.
