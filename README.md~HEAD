## Mini

OpenSource Mini IP camera streamer

### Maintainers Wanted

The project is currently looking for a maintainer (the original author doesn't
have time anymore due to his business). Please contact [Igor
Zalatov](mailto:flyrouter@gmail.com) to inquire further.

### Platform support

| Family      | Audio | JPEG | RTSP | MD | OSD |
| ----------- | ----- | ---- | ---- | -- | --- |
| hi3516cv100 |   ❌  |  ❌  |  ❌  | ⁿ/ₐ | ❌ |
| hi3518cv200 |   ❌  |  ✅  |  ✅  | ✅ |  ❌ |
| hi3516cv300 |   ❌  |  ✅  |  ✅  | ✅ |  ❌ |
| hi3516ev300 |   ❌  |  ❌  |  ❌  | ❌ |  ❌ |

### Build with CMake

```console
$ cmake -H. -Bbuild \
    -DCMAKE_BUILD_TYPE=Release \
    -DPLATFORM_SDK_DIR=<PATH_TO_SDK> \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_TOOLCHAIN_FILE=tools/cmake/toolchains/arm-openipc-linux-musleabi.cmake
$ cmake --build build
```

Where <PATH_TO_SDK> either `glutinium/hisi-osdrv2` or `glutinium/hisi-osdrv3`

### Hardware for development

We consider buying [HiSilicon 3516CV300 + Sony IMX291
board](https://aliexpress.com/item/1005002315913099.html) to use it as
development kit. It has 128Mb of RAM and 16Mb of SPI Flash ROM. Use
[Coupler](https://github.com/OpenIPC/coupler) to get rid of stock firmware and
install OpenIPC (you don't even need to solder anything like UART adapter).

### First run

`Mini` has no sensor autodetection currently, but one can use built-in `ipcinfo
--long_sensor` to determine sensor type and its control bus and then manually
specify appropriate sensor config file path in `sensor_config` in `mini.ini`
