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
} sw_server_t;

static void* sw_worker(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);

    char buffer[1024];
    while (1) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            printf("Client disconnected.\n");
            close(client_fd);
            break;
        }
        buffer[n] = '\0';
        printf("Received: %s\n", buffer);
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
            if (server->running) {
                perror("accept failed");
            }
            continue;
        }
        printf("Client connected.\n");

        pthread_t thread;
        int* fd_ptr = malloc(sizeof(int));
        *fd_ptr = client_fd;
        if (pthread_create(&thread, NULL, sw_worker, fd_ptr) != 0) {
            perror("pthread_create failed");
            close(client_fd);
            free(fd_ptr);
        } else {
            pthread_detach(thread);
        }
    }

    return NULL;
}

static int sw_start(sw_server_t* server, unsigned short port) {
    server->port = port;
    server->running = 1;

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

    // Run server loop in a new thread
    if (pthread_create(&server->thread, NULL, sw_server_loop, server) != 0) {
        perror("pthread_create failed");
        close(server->server_fd);
        return -1;
    }

    pthread_detach(server->thread);  // auto-cleanup when server stops
    return 0;
}

static void sw_stop(sw_server_t* server) {
    server->running = 0;
    shutdown(server->server_fd, SHUT_RDWR);
    close(server->server_fd);
}


#endif // SOCKET_WORKER_H
