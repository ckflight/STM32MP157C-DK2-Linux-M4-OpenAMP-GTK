# STM32MP157C-DK2 Development Guide

This guide documents the development workflow used with the **STM32MP157C-DK2**, including:

* OpenSTLinux bring-up
* Ethernet / SSH connection
* Cortex-M4 development and debugging
* OpenAMP communication between Linux and M4
* IMU acquisition on M4
* IMU data transfer to Linux
* GTK application on Cortex-A7/Linux

## Project Architecture

```text
                         STM32MP157C-DK2
┌──────────────────────────────────────────────────────────┐
│                                                          │
│   Cortex-A7 / OpenSTLinux                                │
│   ┌──────────────────────────────────────────────────┐   │
│   │ GTK Application                                  │   │
│   │ Ethernet / SSH / USB / Filesystem                │   │
│   └──────────────────────────────────────────────────┘   │
│                         │                                │
│              remoteproc │ Load / Start / Stop M4         │
│                         ▼                                │
│   ┌──────────────────────────────────────────────────┐   │
│   │ Cortex-M4                                        │   │
│   │ IMU / I2C / GPIO / Real-Time Processing         │   │
│   └──────────────────────────────────────────────────┘   │
│                         ↕                                │
│                  OpenAMP / RPMsg                         │
│                Linux ↔ M4 Data                           │
│                                                          │
└──────────────────────────────────────────────────────────┘
                          ▲
                          │ SWD/JTAG Debug
                          │
                    PC ── ST-LINK
```

Project flow:

```text
OpenSTLinux Bring-Up → Ethernet / SSH → M4 Development
        → OpenAMP / RPMsg → IMU Data → GTK Application
```

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

---

# 2. Connect to the Board

After OpenSTLinux boots, access the **Linux terminal through the ST-LINK debug interface**:

```text
CN11 / ST-LINK (USB) → PC
```

Connect using:

```bash
ls /dev/ttyACM*
picocom -b 115200 /dev/ttyACM0
```

From the Linux terminal, find the Ethernet IP address:

```bash
ip addr
```

Example:

```text
end0 → 10.42.0.252
```

Once the IP is known, normal development can continue over **SSH**:

```bash
ssh root@10.42.0.252
```

```text
ST-LINK Serial → Linux Terminal → Find IP → SSH
```

## 2.1 Host PC Internet Sharing

To give the STM32MP157C-DK2 Internet access through the host PC, configure the host Ethernet connection in Ubuntu:

```text
Settings → Network → Wired → IPv4
IPv4 Method → Shared to other computers
```

The host PC shares its Internet connection with the board over Ethernet:

```text
Internet
   ↓
Host PC (Wi-Fi)
   ↓
Ethernet — Shared to other computers
   ↓
STM32MP157C-DK2 (end0)
```

Reconnect Ethernet and verify the connection from the board:

```bash
ip addr
ping 8.8.8.8
ping google.com
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

# 4. Cortex-M4 Project Configuration

Create the STM32MP157C-DK2 project in **STM32CubeMX** and configure it for OpenAMP.

In **Project Manager**:

```text
Application Structure → Basic
```

Use **Basic** structure to avoid unnecessary library/package dependency issues.

Then enable the required Cortex-M4 interrupts:

```text
NVIC:
IPCC RX1 occupied interrupt → Enable
IPCC TX1 free interrupt     → Enable
HSEM interrupt 2            → Enable
```

After enabling these interrupts, **IPCC** and **HSEM** become active and OpenAMP can be enabled:

```text
Middleware and Software Packs:
OPENAMP → M4 → Activated
Communication Mode → REMOTE
```

Keep the default OpenAMP shared-memory settings unless a custom memory layout is required.

<img width="2493" height="1408" alt="Image" src="https://github.com/user-attachments/assets/7a4d40ec-2790-4702-a54b-89026429df6e" />
<img width="2496" height="1404" alt="Image" src="https://github.com/user-attachments/assets/f555d448-d8fd-49ae-994d-ba2e702bf308" />
<img width="2496" height="1404" alt="Image" src="https://github.com/user-attachments/assets/377bb00a-b909-4589-8da0-6296ab03f2bd" />