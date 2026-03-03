#!/bin/bash
# setup.sh — Prepares rootfs and network namespaces for two containers
# Run ONCE as root before starting containers:
#   sudo bash demo/setup.sh

set -e

ROOTFS1="$(pwd)/rootfs1"
ROOTFS2="$(pwd)/rootfs2"
NS1="container_ns1"
NS2="container_ns2"

# ── colours ──────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; NC='\033[0m'
info()  { echo -e "${GREEN}[setup]${NC} $*"; }
error() { echo -e "${RED}[error]${NC} $*"; exit 1; }

[ "$(id -u)" -eq 0 ] || error "Must be run as root"

# ────────────────────────────────────────────────────────────
# 1. Build rootfs from busybox static binary
# ────────────────────────────────────────────────────────────
build_rootfs() {
    local ROOT="$1"
    info "Building rootfs at $ROOT ..."

    # Check if busybox is available
    BUSYBOX=$(which busybox 2>/dev/null || true)
    if [ -z "$BUSYBOX" ]; then
        # Try to download static busybox
        info "Downloading static busybox..."
        BUSYBOX="/tmp/busybox-static"
        wget -q "https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox" \
             -O "$BUSYBOX" || \
        apt-get install -y busybox-static 2>/dev/null && BUSYBOX=$(which busybox)
        chmod +x "$BUSYBOX" 2>/dev/null || true
    fi

    # Create directory structure
    mkdir -p "$ROOT"/{bin,sbin,usr/bin,usr/sbin,lib,lib64,proc,sys,dev,tmp,etc,root,server}

    # Install busybox and create applets
    if [ -f "$BUSYBOX" ]; then
        cp "$BUSYBOX" "$ROOT/bin/busybox"
        chroot "$ROOT" /bin/busybox --install -s /bin 2>/dev/null || \
            (cd "$ROOT/bin" && for app in sh ls ps hostname cat echo mkdir rm; do
                ln -sf busybox "$app" 2>/dev/null; done)
    else
        # Copy binaries from host with their libraries
        info "Copying host binaries into rootfs..."
        for bin in sh bash ls ps cat echo hostname mkdir sleep; do
            BIN_PATH=$(which "$bin" 2>/dev/null) || continue
            cp "$BIN_PATH" "$ROOT/bin/"
            # Copy required shared libraries
            ldd "$BIN_PATH" 2>/dev/null | grep -oP '/\S+\.so\S*' | while read lib; do
                [ -f "$lib" ] && cp --parents "$lib" "$ROOT/" 2>/dev/null
            done
        done
    fi

    # etc/passwd and etc/hostname
    echo "root:x:0:0:root:/root:/bin/sh" > "$ROOT/etc/passwd"
    echo "container" > "$ROOT/etc/hostname"

    info "rootfs built at $ROOT"
}

# ────────────────────────────────────────────────────────────
# 2. Set up network namespaces and veth pairs
# ────────────────────────────────────────────────────────────
setup_network() {
    info "Setting up network namespaces..."

    # Clean up any existing setup
    ip netns del "$NS1" 2>/dev/null || true
    ip netns del "$NS2" 2>/dev/null || true
    ip link del veth1 2>/dev/null || true
    ip link del veth2 2>/dev/null || true

    # Create namespaces
    ip netns add "$NS1"
    ip netns add "$NS2"

    # ── Container 1 networking ──────────────────────────────
    # Create veth pair: veth1 (host) <-> veth1_c (container1)
    ip link add veth1   type veth peer name veth1_c
    ip link set veth1_c netns "$NS1"

    ip addr add 10.200.1.1/24 dev veth1
    ip link set veth1 up

    ip netns exec "$NS1" ip addr add 10.200.1.2/24 dev veth1_c
    ip netns exec "$NS1" ip link set veth1_c up
    ip netns exec "$NS1" ip link set lo up
    ip netns exec "$NS1" ip route add default via 10.200.1.1

    # ── Container 2 networking ──────────────────────────────
    ip link add veth2   type veth peer name veth2_c
    ip link set veth2_c netns "$NS2"

    ip addr add 10.200.2.1/24 dev veth2
    ip link set veth2 up

    ip netns exec "$NS2" ip addr add 10.200.2.2/24 dev veth2_c
    ip netns exec "$NS2" ip link set veth2_c up
    ip netns exec "$NS2" ip link set lo up
    ip netns exec "$NS2" ip route add default via 10.200.2.1

    # Enable IP forwarding on host
    echo 1 > /proc/sys/net/ipv4/ip_forward

    info "Network setup complete"
    info "  Host -> Container1: 10.200.1.1 <-> 10.200.1.2  (netns: $NS1)"
    info "  Host -> Container2: 10.200.2.1 <-> 10.200.2.2  (netns: $NS2)"
}

# ────────────────────────────────────────────────────────────
# 3. Copy compiled binaries into rootfs
# ────────────────────────────────────────────────────────────
install_binaries() {
    local ROOT="$1"
    info "Installing custom binaries into $ROOT ..."

    # Server (must be static)
    [ -f rootfs/server/server ] && cp rootfs/server/server "$ROOT/server/server"
    # Demo binaries
    [ -f rootfs/bin/mem_hog ]   && cp rootfs/bin/mem_hog   "$ROOT/bin/mem_hog"
    [ -f rootfs/bin/cpu_hog ]   && cp rootfs/bin/cpu_hog   "$ROOT/bin/cpu_hog"

    chmod +x "$ROOT"/server/* "$ROOT"/bin/* 2>/dev/null || true
}

# ────────────────────────────────────────────────────────────
# MAIN
# ────────────────────────────────────────────────────────────
build_rootfs "$ROOTFS1"
build_rootfs "$ROOTFS2"
install_binaries "$ROOTFS1"
install_binaries "$ROOTFS2"
setup_network

info "==================================================="
info "Setup complete! Start containers with:"
info ""
info "  Terminal 1 (Container 1):"
info "  sudo ./container $ROOTFS1 mycontainer1 $NS1"
info ""
info "  Terminal 2 (Container 2):"
info "  sudo ./container $ROOTFS2 mycontainer2 $NS2"
info ""
info "  Then test networking from the host:"
info "  ./client 10.200.1.2 8080"
info "==================================================="
