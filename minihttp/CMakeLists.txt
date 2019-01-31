cmake_minimum_required(VERSION 2.8)
project(minihttp C)

# set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "-std=gnu99")

include_directories(
    ./include   # HISDK headers
)

set(HILIBS
    isp _hiae _hiawb _hiaf _hidefog
    pthread m dl
    ive md mpi tde upvqe dnvqe VoiceEngine
)

add_executable(minihttp
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

    src/app_config.h
    src/app_config.c
    src/config.h
    src/config.c
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

    src/rtsp/ringfifo.h
    src/rtsp/ringfifo.c
    src/rtsp/rtputils.h
    src/rtsp/rtputils.c
    src/rtsp/rtspservice.h
    src/rtsp/rtspservice.c
    src/rtsp/rtsputils.h
    src/rtsp/rtsputils.c
)
target_link_libraries( minihttp pthread ${HILIBS} )
install(TARGETS minihttp RUNTIME DESTINATION /usr/bin/)

add_executable( dump src/dump.c src/stack.c src/hierrors.c )
target_link_libraries(dump pthread ${HILIBS})
install(TARGETS dump RUNTIME DESTINATION /usr/bin/)
#
#add_executable( isp_debug tools/isp_debug.c src/stack.c )
#target_link_libraries(isp_debug pthread ${HILIBS})
#install(TARGETS isp_debug RUNTIME DESTINATION /usr/bin/)
#
#add_executable( vi_bayerdump tools/vi_bayerdump.c src/stack.c )
#target_link_libraries(vi_bayerdump pthread ${HILIBS})
#install(TARGETS vi_bayerdump RUNTIME DESTINATION /usr/bin/)
#
#add_executable( vi_dump tools/vi_dump.c src/stack.c )
#target_link_libraries(vi_dump pthread ${HILIBS})
#install(TARGETS vi_dump RUNTIME DESTINATION /usr/bin/)
#
#add_executable( vpss_attr tools/vpss_attr.c src/stack.c )
#target_link_libraries(vpss_attr pthread ${HILIBS})
#install(TARGETS vpss_attr RUNTIME DESTINATION /usr/bin/)
#
#add_executable( vpss_chn_dump tools/vpss_chn_dump.c src/stack.c )
#target_link_libraries(vpss_chn_dump pthread ${HILIBS})
#install(TARGETS vpss_chn_dump RUNTIME DESTINATION /usr/bin/)
#
#add_executable( vpss_src_dump tools/vpss_src_dump.c src/stack.c )
#target_link_libraries(vpss_src_dump pthread ${HILIBS})
#install(TARGETS vpss_src_dump RUNTIME DESTINATION /usr/bin/)

if(APPLE)
    add_executable(test_config
            tools/test_config.c
        src/tools.c
        src/sensors.c
    )
    target_link_libraries(test_config pthread)
    include(tests/CMakeLists.txt)

    add_executable( dump_read src/dump.c src/stack.c src/hierrors.c )
    install(TARGETS dump_read RUNTIME DESTINATION /usr/bin/)
endif()
