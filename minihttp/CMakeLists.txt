cmake_minimum_required(VERSION 2.8)
project(minihttp C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-std=c99")

include_directories(
    ./include   # HISDK headers
)

set(HILIBS
    isp _hiae _hiawb _hiaf _hidefog
    pthread m dl
    ive md mpi tde upvqe dnvqe VoiceEngine
)

add_executable(minihttp
    src/hidemo.h
    src/hidemo.c
    src/hierrors.h
    src/hierrors.c
    src/tools.h
    src/tools.c
    src/stack.c
    src/sensors.h
    src/sensors.c
    src/server.h
    src/server.c
    src/main.c
)
target_link_libraries(minihttp pthread ${HILIBS})
install(TARGETS minihttp RUNTIME DESTINATION /usr/bin/)

if(APPLE)
    add_executable(test_config
        src/test_config.c
        src/tools.c
        src/sensors.c
    )
    target_link_libraries(test_config pthread)
    include(tests/CMakeLists.txt)
endif()
