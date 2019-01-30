#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include "hidemo.h"
#include "server.h"

#include "rtsp/rtspservice.h"
#include "rtsp/rtputils.h"
#include "rtsp/ringfifo.h"

void Usage(char *sPrgNm) {
    printf("Usage : %s <path to sensor config ini>\n", sPrgNm);
    printf("   ex: %s ./configs/imx222_1080p_line.ini\n", sPrgNm);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { Usage(argv[0]); return EXIT_FAILURE; }

    const char* config_path = argv[1];
    if (argc > 2) { dump_jpg = atoi(argv[2]); }

    start_server();

    struct SDKState state;
    memset(&state, 0, sizeof(struct SDKState));
    state.mjpeg_chn = 0;
    state.h264_chn = 1;
    state.jpeg_chn = 2;


    ringmalloc(1920*1080);
    printf("RTSP server START\n");
    PrefsInit();
    printf("listen for client connecting...\n");
    signal(SIGINT, IntHandl);
    int s32MainFd = tcp_listen(SERVER_RTSP_PORT_DEFAULT);
    if (ScheduleInit() == ERR_FATAL)
    {
        fprintf(stderr,"Fatal: Can't start scheduler %s, %i \nServer is aborting.\n", __FILE__, __LINE__);
        return 0;
    }
    RTP_port_pool_init(RTP_DEFAULT_PORT);


    if(start_sdk(config_path, &state) == EXIT_FAILURE) return EXIT_FAILURE;
    // TODO when return EXIT_FAILURE need to deinitialize sdk correctly
    // while (keepRunning) sleep(1);

    struct timespec ts = { 2, 0 };
    while (keepRunning) {
        nanosleep(&ts, NULL);
        EventLoop(s32MainFd);
    }

    stop_sdk(&state);
    ringfree();

    printf("Shutdown main thread\n");
    return EXIT_SUCCESS;
}
