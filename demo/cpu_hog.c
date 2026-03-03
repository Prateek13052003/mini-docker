/**
 * cpu_hog.c — Spins CPU for N seconds
 * Compile: gcc -static -o rootfs/bin/cpu_hog demo/cpu_hog.c
 * Run inside container: /bin/cpu_hog 30
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
    int secs = (argc >= 2) ? atoi(argv[1]) : 20;
    printf("[cpu_hog] Burning CPU for %d seconds...\n", secs);

    time_t end = time(NULL) + secs;
    volatile long long n = 0;
    while (time(NULL) < end)
        n++;   /* hot spin */

    printf("[cpu_hog] Done (counted to %lld)\n", n);
    return 0;
}
