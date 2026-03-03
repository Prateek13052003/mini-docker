#!/bin/bash
# demo.sh — Automated demonstration of all container features
# Must be run AFTER setup.sh, and containers must be running
# Usage: sudo bash demo/demo.sh

set -e
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
step() { echo -e "\n${YELLOW}━━━ DEMO: $* ━━━${NC}"; }
info() { echo -e "${GREEN}>>>${NC} $*"; }

ROOTFS1="$(pwd)/rootfs1"
ROOTFS2="$(pwd)/rootfs2"
NS1="container_ns1"
NS2="container_ns2"

# ── DEMO 1: Namespace isolation (PID) ────────────────────────
step "PID Namespace Isolation"
info "Running ps inside container1 — should show only a few processes (not host's)"
sudo ./container "$ROOTFS1" demo1 "$NS1" /bin/ps &
sleep 2

# ── DEMO 2: Hostname isolation (UTS) ─────────────────────────
step "Hostname / UTS Isolation"
info "Container hostname vs host hostname:"
echo "Host hostname: $(hostname)"
sudo ./container "$ROOTFS1" my-container "$NS1" /bin/hostname &
sleep 2

# ── DEMO 3: Network connectivity ─────────────────────────────
step "Network Connectivity (veth pair)"
info "Starting server inside container1 (background)..."
sudo ./container "$ROOTFS1" net-test "$NS1" /server/server &
SERVER_PID=$!
sleep 2

info "Connecting from host to container IP 10.200.1.2:8080..."
./client 10.200.1.2 8080
kill $SERVER_PID 2>/dev/null; wait $SERVER_PID 2>/dev/null

# ── DEMO 4: Same port, two containers ────────────────────────
step "Two Containers: Same Port, Isolated"
info "Starting server on port 8080 in BOTH containers simultaneously..."
sudo ./container "$ROOTFS1" c1 "$NS1" /server/server &
C1_PID=$!
sudo ./container "$ROOTFS2" c2 "$NS2" /server/server &
C2_PID=$!
sleep 2

info "Connecting to container 1 (10.200.1.2:8080)..."
./client 10.200.1.2 8080
info "Connecting to container 2 (10.200.2.2:8080)..."
./client 10.200.2.2 8080

kill $C1_PID $C2_PID 2>/dev/null
wait $C1_PID $C2_PID 2>/dev/null

# ── DEMO 5: Memory cgroup limit ──────────────────────────────
step "Memory Cgroup Limit (128 MiB)"
info "Running mem_hog inside container — it will be OOM-killed at ~128 MiB"
sudo ./container "$ROOTFS1" memtest "$NS1" /bin/mem_hog || true

echo -e "\n${GREEN}All demos complete!${NC}"
