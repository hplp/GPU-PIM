# GPU-PIM simulator

This is a fork from the original gem5 to create a GPU hosted PIM simulator combining
AMD gpu models with Aquabolt PIM simulator. Refer to the main website for gem5 requirements.
This model is built on Ubuntu 20.

This is still an ongoing work. This repo will give you basic results, but we are working on making
application development easier with a general API. To get the results build the gem5 object and run
makefile.

The main website can be found at <http://www.gem5.org>.

## Building

Once you have all dependencies resolved, run `scons build/VEGA_X86/gem5.opt configs/example/gpufs/vega10,py` to build the executable.

Run `make` to get Aquabolt test results for microbenchmarks.

## ML applications

We have an ongoing effort to build ML applications on this platform. You can find detailed documentation under /deteq folder.
We are still working on automating running ML applications on this platform, but you can find the code to trace ONNX graphs and
evaluate different quantization options.
