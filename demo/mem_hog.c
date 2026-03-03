/**
 * mem_hog.c — Allocates memory until killed by OOM or hits limit
 * Compile statically and place inside rootfs
 * Compile: gcc -static -o rootfs/bin/mem_hog demo/mem_hog.c
 * Run inside container: /bin/mem_hog
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHUNK_SIZE (10 * 1024 * 1024)   /* 10 MiB per step */

int main(void) {
    size_t total = 0;
    printf("[mem_hog] Allocating memory in 10 MiB chunks...\n");
    printf("[mem_hog] Container memory limit: 128 MiB\n");
    printf("[mem_hog] Will be OOM-killed when limit is reached\n\n");

    while (1) {
        char *p = malloc(CHUNK_SIZE);
        if (!p) {
            printf("[mem_hog] malloc failed at %zu MiB\n", total / (1024*1024));
            break;
        }
        /* actually touch the pages so OS allocates physical memory */
        memset(p, 0xAB, CHUNK_SIZE);
        total += CHUNK_SIZE;
        printf("[mem_hog] Allocated %4zu MiB so far...\n",
               total / (1024 * 1024));
        sleep(1);
    }

    printf("[mem_hog] Done — exiting\n");
    return 0;
}
