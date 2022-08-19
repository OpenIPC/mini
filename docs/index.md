![OpenIPC Logo](https://cdn.themactep.com/images/logo_openipc.png)

## Mini

OpenSource Mini IP camera streamer

### Maintainers Wanted

The project is currently looking for a maintainer (the original author doesn't have time anymore due to his business).

Please contact [IgorZalatov](mailto:flyrouter@gmail.com) to inquire further.

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

-----

### Support

OpenIPC offers two levels of support.

- Free support through the community (via [chat](https://openipc.org/#telegram-chat-groups) and [mailing lists](https://github.com/OpenIPC/firmware/discussions)).
- Paid commercial support (from the team of developers).

Please consider subscribing for paid commercial support if you intend to use our product for business.
As a paid customer, you will get technical support and maintenance services directly from our skilled team.
Your bug reports and feature requests will get prioritized attention and expedited solutions. It's a win-win
strategy for both parties, that would contribute to the stability your business, and help core developers
to work on the project full-time.

If you have any specific questions concerning our project, feel free to [contact us](mailto:flyrouter@gmail.com).

### Participating and Contribution

If you like what we do, and willing to intensify the development, please consider participating.

You can improve existing code and send us patches. You can add new features missing from our code.

You can help us to write a better documentation, proofread and correct our websites.

You can just donate some money to cover the cost of development and long-term maintaining of what we believe
is going to be the most stable, flexible, and open IP Network Camera Framework for users like yourself.

You can make a financial contribution to the project at [Open Collective](https://opencollective.com/openipc/contribute/backer-14335/checkout).

Thank you.

<p align="center">
<a href="https://opencollective.com/openipc/contribute/backer-14335/checkout" target="_blank"><img src="https://opencollective.com/webpack/donate/button@2x.png?color=blue" width="375" alt="Open Collective donate button"></a>
</p>
