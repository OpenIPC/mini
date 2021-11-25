cmake_minimum_required(VERSION 2.8.12)
project(mini C)

set(CMAKE_C_STANDARD 99)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(HiSiliconSDK REQUIRED)
find_package(Threads REQUIRED)

include_directories(
    ./src/
    ${VENDOR_SDK_INCLUDE_DIRS}
)

add_executable(mini
    src/mp4/bitbuf.h
    src/mp4/bitbuf.c
    src/mp4/moov.h
    src/mp4/moov.c
    src/mp4/moof.h
    src/mp4/moof.c
    src/mp4/nal.h
    src/mp4/nal.c
    src/mp4/mp4.h
    src/mp4/mp4.c

    src/config/app_config.h
    src/config/app_config.c
    src/config/config.h
    src/config/config.c
    src/config/sensor_config.h
    src/config/sensor_config.c

    src/rtsp/ringfifo.h
    src/rtsp/ringfifo.c
    src/rtsp/rtputils.h
    src/rtsp/rtputils.c
    src/rtsp/rtspservice.h
    src/rtsp/rtspservice.c
    src/rtsp/rtsputils.h
    src/rtsp/rtsputils.c

    src/hidemo.h
    src/hidemo.c
    src/hierrors.h
    src/hierrors.c
    src/tools.h
    src/tools.c
    src/stack.c
    src/server.h
    src/server.c
    src/sensor.h
    src/sensor.c
    src/chipid.h
    src/chipid.c
    src/motion_detect.h
    src/motion_detect.c
    src/night.h
    src/night.c
    src/gpio.h
    src/gpio.c
    src/http_post.h
    src/http_post.c
    src/hi_jpeg.h
    src/hi_jpeg.c
    src/main.c
)

target_link_libraries(mini
    ${HILIBS}
    ${VENDOR_SDK_LIBRARIES}
    Threads::Threads
    dl)

install(TARGETS mini RUNTIME DESTINATION /usr/bin)
