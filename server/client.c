/**
 * client.c — TCP client that connects to container's server
 * Compile: gcc -o client server/client.c
 * Run on host: ./client 10.200.1.2 8080
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    const char *ip   = argv[1];
    int         port = atoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(port),
    };
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }

    printf("[client] connected to %s:%d\n", ip, port);

    const char *msg = "Hello from host namespace!\n";
    send(sockfd, msg, strlen(msg), 0);

    char buf[1024];
    ssize_t n = recv(sockfd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        printf("[client] echo received: %s", buf);
    }

    close(sockfd);
    return 0;
}
