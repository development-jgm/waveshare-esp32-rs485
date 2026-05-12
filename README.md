# Waveshare ESP32 RS485 Control Tool

Command-line tool to control a **Waveshare ESP32-S3-POE-ETH-8DI-8RO** module over RS485 serial. Supports relay activation with configurable pulse duration and continuous digital input polling.

## Hardware

| Component | Detail |
|-----------|--------|
| Module | Waveshare ESP32-S3-POE-ETH-8DI-8RO |
| Interface | RS485 (9600 baud 8N1) |
| Dongle | USB-to-RS485 (FTDI FT232R recommended) |
| Relays | 8× (10A 250VAC) — NO/COM/NC contacts |
| Inputs | 8× optically isolated digital inputs (INPUT_PULLUP) |

### Wiring the RS485 dongle

```
Dongle A+ → Module RS485 A+
Dongle B- → Module RS485 B-
Dongle GND → Module DGND  (not PE — signal ground reference)
```

> **Important:** Connect GND to **DGND**, not PE. Without this the RS485 signal reference is floating and communication is unreliable.

### Opto input convention

The module uses `INPUT_PULLUP`. The optocouplers pull the pin low when active:

| Opto state | DI pin | `inputs[i]` value |
|------------|--------|-------------------|
| ON (conducting) | LOW | `False` |
| OFF | HIGH (pullup) | `True` |

The script decodes two signals per machine:

```
plugged = not inputs[plugged_di]    # opto ON = plugged to mains
running = plugged and inputs[running_di]  # running opto ON = idle (normally-closed)
```

### Relay wiring

Use **COM + NO** contacts for normally-open activation, or **COM + NC** if the machine expects a break signal. Test with the `relay` command.

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
# Log out and back in, or:
newgrp dialout
```

Find your dongle's port:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
# or with dmesg:
dmesg | tail -20
```

---

## Usage

```bash
source venv/bin/activate

# Pulse relay 1 for 100 ms (default) on /dev/ttyUSB0 (default port)
python3 esp32_control.py relay --channel 1

# Pulse relay 2 for 500 ms on a specific port
python3 esp32_control.py relay --port /dev/ttyUSB1 --channel 2 --duration 500

# One-shot status of all digital inputs
python3 esp32_control.py status

# Continuous polling, refreshed every 500 ms
python3 esp32_control.py poll

# Poll every 200 ms
python3 esp32_control.py poll --interval 0.2
```

### Global options

| Option | Default | Description |
|--------|---------|-------------|
| `--port PATH` | `/dev/ttyUSB0` | Serial device |
| `--baudrate N` | `9600` | Baud rate |

### `relay` — activate a relay

```
python3 esp32_control.py relay --channel N [--duration MS] [--device N]
```

| Option | Default | Description |
|--------|---------|-------------|
| `--channel N` | *(required)* | Relay channel 1–8 |
| `--duration MS` | `100` | Pulse duration in milliseconds |
| `--device N` | `1` | Device address (for multi-unit bus) |

### `status` — one-shot DI read

```
python3 esp32_control.py status [--device N [N ...]]
```

Prints raw DI values and decoded machine states.

### `poll` — continuous monitoring

```
python3 esp32_control.py poll [--device N [N ...]] [--interval SEC]
```

Refreshes the terminal in place. Press `Ctrl+C` to stop.

---

## DI wiring map (default)

Adjust `MACHINE_DI` in `esp32_control.py` to match your physical wiring.

| Machine | Plugged DI | Running DI |
|---------|-----------|------------|
| 1 | DI1 (index 0) | DI2 (index 1) |
| 2 | DI3 (index 2) | DI4 (index 3) |
| 3 | DI5 (index 4) | DI6 (index 5) |
| 4 | DI7 (index 6) | DI8 (index 7) |

---

## Multi-device RS485 bus

Up to 3 modules can share the same RS485 bus. Each must be flashed with a unique device address. The `--device` flag selects which unit to address.

> **Requires firmware modification.** With the default Waveshare firmware all units respond to the same commands — they must be reflashed with addressed firmware (9-byte protocol, `DEVICE_ADDRESS` define in `WS_RS485.h`).

---

## FTDI dongle note

FTDI FT232R dongles send RTS/CTS flowcontrol URBs by default, which causes a USB disconnect (errno 5) after the first RS485 write. The script disables RTS/CTS and DTR/DSR on open and explicitly clears both lines to prevent this.

---

## Firmware

The tool requires the **modified Waveshare MAIN_ALL firmware** with:

- Custom DI query command (`06 01 00 00 00 00 00 00` → response with DI bitmask)
- `Relay_Immediate_Default = 0` in `WS_DIN.h` (disables DI→relay auto-mirroring)

Source and flash script are in the [Lavamax SmartKiosk](https://github.com/development-jgm/Lavamax_SmartKiosk) project.
