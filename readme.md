# STM32MP157C-DK2 Development Guide

This guide documents the development workflow used with the **STM32MP157C-DK2**, including:

* OpenSTLinux bring-up
* Ethernet / SSH connection
* Cortex-M4 development and debugging
* OpenAMP communication between Linux and M4
* IMU acquisition on M4
* IMU data transfer to Linux
* GTK application on Cortex-A7/Linux

---

# 1. Board Bring-Up

## 1.1 Download OpenSTLinux

Download the **STM32MP1 Starter Package** for STM32MP157C-DK2 from ST:

```text
https://www.st.com/en/embedded-software/stm32mp1starter.html
```

Extract the downloaded package. It contains the OpenSTLinux images and flash layouts required for the DK2.

## 1.2 Flash the SD Card

Install **STM32CubeProgrammer**:

```text
https://www.st.com/en/development-tools/stm32cubeprog.html
```

Set `SW1` for **USB DFU mode**:

```text
BOOT0 → OFF
BOOT2 → OFF
```

Connect the board:

```text
CN7 / USB_OTG (USB Type-C) → PC (DFU)
CN6 / USB_PWR (USB Type-C) → 5V Power
```

Insert the microSD card, power the board and press **RESET**.

Open **STM32CubeProgrammer**:

```text
USB → Refresh → Connect
```

The board should now be detected in **DFU mode**.

Locate the **OP-TEE SD-card flash layout**:

```text
STM32MP1 Starter Package/
└── images/
    └── stm32mp1/
        └── flashlayout_st-image-weston/
            └── optee/
                └── FlashLayout_sdcard_stm32mp157c-dk2-optee.tsv
```

In **STM32CubeProgrammer**:

```text
1. Select → Download
2. Open → FlashLayout_sdcard_stm32mp157c-dk2-optee.tsv
3. Binaries path → .../images/stm32mp1/
4. Verify all partitions are selected
5. Click → Download
```

STM32CubeProgrammer will automatically program the required **FSBL, FIP, OP-TEE and OpenSTLinux filesystem partitions** to the SD card.

After programming completes, set `SW1` for **SD-card boot**:

```text
BOOT0 → ON
BOOT2 → ON
```

Power-cycle the board. **OpenSTLinux** should now boot from the SD card.

<img width="2496" height="1404" alt="Image" src="https://github.com/user-attachments/assets/8d579a3c-617c-4aa2-a2e3-4676b941c16f" />

---

# 2. Connect to the Board

After OpenSTLinux boots, first access the **Linux terminal through ST-LINK serial**:

```text
CN11 / ST-LINK (USB) → PC
```

Find the serial port and connect using **picocom**:

```bash
ls /dev/ttyACM*
picocom -b 115200 /dev/ttyACM0
```

Connect the Ethernet cable and check the assigned IP:

```bash
ip addr
```

The Ethernet interface is typically `end0`. Look for the `inet` address:

```text
end0 → inet 10.42.0.252/24
```

Then connect from the host PC using SSH:

```bash
ssh root@10.42.0.252
```

In general:

```text
Initial access → CN11 / ST-LINK Serial (115200 baud)
Normal access  → Ethernet (end0) / SSH
```

## 2.1 Host PC Internet Sharing

To give the STM32MP157C-DK2 Internet access through the host PC, configure the host Ethernet connection in Ubuntu:

```text
Settings → Network → Wired → IPv4
IPv4 Method → Shared to other computers
```

Reconnect Ethernet. The host PC will provide the board with an IP address and route its Internet traffic through the PC.

Check from the STM32MP157C-DK2:

```bash
ip addr
ping 8.8.8.8
ping google.com
```

Typical setup:

```text
Internet
   ↓
Host PC (Wi-Fi)
   ↓
Ethernet — Shared to other computers
   ↓
STM32MP157C-DK2 (end0)
```

If `end0` receives an address such as `10.42.0.x` and the ping succeeds, the board has Internet access through the host PC.

---

# 3. Cortex-M4 Development

Install:

```text
STM32CubeIDE
STM32CubeMX
STM32CubeMP1
```

The **Cortex-M4** is used for real-time and hardware-level tasks, while the Cortex-A7 cores run OpenSTLinux.

```text
Real-time / hardware tasks → Cortex-M4
GUI / Ethernet / USB       → Cortex-A7 + Linux
```

Cortex-M4 development uses three main mechanisms:

```text
ST-LINK        → SWD/JTAG debug (breakpoints, step, registers)
remoteproc     → Linux loads / starts / stops the M4 firmware
OpenAMP/RPMsg  → Linux ↔ M4 data communication
```

`remoteproc` is **not a debugger**; it is the Linux framework used to manage the Cortex-M4.

Typical development architecture:

```text
PC ── ST-LINK ─────────────→ Cortex-M4 (Debug)

             STM32MP157
┌───────────────────────────────┐
│ Cortex-A7 / OpenSTLinux       │
│        │                      │
│    remoteproc                 │
│        │                      │
│        ▼                      │
│ Cortex-M4                     │
│        ↕                      │
│   OpenAMP / RPMsg             │
└───────────────────────────────┘
```

---

# 4. Development Flow

The project is developed in the following order:

```text
OpenSTLinux
     ↓
Ethernet / SSH
     ↓
Cortex-M4 Debug
     ↓
OpenAMP / RPMsg
     ↓
IMU on Cortex-M4
     ↓
M4 → Linux IMU Data
     ↓
GTK Application
```

The following sections document the Cortex-M4 configuration, OpenAMP communication and application development in detail.
