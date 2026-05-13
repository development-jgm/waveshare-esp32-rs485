# Waveshare ESP32 RS485 Control Tool

Command-line tool to control up to 3 **Waveshare ESP32-S3-POE-ETH-8DI-8RO** modules over a shared RS485 bus. Supports per-device relay activation with configurable pulse duration and continuous digital input polling.

## Hardware

| Component | Detail |
|-----------|--------|
| Module | Waveshare ESP32-S3-POE-ETH-8DI-8RO |
| Interface | RS485 (9600 baud 8N1) |
| RS485 bridge | Particle Photon 2 + MAX3485 breakout (Wi-Fi → RS485) |
| Relays | 8× (10A 250VAC) — NO/COM/NC contacts |
| Inputs | 8× optically isolated digital inputs (INPUT_PULLUP) |

## Wiring

### Particle Photon 2 + MAX3485 (production RS485 bridge)

```
Photon 2 pin    MAX3485 breakout    Notes
────────────    ────────────────    ─────────────────────────────────
TX              RX                  ← breakout RX = DI (driver input)
RX              TX                  ← breakout TX = RO (receiver output)
D2              EN                  HIGH=transmit, LOW=receive
3V3             VCC
GND             GND (VCC side)
                A  ─────────────── Waveshare RS485 A
                B  ─────────────── Waveshare RS485 B
```

> **MAX3485 breakout TX/RX labeling:** these generic AliExpress breakouts label pins from the **chip's** perspective, not the MCU's. `RX` on the breakout is the Driver Input (DI) — connect it to MCU TX. `TX` is the Receiver Output (RO) — connect it to MCU RX. This is the opposite of standard UART cross-wiring convention.

> **GND reference:** RS485 is differential — A+B alone is sufficient when both ends share a close ground potential (same enclosure, same power rail). Add a GND wire between the MAX3485 A/B-side GND and the **PE** terminal on the Waveshare RS485 block if communication is unreliable across separate power sources or long cable runs. Do not use DGND — the Waveshare RS485 bus is isolated from the board's digital ground.

> **Bus contention:** if VCC of the MAX3485 droops below 3V, the EN pin is likely floating. Ensure EN is firmly tied to D2 (not left unconnected) — a floating EN enables both TX and RX simultaneously and causes a current short on the bus.

### Opto input convention

The module uses `INPUT_PULLUP`. The optocouplers pull the pin low when active:

| Opto state | DI pin | `inputs[i]` value |
|------------|--------|-------------------|
| ON (conducting) | LOW | `False` |
| OFF | HIGH (pullup) | `True` |

The script decodes two signals per machine:

```
plugged = not inputs[plugged_di]          # opto ON = plugged to mains
running = plugged and inputs[running_di]  # running opto ON = idle (normally-closed)
```

### Relay wiring

Use **COM + NC** contacts if the machine expects a break signal (circuit normally closed, opened on activation). Use **COM + NO** for normally-open activation. Test with the `relay` command.

---

## Photon 2 firmware

Flash `firmware/photon2/` to the Photon 2:

```bash
particle compile p2 firmware/photon2 --saveTo firmware/photon2/photon2.bin
particle flash --usb firmware/photon2/photon2.bin
```

---

## Installation

```bash
git clone https://github.com/development-jgm/waveshare-esp32-rs485.git
cd waveshare-esp32-rs485

python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Serial port permissions (Linux)

```bash
sudo usermod -aG dialout $USER
# Log out and back in, or apply immediately:
newgrp dialout
```

---

## Flashing the ESP32 firmware

The module must run the modified firmware included in `firmware/MAIN_ALL/` before the control tool will work. The modifications are:

- **`WS_RS485.cpp`** — implements the 9-byte addressed protocol and adds a DI query command that responds with the 8 digital input states as a bitmask
- **`WS_DIN.h`** — `Relay_Immediate_Default = 0` disables DI→relay auto-mirroring (prevents relays firing when a machine is plugged in)

### Entering bootloader mode (required before every flash)

The ESP32-S3 USB JTAG interface does not support automatic reset into download mode. You must put the module into bootloader mode manually each time:

1. **Disconnect** the USB cable from the module
2. **Hold** the **BOOT** button
3. **Connect** the USB cable while holding BOOT
4. **Release** BOOT

The module will stay in ROM bootloader mode (stable USB connection) until flashed and reset.

### Flash steps

```bash
source venv/bin/activate

# Flash as device 1 (port auto-detected)
./flash.sh --address 1

# Flash as device 2
./flash.sh --address 2

# Explicit port
./flash.sh --address 1 --port /dev/ttyACM0
```

After flashing, disconnect the USB cable and connect the module to the RS485 bus.

---

## Usage

```bash
source venv/bin/activate
export PARTICLE_TOKEN=your_token_here

# One-shot status of all digital inputs, device 1
python3 esp32_control.py --photon YOUR_DEVICE_ID status

# Pulse relay 1 on device 1 for 100 ms (default)
python3 esp32_control.py --photon YOUR_DEVICE_ID relay --channel 1

# Pulse relay 1 on device 2 for 500 ms
python3 esp32_control.py --photon YOUR_DEVICE_ID relay --device 2 --channel 1 --duration 500

# Continuous polling of both devices, refreshed every 500 ms
python3 esp32_control.py --photon YOUR_DEVICE_ID poll --device 1 2
```

### Global options

| Option | Default | Description |
|--------|---------|-------------|
| `--photon DEVICE_ID` | — | Use Particle Cloud transport (production) |
| `--port PATH` | `/dev/ttyUSB0` | Use USB dongle transport (development) |
| `--baudrate N` | `9600` | Baud rate (USB dongle only) |

### `relay` — activate a relay

```
python3 esp32_control.py relay --channel N [--duration MS] [--device N]
```

| Option | Default | Description |
|--------|---------|-------------|
| `--channel N` | *(required)* | Relay channel 1–8 |
| `--duration MS` | `100` | Pulse duration in milliseconds |
| `--device N` | `1` | Device address (1–3) |

### `status` — one-shot DI read

```
python3 esp32_control.py status [--device N [N ...]]
```

Prints raw DI values and decoded machine states for each device.

### `poll` — continuous monitoring

```
python3 esp32_control.py poll [--device N [N ...]] [--interval SEC]
```

Refreshes the terminal in place. Press `Ctrl+C` to stop.

---

## RS485 protocol

Commands are **9 bytes**: `[DEVICE_ADDRESS] + [8-byte payload]`. Each module silently discards packets not addressed to it, so all devices can share the same bus without collisions.

DI query response is also 9 bytes: `[DEVICE_ADDRESS, 0x06, 0x01, DI_BITMASK, 0x00, 0x00, 0x00, 0x00, 0x00]`.

---

## Per-machine wiring

Each washing machine uses two DI inputs and one relay output on the ESP32 module:

| Signal | DI | Opto ON means… | Wiring |
|--------|----|----------------|--------|
| Plugged | DI odd (1, 3, 5, 7) | Machine plugged into mains | Normally-open sensor to opto input |
| Running | DI even (2, 4, 6, 8) | Machine in use (busy) | Normally-closed sensor to opto input — opto conducting = idle |
| Activate | DO (relay) | — | Relay COM+NO (or COM+NC) to machine start signal |

The normally-closed running sensor means: opto ON (conducting) → DI LOW → `inputs[i] = False` → machine is **idle/available**. When the machine starts, the contact opens → opto OFF → `inputs[i] = True` → machine is **running**.

### DI wiring map (default)

Adjust `MACHINE_DI` in `esp32_control.py` to match your physical wiring.

| Machine | Plugged DI | Running DI | Relay channel |
|---------|-----------|------------|---------------|
| 1 | DI1 (index 0) | DI2 (index 1) | CH1 |
| 2 | DI3 (index 2) | DI4 (index 3) | CH2 |
| 3 | DI5 (index 4) | DI6 (index 5) | CH3 |
| 4 | DI7 (index 6) | DI8 (index 7) | CH4 |

---

## Multi-device RS485 bus

Up to 3 modules can share the same RS485 bus. Each must be flashed with a unique address using `--address N`. Flash each module one at a time via USB, then connect all to the bus:

```bash
# Flash each module individually (with BOOT button procedure each time)
./flash.sh --address 1
./flash.sh --address 2
./flash.sh --address 3

# Then poll all via Particle Cloud
python3 esp32_control.py --photon YOUR_DEVICE_ID poll --device 1 2 3
```

---

## USB dongle (development / debug only)

A USB-to-RS485 dongle can be used during development or to sniff bus traffic. It appears as `/dev/ttyUSB0` on Linux.

```
Dongle A+ → Module RS485 A+
Dongle B- → Module RS485 B-
```

```bash
python3 esp32_control.py --port /dev/ttyUSB0 status
```

Find your dongle's port:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
# or:
dmesg | tail -20
```

> **FTDI note:** FTDI FT232R dongles send RTS/CTS flowcontrol URBs by default, which causes a USB disconnect (errno 5) after the first RS485 write. The script disables RTS/CTS and DTR/DSR on open and explicitly clears both lines to prevent this.
