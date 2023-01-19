cmake_minimum_required(VERSION 2.8.12)
project(mini C)

set(CMAKE_C_STANDARD 99)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(HiSiliconSDK REQUIRED)
find_package(Threads REQUIRED)

include_directories(
    src
    ${VENDOR_SDK_INCLUDE_DIRS}
)

add_executable(${PROJECT_NAME}
    src/mp4/bitbuf.c
    src/mp4/bitbuf.h
    src/mp4/moof.c
    src/mp4/moof.h
    src/mp4/moov.c
    src/mp4/moov.h
    src/mp4/mp4.c
    src/mp4/mp4.h
    src/mp4/nal.c
    src/mp4/nal.h

    src/config/app_config.c
    src/config/app_config.h
    src/config/config.c
    src/config/config.h
    src/config/sensor_config.c
    src/config/sensor_config.h

    src/rtsp/ringfifo.c
    src/rtsp/ringfifo.h
    src/rtsp/rtputils.c
    src/rtsp/rtputils.h
    src/rtsp/rtspservice.c
    src/rtsp/rtspservice.h
    src/rtsp/rtsputils.c
    src/rtsp/rtsputils.h

    src/compat.h
    src/gpio.c
    src/gpio.h
    src/hierrors.c
    src/hierrors.h
    src/http_post.c
    src/http_post.h
    src/jpeg.c
    src/jpeg.h
    src/main.c
    src/mmap.h
    src/motion_detect.c
    src/motion_detect.h
    src/night.c
    src/night.h
    src/sensor.c
    src/sensor.h
    src/server.c
    src/server.h
    src/stack.c
    src/tools.c
    src/tools.h
    src/videohw.c
    src/videohw.h
)

target_link_libraries(${PROJECT_NAME}
    ${HILIBS}
    ${VENDOR_SDK_LIBRARIES}
    Threads::Threads
    dl gcc_s)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION /usr/bin)
