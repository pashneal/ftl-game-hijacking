#ifndef SOCKET_WORKER_H
#define SOCKET_WORKER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

typedef struct {
    int server_fd;
    unsigned short port;
    int running;
    pthread_t thread;
    void (*on_message)(char *data, int len, char *out, int* outlen); // callback pointer
} sw_server_t;

static void* sw_worker(void* arg) {
    puts("Worker thread started.");
    struct {
        int client_fd;
        void (*cb)(char*, int, char*, int*);
    } *ctx = arg;

    int client_fd = ctx->client_fd;
    void (*cb)(char*, int, char*, int*) = ctx->cb;
    free(ctx);

    char inbuf[1024];
    char outbuf[10000];

    while (1) {
        ssize_t n = recv(client_fd, inbuf, sizeof(inbuf), 0);
        if (n <= 0) {
            close(client_fd);
            break;
        }

        int out_len = 0;
        if (cb) {
            cb(inbuf, (int)n, outbuf, &out_len);
            if (out_len > 0) {
                send(client_fd, outbuf, out_len, 0);
            }
        }
    }
    return NULL;
}

static void* sw_server_loop(void* arg) {
    sw_server_t* server = (sw_server_t*)arg;

    printf("Server listening on port %u...\n", server->port);

    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server->server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!server->running) break;
            perror("accept failed");
            continue;
        }

        pthread_t thread;
        struct {
            int client_fd;
            void (*cb)(char *, int, char *, int *);
        } *ctx = malloc(sizeof(*ctx));
        ctx->client_fd = client_fd;
        ctx->cb = server->on_message;

        if (pthread_create(&thread, NULL, sw_worker, ctx) != 0) {
            perror("pthread_create failed");
            close(client_fd);
            free(ctx);
        } else {
            pthread_detach(thread);
        }
    }

    return NULL;
}

static int sw_start(sw_server_t* server, unsigned short port, void (*on_message)(char *data, int len, char *outbuf, int *out_len)) {
    server->port = port;
    server->running = 1;
    server->on_message = on_message;

    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        perror("socket failed");
        return -1;
    }

    int opt = 1;
    setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(server->server_fd);
        return -1;
    }

    if (listen(server->server_fd, 5) < 0) {
        perror("listen failed");
        close(server->server_fd);
        return -1;
    }

    if (pthread_create(&server->thread, NULL, sw_server_loop, server) != 0) {
        perror("pthread_create failed");
        close(server->server_fd);
        return -1;
    }

    pthread_detach(server->thread);
    return 0;
}

static void sw_stop(sw_server_t* server) {
    if (!server->running) return;
    server->running = 0;
    shutdown(server->server_fd, SHUT_RDWR);
    close(server->server_fd);
}

#endif // SOCKET_WORKER_H
