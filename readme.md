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

```text id="gvzhrq"
https://www.st.com/en/embedded-software/stm32mp1starter.html
```

Extract the downloaded package.

The package contains the OpenSTLinux image and the flash layout required for the DK2.

---

## 1.2 Flash the SD Card

Install **STM32CubeProgrammer**:

```text id="skys01"
https://www.st.com/en/development-tools/stm32cubeprog.html
```

To start the board in **USB DFU mode**, configure the `SW1` boot switches as:

```text id="bj2n9v"
BOOT0 → OFF
BOOT2 → OFF
```

This selects the forced USB boot / DFU mode.

Connect the PC to:

```text id="kguz1p"
CN7 / USB_OTG (USB Type-C)
```

Power the board and press **RESET**.

Open **STM32CubeProgrammer** and select:

```text id="6lv6kl"
Connection → USB
Refresh
Connect
```

The board should now be detected in **DFU mode**.

Select the DK2 SD-card flash layout:

```text id="o81d79"
FlashLayout_sdcard_stm32mp157c-dk2-optee.tsv
```

Set the binaries path to the extracted:

```text id="5pb8fy"
images/stm32mp1/
```

and click **Download**.

After programming is complete, configure the boot switches for normal SD-card boot:

```text id="cymoc7"
BOOT0 → ON
BOOT2 → ON
```

Power-cycle or reset the board. **OpenSTLinux** should now boot from the SD card.

---

# 2. Connect to the Board

After OpenSTLinux boots, the board can be accessed either through the **serial console** or **Ethernet/SSH**.

For serial console through ST-LINK:

```bash id="am0op5"
ls /dev/ttyACM*
minicom -D /dev/ttyACM0
```

For Ethernet, connect the cable and check the board IP:

```bash id="xewhgm"
ip addr
```

Then connect from the host PC:

```bash id="uhsbnc"
ssh root@<BOARD_IP>
```

Example:

```bash id="n4xd2m"
ssh root@192.168.1.100
```

Use the serial console mainly for initial bring-up and debugging. For normal development, SSH over Ethernet is more convenient.

---

# 3. Cortex-M4 Development

Install:

```text id="3mrg0u"
STM32CubeIDE
STM32CubeMX
STM32CubeMP1
```

The **Cortex-M4** is used for real-time and hardware-level tasks, while the Cortex-A7 cores run OpenSTLinux.

The basic architecture is:

```text id="ovbhho"
┌─────────────────────────────┐
│ Cortex-A7 + OpenSTLinux     │
│                             │
│ Ethernet                    │
│ USB                         │
│ Filesystem                  │
│ GTK Application             │
└──────────────┬──────────────┘
               │
          OpenAMP / RPMsg
               │
┌──────────────▼──────────────┐
│ Cortex-M4                   │
│                             │
│ IMU acquisition             │
│ SPI / I2C                   │
│ GPIO                        │
│ Real-time processing        │
└─────────────────────────────┘
```

The general design rule used in this project is:

```text id="ifn5di"
Real-time / deterministic hardware tasks → Cortex-M4

GUI / Ethernet / USB / filesystem       → Cortex-A7 + Linux
```

Cortex-M4 firmware can be developed and debugged in two main ways:

```text id="6w4pbw"
Engineering Mode:
PC → ST-LINK → Cortex-M4

Production / Linux Mode:
PC → Linux → remoteproc → Cortex-M4
```

Direct ST-LINK debugging is useful during initial M4 development.

When Linux and M4 are running together, Linux manages the Cortex-M4 through **remoteproc**, while communication between the processors is performed using **OpenAMP/RPMsg**.

---

# 4. Development Flow

The project is developed in the following order:

```text id="lyr1h9"
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
