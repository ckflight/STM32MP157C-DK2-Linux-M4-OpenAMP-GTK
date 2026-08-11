# STM32MP157C-DK2 Development Guide

This guide covers the complete **OpenSTLinux + Cortex-M4** development workflow on the STM32MP157C-DK2.

## Project Architecture

```text id="project-architecture"
                         STM32MP157C-DK2
┌──────────────────────────────────────────────────────────┐
│ Cortex-A7 / OpenSTLinux                                  │
│   GTK / Ethernet / USB / Filesystem                      │
│                         │                                │
│              remoteproc │ Load / Start / Stop M4         │
│                         ▼                                │
│ Cortex-M4                                                │
│   IMU / I2C / GPIO / Real-Time Processing                │
│                         ↕                                │
│                  OpenAMP / RPMsg                         │
│                    Linux ↔ M4                            │
└──────────────────────────────────────────────────────────┘
                          ▲
                          │ SWD/JTAG Debug
                    PC ── ST-LINK
```

```text id="project-components"
ST-LINK        → Cortex-M4 debugging
remoteproc     → Linux manages M4 firmware
OpenAMP/RPMsg  → Linux ↔ M4 communication
```

Project flow:

```text id="project-flow"
OpenSTLinux → Ethernet / SSH → M4 Firmware
→ OpenAMP / RPMsg → IMU Data → GTK Application
```

<img width="800" height="480" alt="Image" src="https://github.com/user-attachments/assets/bcba5922-928d-4d9d-a017-eac9be4a74ce" />

---

# 1. Board Bring-Up

## 1.1 Download OpenSTLinux

Download the **STM32MP1 Starter Package** for STM32MP157C-DK2 from ST:

```text id="starter-package"
https://www.st.com/en/embedded-software/stm32mp1starter.html
```

Extract the package. It contains the OpenSTLinux images and flash layouts required for the DK2.

## 1.2 Flash the SD Card

Install **STM32CubeProgrammer**:

```text id="cubeprogrammer"
https://www.st.com/en/development-tools/stm32cubeprog.html
```

Set `SW1` for **USB DFU mode**:

```text id="dfu-switches"
BOOT0 → OFF
BOOT2 → OFF
```

Connect the board:

```text id="dfu-connections"
CN7 / USB_OTG (USB Type-C) → PC (DFU)
CN6 / USB_PWR (USB Type-C) → 5V Power
```

Insert the microSD card, power the board and press **RESET**.

Open **STM32CubeProgrammer**:

```text id="dfu-connect"
USB → Refresh → Connect
```

Locate the **OP-TEE SD-card flash layout**:

```text id="optee-layout"
STM32MP1 Starter Package/
└── images/
    └── stm32mp1/
        └── flashlayout_st-image-weston/
            └── optee/
                └── FlashLayout_sdcard_stm32mp157c-dk2-optee.tsv
```

In **STM32CubeProgrammer**:

```text id="programmer-steps"
1. Select → Download
2. Open → FlashLayout_sdcard_stm32mp157c-dk2-optee.tsv
3. Binaries path → .../images/stm32mp1/
4. Verify all partitions are selected
5. Click → Download
```

After programming completes, set `SW1` for **SD-card boot**:

```text id="sd-boot-switches"
BOOT0 → ON
BOOT2 → ON
```

Power-cycle the board. **OpenSTLinux** should now boot from the SD card.

---

# 2. Connect to the Board

After OpenSTLinux boots, access the Linux terminal through the **ST-LINK serial interface**:

```text id="stlink-serial"
CN11 / ST-LINK (USB) → PC
```

Connect using:

```bash id="picocom"
ls /dev/ttyACM*
picocom -b 115200 /dev/ttyACM0
```

Find the Ethernet IP:

```bash id="ip-address"
ip addr
```

Example:

```text id="ip-example"
end0 → 10.42.0.252
```

Once the IP is known, continue development over **SSH**:

```bash id="ssh"
ssh root@10.42.0.252
```

```text id="connection-flow"
ST-LINK Serial → Linux Terminal → Find IP → SSH
```

## 2.1 Host PC Internet Sharing

To provide Internet access through the Ubuntu host PC:

```text id="internet-sharing"
Settings → Network → Wired → IPv4
IPv4 Method → Shared to other computers
```

Network path:

```text id="internet-path"
Internet
   ↓
Host PC (Wi-Fi)
   ↓
Shared Ethernet
   ↓
STM32MP157C-DK2 (end0)
```

Verify from the board:

```bash id="internet-test"
ping 8.8.8.8
ping google.com
```

---

# 3. Cortex-M4 CubeMX Project Initialization

Create the STM32MP157C-DK2 project in **STM32CubeMX**.

In **Project Manager**:

```text id="basic-structure"
Application Structure → Basic
```

Use **Basic** structure to avoid unnecessary library/package dependency issues.

Enable the required Cortex-M4 interrupts:

```text id="openamp-interrupts"
NVIC:
IPCC RX1 occupied interrupt → Enable
IPCC TX1 free interrupt     → Enable
HSEM interrupt 2            → Enable
```

After enabling these interrupts, **IPCC** and **HSEM** become active and OpenAMP can be enabled:

```text id="openamp-enable"
Middleware and Software Packs:
OPENAMP → M4 → Activated
Communication Mode → REMOTE
```

Keep the default OpenAMP shared-memory settings unless a custom memory layout is required.

<img width="2493" height="1408" alt="Image" src="https://github.com/user-attachments/assets/7a4d40ec-2790-4702-a54b-89026429df6e" />
<img width="2496" height="1404" alt="Image" src="https://github.com/user-attachments/assets/f555d448-d8fd-49ae-994d-ba2e702bf308" />
<img width="2496" height="1404" alt="Image" src="https://github.com/user-attachments/assets/377bb00a-b909-4589-8da0-6296ab03f2bd" />

---

# 4. Cortex-M4 Debugging

Open the Cortex-M4 **Debug Configuration** in STM32CubeIDE:

```text id="debug-settings"
Debugger:
Load Mode     → thru Linux core (Production mode)
Inet Address  → <BOARD_IP>
Debug probe   → ST-LINK (OpenOCD)
```

Example:

```text id="debug-ip"
Inet Address → 10.42.0.252
```

In **Production Mode**:

```text id="production-debug"
CubeIDE
   │ Ethernet / SSH
   ▼
OpenSTLinux → /usr/local/projects/<ProjectName>/
   │
   │ remoteproc
   ▼
Cortex-M4
```

```text id="debug-components"
Ethernet   → Transfer M4 firmware to Linux
remoteproc → Load / start M4 firmware
ST-LINK    → Debug Cortex-M4
```
<img width="2035" height="1323" alt="Image" src="https://github.com/user-attachments/assets/50e624b1-8aa5-4552-9254-e7744ef913f3" />

# 5. GTK Linux Application

The GTK application runs on the **Cortex-A7 / OpenSTLinux** side and displays the IMU data received from the Cortex-M4 through OpenAMP/RPMsg.

The application source is located in:

```text
GTK_App/
├── main.c
└── build.sh
```

The `build.sh` script uses the **STM32MP1 cross-compilation SDK** to build the GTK application and automatically copies the executable to the board over SSH.

Make the script executable and build from the VS Code terminal:

```bash
cd GTK_App
chmod +x build.sh
./build.sh
```

The executable is copied to:

```text
/usr/local/projects/STM32MP157CAC3_GTK_IMU_Monitor_App
```

Connect to the board and run it:

```bash
ssh root@10.42.0.252
cd /usr/local/projects/STM32MP157CAC3_GTK_IMU_Monitor_App
./STM32MP157CAC3_GTK_IMU_Monitor_App
```

```text
main.c → build.sh → Cross-Compile → SCP → OpenSTLinux → Run GTK App
```
