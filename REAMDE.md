# 96kHz mic array

This directory contains a basic example of getting 96kHz data out of the mic array.

> [!WARNING]
> The stage 2 filter needs updating so that it does not modify the output of stage 1.

> [!WARNING]
> The stage 1 filter will attentuate some of the output. A new stage 1 filter will need designing. See stage
> 1 filter characteristics here: https://www.xmos.com/documentation/XM-014785-PC-9/html/modules/io/modules/mic_array/doc/programming_guide/src/decimator_stages.html

This example works on the xk-voice-sq66, the example will therefore need updating to work on your target. There are `TODO`
comments at the appropriate places in main.cpp

## build

```
cmake -B build --toolchain=xmos_cmake_toolchain/xs3a.cmake
cmake --build build
xrun --xscope build/mic_that_does_it_all.xe
```
