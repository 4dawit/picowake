# PicoWake

Wake up PC from local web server hosted on Raspberry Pi Pico 2 W.

## Prerequisites

### Hardware
- Raspberry Pi Pico 2 W
- Micro-USB data cable 
- PC USB port that **stays powered while the PC sleeps** and supports wake in BIOS
- 2.4 GHz Wi-Fi network
- Browser (phone for me) on the same network

## Setup

### 1. Configure Wi-Fi
Copy the example file and fill in your details:

```bash
cp secrets.example.h secrets.h
```

Then edit `secrets.h`:

```c
#define WIFI_SSID "your_wifi_name"
#define WIFI_PASS "your_wifi_password"
```

### 2. Build

The Pico 2 W needs **Pico SDK 2.0 or newer**. Older SDKs fail with `Unable to find definition of board 'pico2_w'`.

**VS Code (Windows)**
1. Install VS Code and the [Raspberry Pi Pico extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico).
2. Run the extension's **Import Project** command on this folder. When prompted, pick the latest SDK 2.x and let the extension download the toolchain and picotool.
3. Run the extension's **Compile** command. The output is `build/picowake.uf2`.

**Bazzite / Linux (inside a container)**
```bash
distrobox create -n pico -i fedora:latest
distrobox enter pico

sudo dnf install -y cmake gcc gcc-c++ git python3 arm-none-eabi-gcc-cs arm-none-eabi-newlib
git clone --depth 1 --recurse-submodules --shallow-submodules \
    https://github.com/raspberrypi/pico-sdk ~/pico-sdk
export PICO_SDK_PATH=~/pico-sdk

cd /path/to/picowake
mkdir build && cd build
cmake .. -DPICO_BOARD=pico2_w
make -j4
```
### 3. Flash
1. Unplug the Pico from the PC.
2. Hold the **BOOTSEL** button, plug it back in while still holding, then release when the board mounts as a drive.
3. It mounts as a drive named `RP2350`. Copy `picowake.uf2` onto it.
4. The Pico reboots automatically. The onboard LED turns on once it has joined Wi-Fi.

### 4. Find the Pico and bookmark it
1. Find the device (hostname `picowake`) in your router's client list.
2. On your phone, open `http://<pico-ip>` and add the page to your home screen.
> Create a **DHCP reservation** (fixed IP) for the web server so your bookmark never breaks.

### 5. Enable wake on the PC

**BIOS/UEFI:** enable **USB wake / Resume by USB / USB standby power**. Disable **ErP/EuP** if present, since it cuts USB power during sleep.

**Windows**
1. Device Manager -> **Keyboards**.
2. Find the Pico entry (usually "HID Keyboard Device"). To confirm: Properties -> Details -> Hardware Ids -> `VID_CAFE&PID_4011`.
3. Properties -> **Power Management** -> tick **Allow this device to wake the computer**.
4. Verify in an admin terminal: `powercfg -devicequery wake_armed` should list it.

**Bazzite / Linux**
1. Create `/etc/udev/rules.d/90-picowake.rules`:
   ```
   ACTION=="add", SUBSYSTEM=="usb", ATTR{idVendor}=="cafe", ATTR{idProduct}=="4011", ATTR{power/wakeup}="enabled"
   ```
2. Reload rules: `sudo udevadm control --reload && sudo udevadm trigger`
3. Check the USB controller is armed: `cat /proc/acpi/wakeup`. Entries like `XHC`, `XHC0` or `XHC1` should read `*enabled`. If one reads `*disabled`, run `echo XHC | sudo tee /proc/acpi/wakeup` once. This file **toggles**, so only run it when disabled. To persist across reboots, run the same command from a small systemd service.
4. Check the sleep mode: `cat /sys/power/mem_sleep`. Both `s2idle` and `deep` should work with USB wake; try the other if one fails.
