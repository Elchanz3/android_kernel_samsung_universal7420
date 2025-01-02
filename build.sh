#!/bin/bash

ARCH=arm64
KERNEL_SOURCE_DIR="$(pwd)"
OUTPUT_DIR="$(pwd)/kout"
CONFIG_FILE="$(pwd)/arch/arm64/configs/exynos7420-zeroflte_defconfig"
CROSS_COMPILE="/home/chanz22/tc/aarch64-linux-android-4.9/bin/aarch64-linux-android-" 
JOBS=$(nproc)

if [ ! -d "$KERNEL_SOURCE_DIR" ]; then
    echo "Error: Kernel source not found in $KERNEL_SOURCE_DIR"
    exit 1
fi

cd "$KERNEL_SOURCE_DIR" || exit

make mrproper

if [ -f "$CONFIG_FILE" ]; then
    cp "$CONFIG_FILE" .config
else
    exit 1
fi

make -j"$JOBS" O="$OUTPUT_DIR" CROSS_COMPILE="$CROSS_COMPILE" Image modules dtbs

if [ $? -ne 0 ]; then
    echo "Error: kernel stuck in compilation!"
    exit 1
fi

echo "done"
