<div align="center">

```
███╗   ███╗██╗███╗   ██╗██╗    ██████╗  ██████╗  ██████╗██╗  ██╗███████╗██████╗
████╗ ████║██║████╗  ██║██║    ██╔══██╗██╔═══██╗██╔════╝██║ ██╔╝██╔════╝██╔══██╗
██╔████╔██║██║██╔██╗ ██║██║    ██║  ██║██║   ██║██║     █████╔╝ █████╗  ██████╔╝
██║╚██╔╝██║██║██║╚██╗██║██║    ██║  ██║██║   ██║██║     ██╔═██╗ ██╔══╝  ██╔══██╗
██║ ╚═╝ ██║██║██║ ╚████║██║    ██████╔╝╚██████╔╝╚██████╗██║  ██╗███████╗██║  ██║
╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═╝    ╚═════╝  ╚═════╝  ╚═════╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
```

### 🐳 A Container Runtime Built From Scratch in C — No Docker. No LXC. Just Linux.

<br/>

[![Language](https://img.shields.io/badge/Language-C-00599C?style=for-the-badge&logo=c&logoColor=white)]()
[![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)]()
[![Kernel](https://img.shields.io/badge/Kernel-Namespaces%20%2B%20Cgroups-FF6B35?style=for-the-badge)]()
[![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)]()
[![Status](https://img.shields.io/badge/Status-Working-brightgreen?style=for-the-badge)]()

<br/>

> **"What Docker does under the hood — implemented from first principles."**

<br/>

[🚀 Quick Start](#-quick-start) • [📐 Architecture](#-architecture) • [🔬 How It Works](#-how-it-works) • [🎬 Demo](#-demo-walkthrough) • [📁 Project Structure](#-project-structure)

</div>

---

## 🧠 What Is This?

**mini-docker** is a fully functional **Linux container runtime written in C** that creates isolated environments using raw Linux kernel features — the same primitives that power Docker, Podman, and LXC.

No existing container runtime is used. Every isolation boundary is built by directly invoking Linux kernel APIs.

```
Your Code (container.c)
      │
      ▼
  clone() ──────────────────────────────────────────────────┐
      │                                                       │
      ▼                                              CHILD PROCESS
  waitpid()                                    ┌─────────────────────┐
      │                                        │  New PID Namespace  │
      ▼                                        │  New Mount NS       │
  cleanup_cgroup()                             │  New UTS Namespace  │
                                               │  Joined Net NS      │
                                               │  pivot_root()       │
                                               │  /proc mounted      │
                                               │  cgroup enforced    │
                                               │  /bin/sh spawned ✓  │
                                               └─────────────────────┘
```

---

## ✨ Features

| Feature | Implementation | Kernel Mechanism |
|---|---|---|
| 🔒 **Process Isolation** | PID namespace — container sees only its own processes | `clone(CLONE_NEWPID)` |
| 📁 **Filesystem Isolation** | Custom rootfs via pivot_root — completely separate file tree | `clone(CLONE_NEWNS)` + `pivot_root()` |
| 🏷️ **Hostname Isolation** | Each container has its own hostname | `clone(CLONE_NEWUTS)` + `sethostname()` |
| 🌐 **Network Isolation** | Separate IP address per container via veth pairs | `ip netns` + `setns()` |
| 💾 **Memory Limits** | Hard 128 MiB cap — process killed on violation | Linux cgroups v1/v2 |
| 🔌 **Network Connectivity** | Host ↔ Container communication via virtual ethernet | `veth` pair + routing |
| 🚢 **Multi-Container** | Run N containers simultaneously, fully isolated | Multiple namespaces |
| ⚡ **Same Port, Two Containers** | Both containers bind port 8080 — no conflict | Network namespace isolation |

---

## 📐 Architecture

### Namespace Isolation Model

```
┌─────────────────────────────────────────────────────────────┐
│                        HOST MACHINE                          │
│                                                             │
│  PID 1=systemd   eth0=192.168.1.x   hostname=ubuntu        │
│                                                             │
│   ┌─────────────────────┐   ┌─────────────────────┐        │
│   │    CONTAINER 1      │   │    CONTAINER 2      │        │
│   │                     │   │                     │        │
│   │  PID 1 = /bin/sh    │   │  PID 1 = /bin/sh    │        │
│   │  PID 2 = server     │   │  PID 2 = server     │        │
│   │                     │   │                     │        │
│   │  IP: 10.200.1.2     │   │  IP: 10.200.2.2     │        │
│   │  hostname: cont1    │   │  hostname: cont2    │        │
│   │  port 8080 ✓        │   │  port 8080 ✓        │        │
│   │  mem limit: 128MB   │   │  mem limit: 128MB   │        │
│   │                     │   │                     │        │
│   │  rootfs1/           │   │  rootfs2/           │        │
│   └─────────────────────┘   └─────────────────────┘        │
│          │                         │                        │
│        veth1 (10.200.1.1)        veth2 (10.200.2.1)        │
└─────────────────────────────────────────────────────────────┘
```

### Network Topology

```
HOST                    CONTAINER 1              CONTAINER 2
10.200.1.1 ←──veth──→  10.200.1.2
10.200.2.1 ←──veth──→                           10.200.2.2

Client on host can reach both containers at their respective IPs.
Both containers are isolated from each other.
```

---

## 🔬 How It Works

### 1. `clone()` — The Heart of Isolation

```c
int flags = CLONE_NEWPID   // ← New PID namespace
          | CLONE_NEWNS    // ← New Mount namespace
          | CLONE_NEWUTS   // ← New UTS (hostname) namespace
          | SIGCHLD;

pid_t child = clone(child_main, stack_top, flags, &args);
```

A single `clone()` call creates a child process that lives in **completely separate namespaces** from its parent.

### 2. `pivot_root()` — Switching the Filesystem

```c
// Bind-mount rootfs onto itself (pivot_root requirement)
mount(rootfs, rootfs, "bind", MS_BIND | MS_REC, NULL);

// Switch the root — old root moves to .pivot_old/
syscall(SYS_pivot_root, rootfs, put_old);

// Detach the old root entirely
umount2("/.pivot_old", MNT_DETACH);
```

Unlike `chroot`, `pivot_root` swaps the **entire mount namespace root** — providing much stronger isolation.

### 3. `setns()` — Joining a Network Namespace

```c
// Network namespace created beforehand: ip netns add container_ns1
int fd = open("/var/run/netns/container_ns1", O_RDONLY);
setns(fd, CLONE_NEWNET);  // ← Join it!
```

### 4. cgroups — Memory Enforcement

```c
// Create a cgroup for this container
mkdir("/sys/fs/cgroup/memory/container_mycontainer", 0755);

// Set 128 MiB hard limit
write_file("memory.limit_in_bytes", "134217728");

// Add container process to this cgroup
write_file("cgroup.procs", child_pid);

// Now if container exceeds 128MB → OOM Kill!
```

---

## 🚀 Quick Start

### Prerequisites

```bash
sudo apt-get install -y gcc make iproute2 busybox-static
```

### Build

```bash
git clone https://github.com/Prateek13052003/mini-docker.git
cd mini-docker
make all
```

This produces:
- `./container` — the runtime binary
- `./client` — host-side test client
- `rootfs/server/server` — static TCP echo server (goes inside container)
- `rootfs/bin/mem_hog` — memory limit demo binary
- `rootfs/bin/cpu_hog` — CPU burn demo binary

### One-Time Setup

```bash
sudo bash demo/setup.sh
```

This creates:
- Two root filesystems (`rootfs1/`, `rootfs2/`)
- Two network namespaces (`container_ns1`, `container_ns2`)
- veth pairs connecting host ↔ each container

---

## 🎬 Demo Walkthrough

### Start Container 1

```bash
# Terminal 1
sudo ./container rootfs1 mycontainer1 container_ns1
```

```
=== Container Runtime ===
  rootfs  : rootfs1
  hostname: mycontainer1
  netns   : container_ns1
[cgroup v1] memory limit = 128 MiB  (container_mycontainer1)
[net] joined network namespace: container_ns1
[container mycontainer1]#
```

---

### Demo 1 — PID Isolation

```bash
# Inside container:
ps aux
```
```
PID   USER   COMMAND
  1   root   /bin/sh        ← Container thinks it's PID 1!
  2   root   ps aux
```
> Host has 200+ processes. Container sees only 2. ✅

---

### Demo 2 — Hostname Isolation

```bash
# On host:
hostname          # → ubuntu-machine

# Inside container:
hostname          # → mycontainer1
```

---

### Demo 3 — Network: Host talks to Container

```bash
# Inside container (Terminal 1):
/server/server 8080
# [server] listening on port 8080...

# On host (Terminal 2):
./client 10.200.1.2 8080
# [client] connected to 10.200.1.2:8080
# [client] echo received: Hello from host namespace!
```

---

### Demo 4 — Same Port, Two Containers 🤯

```bash
# Container 1:
/server/server 8080 &   # ← listening on 8080

# Container 2 (SAME PORT — no error!):
/server/server 8080 &   # ← also listening on 8080 ✓

# Host:
./client 10.200.1.2 8080   # talks to Container 1
./client 10.200.2.2 8080   # talks to Container 2
```

> This is **impossible** on a regular Linux shell. Only possible with network namespace isolation. ✅

---

### Demo 5 — Memory Cgroup Limit

```bash
# Inside container:
/bin/mem_hog
```
```
[mem_hog] Allocating memory in 10 MiB chunks...
[mem_hog] Allocated   10 MiB so far...
[mem_hog] Allocated   20 MiB so far...
...
[mem_hog] Allocated  120 MiB so far...
Killed                      ← OOM killed by cgroup at 128 MiB ✅
```

---

### Demo 6 — Cross-Container Process Isolation

```bash
# Container 1:
sleep 9999 &
ps aux         # shows: sh, sleep, ps

# Container 2:
ps aux         # sleep 9999 is INVISIBLE ✅
```

---

## 📁 Project Structure

```
mini-docker/
│
├── src/
│   └── container.c        ← Main runtime (clone, pivot_root, cgroups, setns)
│
├── server/
│   ├── server.c           ← Static TCP echo server (runs inside container)
│   └── client.c           ← TCP client (runs on host)
│
├── demo/
│   ├── mem_hog.c          ← Allocates RAM until OOM-killed (cgroup demo)
│   ├── cpu_hog.c          ← Burns CPU (resource limit demo)
│   ├── setup.sh           ← Network namespace + rootfs setup
│   └── demo.sh            ← Automated full demo script
│
├── rootfs/                ← Template root filesystem
│   ├── bin/               ← Statically compiled binaries
│   └── server/            ← Server binary
│
└── Makefile
```

---

## 🛠️ System Calls Used

| Syscall | Purpose |
|---|---|
| `clone()` | Create child process in new namespaces |
| `pivot_root()` | Switch root filesystem of mount namespace |
| `setns()` | Join an existing network namespace |
| `sethostname()` | Set hostname in UTS namespace |
| `mount()` | Mount proc, sysfs, devtmpfs, tmpfs inside container |
| `umount2()` | Detach old root after pivot |
| `unshare()` | (Used by setup scripts) |

---

## 📚 Concepts Demonstrated

- **Linux Namespaces** — PID, Mount, UTS, Network
- **Control Groups (cgroups)** — v1 and v2 memory enforcement
- **Virtual Ethernet (veth)** — Container networking
- **pivot_root** — Secure filesystem switching
- **Static Binaries** — Self-contained executables for minimal rootfs
- **Process Isolation** — Containers unaware of each other

---

## 🔒 Security Note

This project is for **educational purposes**. It must be run as root and is not hardened for production. Real container runtimes (like `runc`) add seccomp filters, capability dropping, user namespace remapping, and more.

---

## 👨‍💻 Author

**Prateek Choudhary**

> Built as part of a Systems Programming course — implementing Linux container internals from scratch without any existing container runtime.

---

<div align="center">

**If this helped you understand containers, give it a ⭐**

*"The best way to understand a system is to build it yourself."*

</div>
