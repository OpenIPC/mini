#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <regex.h>


#include <sys/socket.h>
#include <netdb.h>

#include "mp4/nal.h"
#include "mp4/mp4.h"
#include <hi_comm_venc.h>
#include <config/sensor_config.h>

#include "tools.h"

#include "config/app_config.h"
#include "http_post.h"

#include "hi_jpeg.h"

#define tag "[http_post]: "
//
//struct PostData {
//    char* buf;
//    uint32_t size;
//};
//
//void* http_post_send(void *vargp)  {
//    struct PostData post_data = *((struct PostData *) vargp);
//    char *host_addr = app_config.http_post_host;
//
//    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
//    if (sockfd == -1) { printf(tag "socket creation failed...\n"); return NULL; }
//
//    struct addrinfo *server_addr;
//    int ret = getaddrinfo(host_addr, "80", NULL, &server_addr);
//    if (ret == 0) {
//        const struct addrinfo *r;
//        for (r = server_addr; r != NULL || ret != 0; r = r->ai_next)
//            ret = connect(sockfd, server_addr->ai_addr, server_addr->ai_addrlen);
//        // printf(tag "connected to the server '%s'..\n", host_addr);
//
//        char time_url[256]; {
//            time_t timer;
//            time(&timer);
//            struct tm* tm_info = localtime(&timer);
//            size_t time_len = strftime(time_url, sizeof(time_url), app_config.http_post_url, tm_info);
//            time_url[time_len++] = 0;
//        }
//        {
//            char header_buf[1024];
//            int buf_len = sprintf(header_buf,
//                              "PUT %s HTTP/1.1\r\n"
//                              "Host: %s\r\n"
//                              "User-Agent: Camera openipc.org\r\n"
//                              "Accept: */*\r\n"
//                              "Content-Type: image/jpeg\r\n"
//                              "Content-Length: %u\r\n",
//                              time_url,
//                              host_addr,
//                              post_data.size);
//            write(sockfd, header_buf, buf_len);
//
//            if (strlen(app_config.http_post_login) > 0 && strlen(app_config.http_post_password) > 0) {
//                char log_pass[128];
//                int log_pass_len = sprintf(log_pass, "%s:%s", app_config.http_post_login, app_config.http_post_password);
//                char base64buf[1024];
//                int base64_len = Base64encode(base64buf, log_pass, log_pass_len);
//                base64buf[base64_len++] = 0;
//                int buf_len = sprintf(header_buf, "Authorization: Basic %s\r\n", base64buf);
//                write(sockfd, header_buf, buf_len);
//            }
//            write(sockfd, "\r\n", 2);
//            write(sockfd, post_data.buf, post_data.size);
//
//            // printf(tag "send headers: %s\n", header_buf);
//
//            char replay[1024];
//            int len = read(sockfd, replay, 1024);
////            printf(tag "read %d\n", len);
////            printf(tag "read %s\n", replay);
//        }
//
//        close(sockfd);
//    }
//    freeaddrinfo(server_addr);
//}
//
//void http_post_send_jpeg(uint8_t chn_index, char *buf, ssize_t size) {
//    static time_t last_time = 0;
//    time_t current_time = time(NULL);
//    if (current_time - last_time < app_config.http_post_interval) { return; }
//
//    // printf(tag "http_post_send_jpeg %d\n", chn_index);
//    if (chn_index != app_config.http_post_chn) return;
//
//    static pthread_t http_post_thread_id = 0;
//
//    bool thread_running = true;
//    if (http_post_thread_id == 0) {
//        thread_running = false;
//    } else {
//        int ret = pthread_kill(http_post_thread_id, 0);
//        if (ret == 0) {
//            thread_running = true;
//        } else {
//            thread_running = false;
//            http_post_thread_id = NULL;
//        }
//    }
//
//    if (!thread_running) {
//        static struct PostData post_data;
//        free(post_data.buf);
//        post_data.size = size;
//        post_data.buf = malloc(size);
//        memcpy(post_data.buf, buf, size);
//
//        pthread_attr_t thread_attr;
//        pthread_attr_init(&thread_attr);
//        size_t stacksize;
//        pthread_attr_getstacksize(&thread_attr,&stacksize);
//        size_t new_stacksize = 16*1024;
//        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) { printf(tag "Error:  Can't set stack size %ld\n", new_stacksize); }
//        pthread_create(&http_post_thread_id, &thread_attr, http_post_send, (void *) &post_data);
//        if (pthread_attr_setstacksize(&thread_attr, stacksize)) { printf(tag "Error:  Can't set stack size %ld\n", stacksize); }
//        pthread_attr_destroy(&thread_attr);
//        last_time = current_time;
//    } else {
//        // printf(tag "thread already running\n");
//    }
//}


















HI_S32 post_send(struct JpegData *jpeg)  {
    char *host_addr = app_config.http_post_host;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) { printf(tag "socket creation failed...\n"); return NULL; }

    struct addrinfo *server_addr;
    int ret = getaddrinfo(host_addr, "80", NULL, &server_addr);
    if (ret == 0) {
        const struct addrinfo *r;
        for (r = server_addr; r != NULL || ret != 0; r = r->ai_next)
            ret = connect(sockfd, server_addr->ai_addr, server_addr->ai_addrlen);
        printf(tag "connected to the server '%s'..\n", host_addr);

        char time_url[256]; {
            time_t timer;
            time(&timer);
            struct tm* tm_info = localtime(&timer);
            size_t time_len = strftime(time_url, sizeof(time_url), app_config.http_post_url, tm_info);
            time_url[time_len++] = 0;
        }
        // printf(tag "url %s\n", time_url);
        {
            char header_buf[1024];
            int buf_len = sprintf(header_buf,
                                  "PUT %s HTTP/1.1\r\n"
                                  "Host: %s\r\n"
                                  "User-Agent: Camera openipc.org\r\n"
                                  "Accept: */*\r\n"
                                  "Content-Type: image/jpeg\r\n"
                                  "Content-Length: %u\r\n",
                                  time_url,
                                  host_addr,
                                  jpeg->jpeg_size);
            write(sockfd, header_buf, buf_len);

            if (strlen(app_config.http_post_login) > 0 && strlen(app_config.http_post_password) > 0) {
                char log_pass[128];
                int log_pass_len = sprintf(log_pass, "%s:%s", app_config.http_post_login, app_config.http_post_password);
                char base64buf[1024];
                int base64_len = Base64encode(base64buf, log_pass, log_pass_len);
                base64buf[base64_len++] = 0;
                int buf_len = sprintf(header_buf, "Authorization: Basic %s\r\n", base64buf);
                write(sockfd, header_buf, buf_len);
            }
            write(sockfd, "\r\n", 2);
            write(sockfd, jpeg->buf, jpeg->jpeg_size);

            // printf(tag "send headers: %s\n", header_buf);

            char replay[1024];
            int len = read(sockfd, replay, 1024);
//            printf(tag "read %d\n", len);
//            printf(tag "read %s\n", replay);
        }

        close(sockfd);
    }
    freeaddrinfo(server_addr);

    return HI_SUCCESS;
}

void* send_thread(void *vargp)  {
    struct JpegData jpeg = {0};
    jpeg.buf = NULL;
    jpeg.buf_size = 0;
    jpeg.jpeg_size = 0;
    sleep(3);
    while (keepRunning) {
        static time_t last_time = 0;
        time_t current_time = time(NULL);
        if (current_time - last_time < app_config.http_post_interval) { sleep(1); continue; }

        // printf(tag "start get_jpeg\n");
        HI_S32 s32Ret = get_jpeg(app_config.http_post_width, app_config.http_post_height, &jpeg);
        if (s32Ret != HI_SUCCESS) { printf(tag "get_jpeg error!\n"); continue; }
        last_time = current_time;

        // printf(tag "start post_send\n");
        s32Ret = post_send(&jpeg);
        if (s32Ret != HI_SUCCESS) { printf(tag "post_send error!\n"); continue; }
    }
}

void start_http_post_send() {
    pthread_t http_post_thread_id = 0;

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr,&stacksize);
    size_t new_stacksize = 16*1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) { printf(tag "Error:  Can't set stack size %ld\n", new_stacksize); }
    pthread_create(&http_post_thread_id, &thread_attr, send_thread, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize)) { printf(tag "Error:  Can't set stack size %ld\n", stacksize); }
    pthread_attr_destroy(&thread_attr);
}

