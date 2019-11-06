#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <regex.h>

#include "mp4/nal.h"
#include "mp4/mp4.h"
#include <hi_comm_venc.h>
#include <config/sensor_config.h>

#include "tools.h"

#include "hi_jpeg.h"

#include "config/app_config.h"

bool keepRunning = true; // keep running infinite loop while true

enum StreamType {
    STREAM_H264,
    STREAM_JPEG,
    STREAM_MJPEG,
    STREAM_MP4
};

struct Client {
    int socket_fd;
    enum StreamType type;

    struct Mp4State mp4_state;

    uint32_t nal_count;
};

// shared http video clients list
#define MAX_CLIENTS 50
struct Client client_fds[MAX_CLIENTS];
pthread_mutex_t client_fds_mutex;

void close_socket_fd(int socket_fd) {
    shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);
}

void free_client(int i) {
    if (client_fds[i].socket_fd < 0) return;
    close_socket_fd(client_fds[i].socket_fd);
    client_fds[i].socket_fd = -1;
}

#ifdef  __APPLE__
#define MSG_NOSIGNAL SO_NOSIGPIPE
#endif
int send_to_fd(int client_fd, char* buf, ssize_t size) {
    ssize_t sent = 0, len = 0;
    if (client_fd < 0) return -1;
    while (sent < size) {
        len = send(client_fd, buf + sent, size - sent, MSG_NOSIGNAL);
        if (len < 0) return -1;
        sent += len;
    }
    return 0;
}


int send_to_fd_nonblock(int client_fd, char* buf, ssize_t size) {
    if (client_fd < 0) return -1;
    send(client_fd, buf, size, MSG_DONTWAIT | MSG_NOSIGNAL);
    return 0;
}

int send_to_client(int i, char* buf, ssize_t size) {;
    if (send_to_fd(client_fds[i].socket_fd, buf, size) < 0) { free_client(i); return -1; }
    return 0;
}

void send_h264_to_client(uint8_t chn_index, const void *p) {
    const VENC_STREAM_S *stream = (const VENC_STREAM_S *)p;

    for (uint32_t i = 0; i < stream->u32PackCount; ++i) {
        VENC_PACK_S *pack = &stream->pstPack[i];
        uint32_t pack_len = pack->u32Len - pack->u32Offset;
        uint8_t *pack_data = pack->pu8Addr + pack->u32Offset;

        ssize_t nal_start = 3;
        if (!nal_chk3(pack_data, 0)) { ++nal_start; if(!nal_chk4(pack_data, 0)) continue; }

        struct NAL nal; nal_parse_header(&nal, (pack_data + nal_start)[0]);
//        switch (nal.unit_type) {
//            case NalUnitType_SPS: { set_sps(pack_data, pack_len); break; }
//            case NalUnitType_PPS: { set_pps(pack_data, pack_len); break; }
//            case NalUnitType_CodedSliceIdr: { set_slice(pack_data, pack_len); break; }
//            case NalUnitType_CodedSliceNonIdr: { set_slice(pack_data, pack_len); break; }
//            default: continue;
//        }
        // printf("NAL: %s\n", nal_type_to_str(nal.unit_type));

        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < MAX_CLIENTS; ++i) {
            if (client_fds[i].socket_fd < 0) continue;
            if (client_fds[i].type != STREAM_H264) continue;

            if (client_fds[i].nal_count == 0 && nal.unit_type != NalUnitType_SPS) continue;

            printf("NAL: %s send to %d\n", nal_type_to_str(nal.unit_type), i);

            static char len_buf[50];
            ssize_t len_size = sprintf(len_buf, "%zX\r\n", (ssize_t)pack_len);
            if (send_to_client(i, len_buf, len_size) < 0) continue; // send <SIZE>\r\n
            if (send_to_client(i, pack_data, pack_len) < 0) continue; // send <DATA>
            if (send_to_client(i, "\r\n", 2) < 0) continue; // send \r\n

            client_fds[i].nal_count++;
            if (client_fds[i].nal_count == 300) {
                char end[] = "0\r\n\r\n";
                if (send_to_client(i, end, sizeof(end)) < 0) continue;
                free_client(i);
            }
        }
        pthread_mutex_unlock(&client_fds_mutex);
    }
}


struct Mp4Context mp4_context;

void send_mp4_to_client(uint8_t chn_index, const void *p) {
    const VENC_STREAM_S *stream = (const VENC_STREAM_S *)p;

    for (uint32_t i = 0; i < stream->u32PackCount; ++i) {
        VENC_PACK_S *pack = &stream->pstPack[i];
        uint32_t pack_len = pack->u32Len - pack->u32Offset;
        uint8_t *pack_data = pack->pu8Addr + pack->u32Offset;

        ssize_t nal_start = 3;
        if (!nal_chk3(pack_data, 0)) { ++nal_start; if(!nal_chk4(pack_data, 0)) continue; }
        pack_data += nal_start;

        struct NAL nal; nal_parse_header(&nal, pack_data[0]);
        switch (nal.unit_type) {
            case NalUnitType_SPS: { set_sps(&mp4_context, pack_data, pack_len); break; }
            case NalUnitType_PPS: { set_pps(&mp4_context, pack_data, pack_len); break; }
            case NalUnitType_CodedSliceIdr: { set_slice(&mp4_context, pack_data, pack_len, nal.unit_type); break; }
            case NalUnitType_CodedSliceNonIdr: { set_slice(&mp4_context, pack_data, pack_len, nal.unit_type); break; }
            default: continue;
        }

//        static uint64_t last_pts = 0;
//        uint64_t delta = stream->pstPack->u64PTS - last_pts;
//        // printf("nal %s       pts: %lld       delta: %lld\n", nal_type_to_str(nal.unit_type), stream->pstPack->u64PTS, delta);
//        printf("%d   nal %s    delta: %lld\n", chn_index, nal_type_to_str(nal.unit_type), delta);
//        last_pts = stream->pstPack->u64PTS;

        if (nal.unit_type != NalUnitType_CodedSliceIdr && nal.unit_type != NalUnitType_CodedSliceNonIdr) continue;

        static enum BufError err;
        static char len_buf[50];
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < MAX_CLIENTS; ++i) {
            if (client_fds[i].socket_fd < 0) continue;
            if (client_fds[i].type != STREAM_MP4) continue;

            if (!client_fds[i].mp4_state.header_sent) {
                struct BitBuf header_buf;
                err = get_header(&mp4_context, &header_buf); chk_err_continue
                ssize_t len_size = sprintf(len_buf, "%zX\r\n", header_buf.offset);
                if (send_to_client(i, len_buf, len_size) < 0) continue; // send <SIZE>\r\n
                if (send_to_client(i, header_buf.buf, header_buf.offset) < 0) continue; // send <DATA>
                if (send_to_client(i, "\r\n", 2) < 0) continue; // send \r\n

                client_fds[i].mp4_state.sequence_number = 1;
                client_fds[i].mp4_state.base_data_offset = header_buf.offset;
                client_fds[i].mp4_state.base_media_decode_time = 0;
                client_fds[i].mp4_state.header_sent = true;
                client_fds[i].mp4_state.nals_count = 0;
                client_fds[i].mp4_state.default_sample_duration = default_sample_size;
            }

            err = set_mp4_state(&mp4_context, &client_fds[i].mp4_state); chk_err_continue
            {
                struct BitBuf moof_buf;
                err = get_moof(&mp4_context, &moof_buf); chk_err_continue
                ssize_t len_size = sprintf(len_buf, "%zX\r\n", (ssize_t)moof_buf.offset);
                if (send_to_client(i, len_buf, len_size) < 0) continue; // send <SIZE>\r\n
                if (send_to_client(i, moof_buf.buf, moof_buf.offset) < 0) continue; // send <DATA>
                if (send_to_client(i, "\r\n", 2) < 0) continue; // send \r\n
            }
            {
                struct BitBuf mdat_buf;
                err = get_mdat(&mp4_context, &mdat_buf); chk_err_continue
                ssize_t len_size = sprintf(len_buf, "%zX\r\n", (ssize_t)mdat_buf.offset);
                if (send_to_client(i, len_buf, len_size) < 0) continue; // send <SIZE>\r\n
                if (send_to_client(i, mdat_buf.buf, mdat_buf.offset) < 0) continue; // send <DATA>
                if (send_to_client(i, "\r\n", 2) < 0) continue; // send \r\n
            }
            // client_fds[i].mp4_state.nals_count++;
            // if (client_fds[i].mp4_state.nals_count == 300) {
            //     char end[] = "0\r\n\r\n";
            //     if (send_to_client(i, end, sizeof(end)) < 0) continue;
            //     free_client(i);
            // }
        }
        pthread_mutex_unlock(&client_fds_mutex);
    }
}

void send_mjpeg(uint8_t chn_index, char *buf, ssize_t size) {
    static char prefix_buf[128];
    ssize_t prefix_size = sprintf(prefix_buf, "--boundarydonotcross\r\nContent-Type:image/jpeg\r\nContent-Length: %lu\r\n\r\n", size);
    buf[size++] = '\r'; buf[size++] = '\n';

    pthread_mutex_lock(&client_fds_mutex);
    for (uint32_t i = 0; i < MAX_CLIENTS; ++i) {
        if (client_fds[i].socket_fd < 0) continue;
        if (client_fds[i].type != STREAM_MJPEG) continue;
        if (send_to_client(i, prefix_buf, prefix_size) < 0) continue; // send <SIZE>\r\n
        if (send_to_client(i, buf, size) < 0) continue; // send <DATA>\r\n
    }
    pthread_mutex_unlock(&client_fds_mutex);
}

void send_jpeg(uint8_t chn_index, char *buf, ssize_t size) {
    static char prefix_buf[128];
    ssize_t prefix_size = sprintf(prefix_buf, "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n", size);
    buf[size++] = '\r'; buf[size++] = '\n';

    pthread_mutex_lock(&client_fds_mutex);
    for (uint32_t i = 0; i < MAX_CLIENTS; ++i) {
        if (client_fds[i].socket_fd < 0) continue;
        if (client_fds[i].type != STREAM_JPEG) continue;
        if (send_to_client(i, prefix_buf, prefix_size) < 0) continue; // send <SIZE>\r\n
        if (send_to_client(i, buf, size) < 0) continue; // send <DATA>\r\n
        free_client(i);
    }
    pthread_mutex_unlock(&client_fds_mutex);
}


struct JpegTask {
    int client_fd;
    uint16_t width;
    uint16_t height;
    uint8_t qfactor;
    uint8_t color2Gray;
};
void* send_jpeg_thread(void *vargp) {
    // int client_fd = *((int *) vargp);
    struct JpegTask task = *((struct JpegTask *) vargp);
    struct JpegData jpeg = {0};
    printf("try to request jpeg (%ux%u, qfactor %u, color2Gray %d) from hisdk...\n", task.width, task.height, task.qfactor, task.color2Gray);
    HI_S32 s32Ret = get_jpeg(task.width, task.height, task.qfactor, task.color2Gray, &jpeg);
    if (s32Ret != HI_SUCCESS) {
        printf("can't get jpeg from hisdk...\n");
        static char response[] = "HTTP/1.1 503 Internal Error\r\nContent-Length: 11\r\nConnection: close\r\n\r\nHello, 503!";
        send_to_fd(task.client_fd, response, sizeof(response) - 1); // zero ending string!
        close_socket_fd(task.client_fd);
        return NULL;
    }
    printf("request jpeg from hisdk is ok\n");
    char buf[1024];
    int buf_len = sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n", jpeg.jpeg_size);
    send_to_fd(task.client_fd, buf, buf_len);
    send_to_fd(task.client_fd, jpeg.buf, jpeg.jpeg_size);
    send_to_fd(task.client_fd, "\r\n", 2);
    close_socket_fd(task.client_fd);
    free(jpeg.buf);
    printf("jpeg was send\n");
    return NULL;
}

int send_file(const int client_fd, const char *path) {
    if(access(path, F_OK) != -1) { // file exists
        const char* mime = getMime(path);
        FILE *file = fopen(path, "r");
        if (file == NULL) { close_socket_fd(client_fd); return 0; }
        char header[1024];
        int header_len = sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n", mime);
        send_to_fd(client_fd, header, header_len); // zero ending string!
        const int buf_size = 1024;
        char buf[buf_size+2];
        char len_buf[50]; ssize_t len_size;
        while (1) {
            ssize_t size = fread(buf, sizeof(char), buf_size, file);
            if (size <= 0) { break; }
            len_size = sprintf(len_buf, "%zX\r\n", size);
            buf[size++] = '\r'; buf[size++] = '\n';
            send_to_fd(client_fd, len_buf, len_size); // send <SIZE>\r\n
            send_to_fd(client_fd, buf, size); // send <DATA>\r\n
        }
        char end[] = "0\r\n\r\n";
        send_to_fd(client_fd, end, sizeof(end));
        fclose(file);
        close_socket_fd(client_fd);
        return 1;
    }
    return 0;
}

int send_mjpeg_html(const int client_fd) {
    char html[] = "<html>\n"
                  "    <head>\n"
                  "        <title>MJPG-Streamer - Stream Example</title>\n"
                  "    </head>\n"
                  "    <body>\n"
                  "        <center>\n"
                  "            <img src=\"mjpeg\" />\n"
                  "        </center>\n"
                  "    </body>\n"
                  "</html>";
    char buf[1024];
    int buf_len = sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n%s", strlen(html), html);
    buf[buf_len++] = 0;
    send_to_fd(client_fd, buf, buf_len);
    close_socket_fd(client_fd);
    return 1;
}

int send_video_html(const int client_fd) {
    char html[] = "<html>\n"
                  "    <head>\n"
                  "        <title>MP4 Streamer</title>\n"
                  "    </head>\n"
                  "    <body>\n"
                  "        <center>\n"
                  "            <video width=\"700\" src=\"video.mp4\" autoplay controls />\n"
                  "        </center>\n"
                  "    </body>\n"
                  "</html>";
    char buf[1024];
    int buf_len = sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n%s", strlen(html), html);
    buf[buf_len++] = 0;
    send_to_fd(client_fd, buf, buf_len);
    close_socket_fd(client_fd);
    return 1;
}

int send_image_html(const int client_fd) {
    char html[] = "<html>\n"
                  "    <head>\n"
                  "        <title>Jpeg Example</title>\n"
                  "    </head>\n"
                  "    <body>\n"
                  "        <center>\n"
                  "            <img src=\"image.jpg\" />\n"
                  "        </center>\n"
                  "    </body>\n"
                  "</html>";
    char buf[1024];
    int buf_len = sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n%s", strlen(html), html);
    buf[buf_len++] = 0;
    send_to_fd(client_fd, buf, buf_len);
    close_socket_fd(client_fd);
    return 1;
}

#define MAX_HEADERS 1024*8
char request_headers[MAX_HEADERS];
char request_path[64];
char header[256];

void *server_thread(void *vargp) {
    memset(&mp4_context, 0, sizeof(struct Mp4Context));

    int server_fd = *((int *) vargp);
    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        printf("Web server error: setsockopt(SO_REUSEADDR) failed"); fflush(stdout);
    }
    // int set = 1;
    // setsockopt(server_fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(app_config.web_port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    int res = bind(server_fd, (struct sockaddr*) &server, sizeof(server));
    if (res != 0) {
        printf("Web server error: %s (%d)\n", strerror(errno), errno);
        keepRunning = false;
        close_socket_fd(server_fd);
        return NULL;
    }
    listen(server_fd, 128);

    while (keepRunning) {
        // waiting for a new connection
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) break;

        // parse request headers, get request path
        recv(client_fd, request_headers, MAX_HEADERS, 0);
        if(!parseRequestPath(request_headers, request_path)) { close_socket_fd(client_fd); continue; };

        if (strcmp(request_path, "./exit") == 0) {
            // exit
            char response[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 11\r\nConnection: close\r\n\r\nGoodBye!!!!";
            send_to_fd(client_fd, response, sizeof(response) - 1); // zero ending string!
            close_socket_fd(client_fd);
            keepRunning = false; break;
        }

        // if path is root send ./index.html file
        if (strcmp(request_path, "./") == 0) strcpy(request_path, "./mjpeg.html");

        // send JPEG html page
        if (strcmp(request_path, "./image.html") == 0 && app_config.jpeg_enable) { send_image_html(client_fd); continue; }
        // send MJPEG html page
        if (strcmp(request_path, "./mjpeg.html") == 0 && app_config.mjpeg_enable) { send_mjpeg_html(client_fd); continue; }
        // send MP4 html page
        if (strcmp(request_path, "./video.html") == 0 && app_config.mp4_enable) { send_video_html(client_fd); continue; }

        // if h264 stream is requested add client_fd socket to client_fds array and send h264 stream with http_thread
        if (strcmp(request_path, "./video.h264") == 0) {
            int header_len = sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n");
            send_to_fd(client_fd, header, header_len);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) { client_fds[i].socket_fd = client_fd; client_fds[i].type = STREAM_H264; client_fds[i].nal_count = 0; break; }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        if (strcmp(request_path, "./video.mp4") == 0 && app_config.mp4_enable) {
            int header_len = sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n");
            send_to_fd(client_fd, header, header_len);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) { client_fds[i].socket_fd = client_fd; client_fds[i].type = STREAM_MP4; client_fds[i].mp4_state.header_sent = false; break; }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        // if mjpeg stream is requested add client_fd socket to client_fds array and send mjpeg stream with http_thread
        if (strcmp(request_path, "./mjpeg") == 0 && app_config.mjpeg_enable) {
            int header_len = sprintf(header, "HTTP/1.0 200 OK\r\nCache-Control: no-cache\r\nPragma: no-cache\r\nConnection: close\r\nContent-Type: multipart/x-mixed-replace; boundary=boundarydonotcross\r\n\r\n");
            send_to_fd(client_fd, header, header_len);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) { client_fds[i].socket_fd = client_fd; client_fds[i].type = STREAM_MJPEG; break; }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

//        if (strcmp(request_path, "./image.jpg") == 0 && app_config.jpeg_enable) {
//            pthread_mutex_lock(&client_fds_mutex);
//            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
//                if (client_fds[i].socket_fd < 0) { client_fds[i].socket_fd = client_fd; client_fds[i].type = STREAM_JPEG; break; }
//            pthread_mutex_unlock(&client_fds_mutex);
//            continue;
//        }

        if (startsWith(request_path, "./image.jpg") && app_config.jpeg_enable) {
            {
                struct JpegTask task;
                task.client_fd = client_fd;
                task.width = app_config.jpeg_width;
                task.height = app_config.jpeg_height;
                task.qfactor = app_config.jpeg_qfactor;
                task.color2Gray = 3;

                get_uint16(request_path, "width=", &task.width);
                get_uint16(request_path, "height=", &task.height);
                get_uint8(request_path, "qfactor=", &task.qfactor);
                get_uint8(request_path, "color2gray=", &task.color2Gray);

                pthread_t thread_id;
                pthread_attr_t thread_attr;
                pthread_attr_init(&thread_attr);
                size_t stacksize;
                pthread_attr_getstacksize(&thread_attr,&stacksize);
                size_t new_stacksize = 16*1024;
                if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) { printf("Error:  Can't set stack size %ld\n", new_stacksize); }
                pthread_create(&thread_id, &thread_attr, send_jpeg_thread, (void *) &task);
                if (pthread_attr_setstacksize(&thread_attr, stacksize)) { printf("Error:  Can't set stack size %ld\n", stacksize); }
                pthread_attr_destroy(&thread_attr);
            }
            continue;
        }

        // try to send static file
        if (app_config.web_enable_static && send_file(client_fd, request_path)) continue;

        // 404
        static char response[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 11\r\nConnection: close\r\n\r\nHello, 404!";
        send_to_fd(client_fd, response, sizeof(response) - 1); // zero ending string!
        close_socket_fd(client_fd);
    }
    close_socket_fd(server_fd);
    printf("Shutdown server thread\n");
    return NULL;
}

void sig_handler(int signo) { printf("Graceful shutdown...\n"); keepRunning = false; }
void epipe_handler(int signo) { printf("EPIPE\n"); }
void spipe_handler(int signo) { printf("SIGPIPE\n"); }

int server_fd = -1;
pthread_t server_thread_id;

int start_server() {
    if (signal(SIGINT,  sig_handler) == SIG_ERR) printf("Error: can't catch SIGINT\n");
    if (signal(SIGQUIT, sig_handler) == SIG_ERR) printf("Error: can't catch SIGQUIT\n");
    if (signal(SIGTERM, sig_handler) == SIG_ERR) printf("Error: can't catch SIGTERM\n");

    if (signal(SIGPIPE, spipe_handler) == SIG_ERR) printf("Error: can't catch SIGPIPE\n");
    if (signal(EPIPE, epipe_handler) == SIG_ERR) printf("Error: can't catch EPIPE\n");

    // set clients_fds list to -1
    for (uint32_t i = 0; i < MAX_CLIENTS; ++i) { client_fds[i].socket_fd = -1; client_fds[i].type = -1; }
    pthread_mutex_init(&client_fds_mutex, NULL);

    // start server and http video stream threads
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        size_t stacksize;
        pthread_attr_getstacksize(&thread_attr,&stacksize);
        size_t new_stacksize = app_config.web_server_thread_stack_size;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) { printf("Error:  Can't set stack size %ld\n", new_stacksize); }
        pthread_create(&server_thread_id, &thread_attr, server_thread, (void *) &server_fd);
        if (pthread_attr_setstacksize(&thread_attr, stacksize)) { printf("Error:  Can't set stack size %ld\n", stacksize); }
        pthread_attr_destroy(&thread_attr);
    }

    return EXIT_SUCCESS;
}

int stop_server() {
    keepRunning = false;

    // stop server_thread while server_fd is closed
    close_socket_fd(server_fd);
    pthread_join(server_thread_id, NULL);

    pthread_mutex_destroy(&client_fds_mutex);
    printf("Shutdown server\n");
    return EXIT_SUCCESS;
}
