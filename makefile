CC      = gcc
CFLAGS  = -Wall -Wextra -O2
STATIC  = -static

.PHONY: all clean

all: container client rootfs_bins

# Main runtime — dynamic is fine (needs kernel syscall interfaces)
container: src/container.c
	$(CC) $(CFLAGS) -o container src/container.c

# Client runs on host
client: server/client.c
	$(CC) $(CFLAGS) -o client server/client.c

# Binaries inside the rootfs MUST be static (no dynamic linker in minimal rootfs)
rootfs_bins: rootfs/server/server rootfs/bin/mem_hog rootfs/bin/cpu_hog

rootfs/server/server: server/server.c
	mkdir -p rootfs/server
	$(CC) $(CFLAGS) $(STATIC) -o rootfs/server/server server/server.c

rootfs/bin/mem_hog: demo/mem_hog.c
	mkdir -p rootfs/bin
	$(CC) $(CFLAGS) $(STATIC) -o rootfs/bin/mem_hog demo/mem_hog.c

rootfs/bin/cpu_hog: demo/cpu_hog.c
	mkdir -p rootfs/bin
	$(CC) $(CFLAGS) $(STATIC) -o rootfs/bin/cpu_hog demo/cpu_hog.c

clean:
	rm -f container client
	rm -f rootfs/server/server rootfs/bin/mem_hog rootfs/bin/cpu_hog
