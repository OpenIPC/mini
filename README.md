Mini Video Streamer
===================
__A part of [OpenIPC Project](https://openipc.org/)__

```diff
@@ This project needs developers! Please contact Igor Zalatov <flyrouter@gmail.com>. @@
```

## Description
Mini is an open source video streaming software for HiSilicon IP cameras. It is a
malnourished and underloved little brother of the commercial Majestic video streamer.

### Supported hardware and features

| SoC Family  | Audio Stream | JPEG Snapshot | RTSP Stream | Motion Detect | On-Screen Display |
|-------------|:------------:|:-------------:|:-----------:|:-------------:|:-----------------:|
| Hi3516CV100 | ✗            | ✗             | ✗           | ⁿ/ₐ           | ✗                 |
| Hi3516CV200 | ✗            | ✔️            | ✔️          | ✔️            | ✗                 |
| Hi3516CV300 | ✗            | ✔️            | ✔️          | ✔️            | ✗                 |
| Hi3516CV500 | ✗            | ✗             | ✗           | ✗             | ✗                 |

_✔️ - supported, ✗ - not supported, ⁿ/ₐ - not supported by hardware_

### Recommended hardware
We recommend buying a [HiSilicon 3516CV300 + Sony IMX291](https://aliexpress.com/item/1005002315913099.html) 
board as a development kit. This IP camera module comes with 128MB of RAM and 16MB SPI Flash ROM.

Use [Coupler](https://github.com/OpenIPC/coupler) to replace the stock firmware with OpenIPC.
You won't even need to solder anything like a UART adapter.

### Building
To clone the code locally, run
```console
git clone --recurse-submodules https://github.com/openipc/mini
```
or, if you have already checked out the repository without submodules, run
```console
git submodule init
git submodule update
```

Build the code with CMake:
```console
$ cmake -H. -Bbuild \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLATFORM_SDK_DIR=<PATH_TO_SDK> \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_TOOLCHAIN_FILE=tools/cmake/toolchains/arm-openipc-linux-musleabi.cmake
$ cmake --build build
```
Where _<PATH_TO_SDK>_ is either `glutinium/hisi-osdrv2` or `glutinium/hisi-osdrv3`.

### Configuration
The Mini streamer does not support sensor autodetection yet. You will need to use
`ipcinfo --long_sensor` to determine the sensor model and its control bus, and then set 
the path to a corresponding config file as `sensor_config` parameter in `mini.ini`.

### Authors
- [@widgetii](https://github.com/widgetii)
