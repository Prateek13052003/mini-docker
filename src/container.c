/**
 * container.c  —  Minimal Linux Container Runtime
 * Compile:  gcc -o container src/container.c
 * Run:      sudo ./container <rootfs> <hostname> <netns_name> [cmd]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sched.h>

#define STACK_SIZE  (1024 * 1024)
#define CGROUP_BASE "/sys/fs/cgroup"
#define MEM_LIMIT   "134217728"     /* 128 MiB */

typedef struct {
    char *rootfs;
    char *hostname;
    char *netns;
    char *cmd;
} ContainerArgs;

static void die(const char *msg) { perror(msg); exit(EXIT_FAILURE); }

static void write_file(const char *path, const char *data) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    write(fd, data, strlen(data));
    close(fd);
}

static int setup_cgroup(const char *name) {
    char path[PATH_MAX], lp[PATH_MAX];
    int v1 = 0;

    /* try cgroup v1 */
    snprintf(path, sizeof(path), "%s/memory/container_%s", CGROUP_BASE, name);
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        snprintf(lp, sizeof(lp), "%s/memory.limit_in_bytes", path);
        if (access(lp, W_OK) == 0) {
            write_file(lp, MEM_LIMIT);
            snprintf(lp, sizeof(lp), "%s/memory.memsw.limit_in_bytes", path);
            write_file(lp, MEM_LIMIT);
            printf("[cgroup v1] memory limit = 128 MiB  (container_%s)\n", name);
            v1 = 1;
        }
    }

    if (!v1) {
        /* cgroup v2 */
        snprintf(path, sizeof(path), "%s/container_%s", CGROUP_BASE, name);
        mkdir(path, 0755);
        write_file(CGROUP_BASE "/cgroup.subtree_control", "+memory");
        snprintf(lp, sizeof(lp), "%s/memory.max", path);
        write_file(lp, MEM_LIMIT);
        printf("[cgroup v2] memory.max = 128 MiB  (container_%s)\n", name);
    }
    return v1;
}

static void add_to_cgroup(const char *name, pid_t pid, int v1) {
    char path[PATH_MAX], pidbuf[32];
    snprintf(pidbuf, sizeof(pidbuf), "%d\n", pid);
    if (v1)
        snprintf(path, sizeof(path),
                 "%s/memory/container_%s/cgroup.procs", CGROUP_BASE, name);
    else
        snprintf(path, sizeof(path),
                 "%s/container_%s/cgroup.procs", CGROUP_BASE, name);
    write_file(path, pidbuf);
}

static void cleanup_cgroup(const char *name) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/memory/container_%s", CGROUP_BASE, name);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/container_%s", CGROUP_BASE, name);
    rmdir(path);
}

static void join_netns(const char *netns) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/var/run/netns/%s", netns);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[warn] cannot open netns '%s': %s\n",
                netns, strerror(errno));
        return;
    }
    if (setns(fd, CLONE_NEWNET) < 0) die("setns");
    close(fd);
}

static void setup_mounts(const char *rootfs) {
    if (mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0)
        die("mount --make-rprivate /");

    if (mount(rootfs, rootfs, "bind", MS_BIND | MS_REC, NULL) < 0)
        die("bind mount rootfs");

    char put_old[PATH_MAX];
    snprintf(put_old, sizeof(put_old), "%s/.pivot_old", rootfs);
    mkdir(put_old, 0700);

    if (syscall(SYS_pivot_root, rootfs, put_old) == 0) {
        chdir("/");
        umount2("/.pivot_old", MNT_DETACH);
        rmdir("/.pivot_old");
    } else {
        fprintf(stderr, "[warn] pivot_root failed (%s) — using chroot\n",
                strerror(errno));
        if (chroot(rootfs) < 0) die("chroot");
        chdir("/");
    }

    mkdir("/proc", 0555);
    if (mount("proc", "/proc", "proc",
              MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) < 0)
        fprintf(stderr, "[warn] mount proc: %s\n", strerror(errno));

    mkdir("/sys",  0555);
    mount("sysfs", "/sys", "sysfs",
          MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_RDONLY, NULL);

    mkdir("/dev", 0755);
    mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID | MS_STRICTATIME, NULL);

    mkdir("/tmp", 01777);
    mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, NULL);
}

static int child_main(void *arg) {
    ContainerArgs *a = (ContainerArgs *)arg;

    join_netns(a->netns);

    if (sethostname(a->hostname, strlen(a->hostname)) < 0)
        die("sethostname");

    setup_mounts(a->rootfs);

    char *envp[] = {
        "TERM=xterm-256color",
        "PATH=/bin:/sbin:/usr/bin:/usr/sbin:/server",
        "HOME=/root",
        "PS1=[container \\h]# ",
        NULL
    };
    char *argv[] = { a->cmd, NULL };
    execve(a->cmd, argv, envp);

    char *sh[] = { "/bin/sh", NULL };
    execve("/bin/sh", sh, envp);
    die("execve");
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <rootfs> <hostname> <netns> [cmd]\n"
            "  rootfs   – path to container root filesystem\n"
            "  hostname – hostname inside container\n"
            "  netns    – pre-created network namespace (ip netns add ...)\n"
            "  cmd      – command to run (default: /bin/sh)\n",
            argv[0]);
        return 1;
    }
    if (geteuid() != 0) {
        fprintf(stderr, "Must run as root\n");
        return 1;
    }

    ContainerArgs args = {
        .rootfs   = argv[1],
        .hostname = argv[2],
        .netns    = argv[3],
        .cmd      = (argc >= 5) ? argv[4] : "/bin/sh",
    };

    printf("=== Container Runtime ===\n");
    printf("  rootfs  : %s\n  hostname: %s\n  netns   : %s\n  cmd     : %s\n",
           args.rootfs, args.hostname, args.netns, args.cmd);

    int v1 = setup_cgroup(args.hostname);

    char *stack = malloc(STACK_SIZE);
    if (!stack) die("malloc");

    int flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | SIGCHLD;
    pid_t child = clone(child_main, stack + STACK_SIZE, flags, &args);
    if (child < 0) die("clone");

    add_to_cgroup(args.hostname, child, v1);

    int status;
    waitpid(child, &status, 0);
    cleanup_cgroup(args.hostname);
    free(stack);

    if (WIFEXITED(status))   return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 0;
}
