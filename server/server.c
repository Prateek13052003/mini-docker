/**
 * server.c — Simple TCP echo server
 * Must be compiled statically and placed inside rootfs/server/
 *
 * Compile: gcc -static -o rootfs/server/server server/server.c
 * Run inside container: /server/server 8080
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BACKLOG 5
#define BUFSIZE 1024

int main(int argc, char *argv[]) {
    int port = (argc >= 2) ? atoi(argv[1]) : 8080;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(port),
    };

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    listen(sockfd, BACKLOG);
    printf("[server] listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof(cli);
        int conn = accept(sockfd, (struct sockaddr *)&cli, &cli_len);
        if (conn < 0) { perror("accept"); continue; }

        printf("[server] connection from %s\n", inet_ntoa(cli.sin_addr));

        char buf[BUFSIZE];
        ssize_t n;
        while ((n = recv(conn, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[n] = '\0';
            printf("[server] received: %s", buf);
            send(conn, buf, n, 0);   /* echo back */
        }
        close(conn);
        printf("[server] connection closed\n");
    }

    close(sockfd);
    return 0;
}
