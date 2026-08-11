#!/bin/bash

set -e

unset LD_LIBRARY_PATH

source /opt/st/stm32mp1/5.0.17-openstlinux-6.6-yocto-scarthgap-mpu-v26.06.10/environment-setup-cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi

echo "Building..."
$CC main.c -o STM32MP157CAC3_GTK_IMU_Monitor_App $(pkg-config --cflags --libs gtk+-3.0)

echo "Checking binary..."
file STM32MP157CAC3_GTK_IMU_Monitor_App

echo "Copying to STM32MP1..."
scp STM32MP157CAC3_GTK_IMU_Monitor_App root@10.42.0.252:/usr/local/projects/STM32MP157CAC3_GTK_IMU_Monitor_App

echo "Done."