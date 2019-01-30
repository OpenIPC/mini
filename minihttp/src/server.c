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

#include "tools.h"

int keepRunning = 1; // keep running infinite loop while true

enum StreamType {
    STREAM_H264,
    STREAM_JPEG,
    STREAM_MJPEG
};

struct Client {
    int socket_fd;
    enum StreamType type;
};

// shared http video clients list
#define MAX_CLIENTS 10
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

int send_to_client(int i, char* buf, ssize_t size) {;
    if (send_to_fd(client_fds[i].socket_fd, buf, size) < 0) { free_client(i); return -1; }
    return 0;
}

//void *http_thread_h264(void *vargp) {
//    int read_fd = *((int *) vargp);
//    const int READ_BUF_SIZE = 256*1024;
//    char buf[READ_BUF_SIZE + 2]; ssize_t size;
//    char len_buf[50]; ssize_t len_size;
//    while (keepRunning) {
//        size = read(read_fd, buf, READ_BUF_SIZE);
//        len_size = sprintf(len_buf, "%zX\r\n", size);
//        buf[size++] = '\r'; buf[size++] = '\n';
//
//        pthread_mutex_lock(&client_fds_mutex);
//        for (uint32_t i = 0; i < MAX_CLIENTS; ++i) {
//            if (client_fds[i].socket_fd < 0) continue;
//            if (client_fds[i].type != STREAM_H264) continue;
//            if (send_to_client(i, len_buf, len_size) < 0) continue; // send <SIZE>\r\n
//            if (send_to_client(i, buf, size) < 0) continue; // send <DATA>\r\n
//        }
//        pthread_mutex_unlock(&client_fds_mutex);
//    }
//
//    pthread_mutex_lock(&client_fds_mutex);
//    char end[] = "0\r\n\r\n";
//    for (uint32_t i = 0; i < MAX_CLIENTS; ++i) {
//        if (client_fds[i].socket_fd < 0) continue;
//        if (client_fds[i].type != STREAM_H264) continue;
//        // send 0\r\n\r\n
//        if (send_to_client(i, end, sizeof(end)) < 0) continue;
//        free_client(i);
//    }
//    pthread_mutex_unlock(&client_fds_mutex);
//    printf("Shutdown http thread\n");
//    return NULL;
//}

void send_mjpeg(char *buf, ssize_t size) {
    static char prefix_buf[128];
    ssize_t prefix_size = sprintf(prefix_buf, "--boundarydonotcross\r\nContent-Type:image/jpeg\r\nContent-Length: %d\r\n\r\n", size);
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

void send_jpeg(char *buf, ssize_t size) {
    static char prefix_buf[128];
    ssize_t prefix_size = sprintf(prefix_buf, "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: %zX\r\nConnection: close\r\n\r\n", size);
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
    int buf_len = sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zX\r\nConnection: close\r\n\r\n%s", strlen(html), html);
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
    int buf_len = sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zX\r\nConnection: close\r\n\r\n%s", strlen(html), html);
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
    int server_fd = *((int *) vargp);
    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        printf("setsockopt(SO_REUSEADDR) failed"); fflush(stdout);
    }
    // int set = 1;
    // setsockopt(server_fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    int res = bind(server_fd, (struct sockaddr*) &server, sizeof(server));
    if (res != 0) {
        printf("Error: %s (%d)\n", strerror(errno), errno);
        keepRunning = 0;
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
            keepRunning = 0; break;
        }

        // if path is root send ./index.html file
        if (strcmp(request_path, "./") == 0) strcpy(request_path, "./mjpeg.html");

        // send JPEG html page
        if (strcmp(request_path, "./image.html") == 0) { send_image_html(client_fd); continue; }
        // send MJPEG html page
        if (strcmp(request_path, "./mjpeg.html") == 0) { send_mjpeg_html(client_fd); continue; }

        // if h264 stream is requested add client_fd socket to client_fds array and send h264 stream with http_thread
        if (strcmp(request_path, "./h264") == 0) {
            char *mime = "text/plain"; // application/octet-stream
            int header_len = sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n", mime);
            send_to_fd(client_fd, header, header_len);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) { client_fds[i].socket_fd = client_fd; client_fds[i].type = STREAM_H264; break; }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        // if mjpeg stream is requested add client_fd socket to client_fds array and send mjpeg stream with http_thread
        if (strcmp(request_path, "./mjpeg") == 0) {
            int header_len = sprintf(header, "HTTP/1.0 200 OK\r\nCache-Control: no-cache\r\nPragma: no-cache\r\nConnection: close\r\nContent-Type: multipart/x-mixed-replace; boundary=boundarydonotcross\r\n\r\n");
            send_to_fd(client_fd, header, header_len);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) { client_fds[i].socket_fd = client_fd; client_fds[i].type = STREAM_MJPEG; break; }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        if (strcmp(request_path, "./image.jpg") == 0) {
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) { client_fds[i].socket_fd = client_fd; client_fds[i].type = STREAM_JPEG; break; }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        // try to send static file
        if (send_file(client_fd, request_path)) continue;

        // 404
        static char response[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 11\r\nConnection: close\r\n\r\nHello, 404!";
        send_to_fd(client_fd, response, sizeof(response) - 1); // zero ending string!
        close_socket_fd(client_fd);
    }
    close_socket_fd(server_fd);
    printf("Shutdown server thread\n");
    return NULL;
}

void sig_handler(int signo) { printf("Graceful shutdown...\n"); keepRunning = 0; }
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
        size_t new_stacksize = 16*1024;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) { printf("Error:  Can't set stack size %ld\n", new_stacksize); }
        pthread_create(&server_thread_id, &thread_attr, server_thread, (void *) &server_fd);
        if (pthread_attr_setstacksize(&thread_attr, stacksize)) { printf("Error:  Can't set stack size %ld\n", stacksize); }
        pthread_attr_destroy(&thread_attr);
    }

    return EXIT_SUCCESS;
}

int stop_server() {
    keepRunning = 0;

    // stop server_thread while server_fd is closed
    close_socket_fd(server_fd);
    pthread_join(server_thread_id, NULL);

    pthread_mutex_destroy(&client_fds_mutex);
    printf("Shutdown server\n");
    return EXIT_SUCCESS;
}
