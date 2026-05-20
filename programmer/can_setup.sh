#!/bin/bash
# Fixed CAN setup script addressing Jetson timing issues
# Based on NVIDIA forum solutions for frame error problems

if [ "$(whoami)" != "root" ]; then
    echo "Please run as root"
    exit 1
fi

echo "=== Jetson CAN Setup - Fixed for Frame Errors ==="

echo "Configuring pinmux registers..."
sudo busybox devmem 0x0c303000 32 0x0000C400
sudo busybox devmem 0x0c303008 32 0x0000C458
sudo busybox devmem 0x0c303010 32 0x0000C400
sudo busybox devmem 0x0c303018 32 0x0000C458

echo "Reloading CAN modules..."
sudo rmmod mttcan 2>/dev/null || true
sudo rmmod can_raw 2>/dev/null || true
sudo rmmod can 2>/dev/null || true

sudo modprobe can
sudo modprobe can_raw
sudo modprobe mttcan

sudo ip link set down can0 2>/dev/null || true
sudo ip link set down can1 2>/dev/null || true

sudo ip link delete can0 2>/dev/null || true
sudo ip link delete can1 2>/dev/null || true

sleep 1

echo "Configuring CAN interfaces..."

echo "Setting up Classic CAN mode for mixed device network..."
sudo ip link set can0 type can bitrate 250000 sjw 4 \
    berr-reporting on fd off restart-ms 100 loopback off

sudo ip link set can1 type can bitrate 250000 sjw 4 \
    berr-reporting on fd off restart-ms 100 loopback off

sudo ip link set can0 txqueuelen 1000
sudo ip link set can1 txqueuelen 1000

echo "Bringing up CAN interfaces..."
sudo ip link set up can0
sudo ip link set up can1

if [ -w /proc/sys/net/core/rmem_max ] && [ -w /proc/sys/net/core/wmem_max ]; then
    echo "Configuring socket buffers..."
    echo 262144 > /proc/sys/net/core/rmem_max
    echo 262144 > /proc/sys/net/core/wmem_max
else
    echo "Warning: Cannot modify socket buffer limits"
fi

echo "=== CAN Configuration Complete ==="
echo ""
echo "Interface Status:"
ip -details link show can0
echo ""
ip -details link show can1
echo ""
