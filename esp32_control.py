#!/usr/bin/env python3
"""
Waveshare ESP32-S3-POE-ETH-8DI-8RO — RS485 control tool

Relay activation and digital input polling over RS485 serial.
Supports up to 3 ESP32 units on the same RS485 bus (requires addressed firmware).

Hardware
--------
  Waveshare ESP32-S3-POE-ETH-8DI-8RO
    - 8 relay outputs (NO/COM/NC contacts)
    - 8 optically-isolated digital inputs (INPUT_PULLUP)
    - RS485 interface → USB-to-RS485 dongle → host computer

Opto wiring convention (INPUT_PULLUP)
--------------------------------------
  Opto ON  → DI pulled LOW  (False from firmware)
  Opto OFF → DI pulled HIGH (True  from firmware)

  Per channel pair (plugged_di, running_di):
    plugged  = not inputs[plugged_di]   # opto ON = plugged to mains
    running  = plugged and inputs[running_di]  # running opto ON = idle (normally-closed)

Usage
-----
  # Pulse relay 1 for 100 ms (default)
  python3 esp32_control.py relay --channel 1

  # Pulse relay 2 on device 2 for 500 ms
  python3 esp32_control.py relay --device 2 --channel 2 --duration 500

  # One-shot status
  python3 esp32_control.py status

  # Continuous polling (all devices)
  python3 esp32_control.py poll --device 1 2 3 --interval 0.5

Multi-device note
-----------------
When multiple ESP32s share the same RS485 bus each unit must be flashed with a
unique DEVICE_ADDRESS (0x01, 0x02, 0x03) in WS_RS485.h, and the firmware must
implement the 9-byte addressed protocol. With the default single-device firmware
only --device 1 is valid.
"""

import sys
import time
import argparse
import os
import serial

# ── RS485 commands (from Waveshare WS_RS485.cpp firmware data[][] table) ──────
_CMD_TOGGLE = [
    bytes([0x06, 0x05, 0x00, 0x01, 0x55, 0x00, 0xA2, 0xED]),  # CH1
    bytes([0x06, 0x05, 0x00, 0x02, 0x55, 0x00, 0x52, 0xED]),  # CH2
    bytes([0x06, 0x05, 0x00, 0x03, 0x55, 0x00, 0x03, 0x2D]),  # CH3
    bytes([0x06, 0x05, 0x00, 0x04, 0x55, 0x00, 0xB2, 0xEC]),  # CH4
    bytes([0x06, 0x05, 0x00, 0x05, 0x55, 0x00, 0xE3, 0x2C]),  # CH5
    bytes([0x06, 0x05, 0x00, 0x06, 0x55, 0x00, 0x13, 0x2C]),  # CH6
    bytes([0x06, 0x05, 0x00, 0x07, 0x55, 0x00, 0x42, 0xEC]),  # CH7
    bytes([0x06, 0x05, 0x00, 0x08, 0x55, 0x00, 0x72, 0xEF]),  # CH8
]
_CMD_ALL_OFF       = bytes([0x06, 0x05, 0x00, 0xFF, 0x00, 0x00, 0xFC, 0x4D])
_CMD_QUERY_INPUTS  = bytes([0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])

# ── DI wiring map ─────────────────────────────────────────────────────────────
# (plugged_di_index, running_di_index) — 0-based, matches physical wiring
MACHINE_DI = {
    1: (0, 1),
    2: (2, 3),
    3: (4, 5),
    4: (6, 7),
}

DEFAULT_PULSE_MS = 100


# ── Low-level serial helpers ───────────────────────────────────────────────────

def open_port(port: str, baudrate: int) -> serial.Serial:
    """
    Open RS485 serial port with FTDI flowcontrol fix.

    FTDI FT232R sends flowcontrol URBs by default that cause errno 5
    (USB device disconnect) after the first write. Disabling RTS/CTS
    and DTR/DSR, then explicitly clearing RTS and DTR, prevents this.
    """
    ser = serial.Serial(
        port, baudrate=baudrate, bytesize=8, parity='N', stopbits=1,
        timeout=1.0, write_timeout=2,
        xonxoff=False, rtscts=False, dsrdtr=False,
    )
    ser.setRTS(False)
    ser.setDTR(False)
    return ser


def relay_on(ser: serial.Serial, channel: int, device: int = 1):
    if not 1 <= channel <= 8:
        raise ValueError(f"Channel must be 1–8, got {channel}")
    ser.write(bytes([device]) + _CMD_TOGGLE[channel - 1])
    time.sleep(0.05)


def relay_off(ser: serial.Serial, channel: int, device: int = 1):
    if not 1 <= channel <= 8:
        raise ValueError(f"Channel must be 1–8, got {channel}")
    ser.write(bytes([device]) + _CMD_TOGGLE[channel - 1])
    time.sleep(0.05)


def get_inputs(ser: serial.Serial, device: int = 1) -> list:
    """Return list of 8 bools (True = DI HIGH). Returns all False on timeout."""
    ser.reset_input_buffer()
    ser.write(bytes([device]) + _CMD_QUERY_INPUTS)
    time.sleep(0.1)
    if ser.in_waiting >= 9:
        response = ser.read(9)
        if response[0] == device and response[1] == 0x06 and response[2] == 0x01:
            di_byte = response[3]
            return [(di_byte >> i) & 1 == 1 for i in range(8)]
    return [False] * 8


# ── Transport layer ───────────────────────────────────────────────────────────
#
# Both transports expose the same three methods:
#   relay_pulse(device, channel, duration_ms)
#   get_inputs(device) → list[bool]
#   close()

class _SerialTransport:
    def __init__(self, port: str, baudrate: int):
        self._ser = open_port(port, baudrate)
        self._port = port

    def relay_pulse(self, device: int, channel: int, duration_ms: int):
        relay_on(self._ser, channel, device)
        time.sleep(duration_ms / 1000.0)
        relay_off(self._ser, channel, device)

    def get_inputs(self, device: int) -> list:
        return get_inputs(self._ser, device)

    def close(self):
        self._ser.close()

    @property
    def label(self) -> str:
        return self._port


class _ParticleTransport:
    _API = "https://api.particle.io/v1/devices"

    def __init__(self, photon_id: str, token: str):
        import requests as _req
        self._req = _req
        self._url = f"{self._API}/{photon_id}"
        self._headers = {"Authorization": f"Bearer {token}"}
        self._id = photon_id

    def _call(self, func: str, arg: str) -> int:
        r = self._req.post(
            f"{self._url}/{func}",
            data={"arg": arg},
            headers=self._headers,
            timeout=15,
        )
        r.raise_for_status()
        return r.json()["return_value"]

    def relay_pulse(self, device: int, channel: int, duration_ms: int):
        ret = self._call("relay", f"{device},{channel},{duration_ms}")
        if ret < 0:
            raise RuntimeError(f"Relay failed (device={device} ch={channel} ret={ret})")

    def get_inputs(self, device: int) -> list:
        bitmask = self._call("queryDI", str(device))
        if bitmask < 0:
            return [False] * 8
        return [(bitmask >> i) & 1 == 1 for i in range(8)]

    def close(self):
        pass

    @property
    def label(self) -> str:
        return f"Particle:{self._id[:8]}"


def _build_transport(args):
    if getattr(args, "photon", None):
        token = os.environ.get("PARTICLE_TOKEN")
        if not token:
            print("ERROR: PARTICLE_TOKEN environment variable not set.", file=sys.stderr)
            sys.exit(1)
        return _ParticleTransport(args.photon, token)
    return _SerialTransport(args.port, args.baudrate)


# ── State decoding ─────────────────────────────────────────────────────────────

def decode_inputs(inputs: list) -> dict:
    result = {}
    for machine, (p_di, r_di) in MACHINE_DI.items():
        plugged = not inputs[p_di]
        running = plugged and inputs[r_di]
        result[machine] = {"plugged": plugged, "running": running}
    return result


def format_state(machine: int, state: dict) -> str:
    if not state["plugged"]:
        return f"  Machine {machine}:  ○  out of service"
    if state["running"]:
        return f"  Machine {machine}:  ▶  running"
    return f"  Machine {machine}:  ●  available"


# ── Commands ───────────────────────────────────────────────────────────────────

def cmd_relay(args):
    t = _build_transport(args)
    try:
        dev, ch, ms = args.device[0], args.channel, args.duration
        print(f"[device {dev}] Relay {ch} — pulse {ms} ms …", end="", flush=True)
        t.relay_pulse(dev, ch, ms)
        print(" done")
    finally:
        t.close()


def cmd_status(args):
    t = _build_transport(args)
    try:
        for dev in args.device:
            inputs = t.get_inputs(dev)
            states = decode_inputs(inputs)
            raw = " ".join(f"DI{i+1}={'1' if v else '0'}" for i, v in enumerate(inputs))
            print(f"\n── Device {dev} ({t.label}) ─────────────────────────")
            print(f"  Raw: {raw}")
            for machine, state in states.items():
                print(format_state(machine, state))
    finally:
        t.close()


def cmd_poll(args):
    t = _build_transport(args)
    print(f"Polling device(s) {args.device} via {t.label} — Ctrl+C to stop.\n")
    prev_lines = 0

    try:
        while True:
            lines = []
            for dev in args.device:
                try:
                    inputs = t.get_inputs(dev)
                except Exception as e:
                    lines.append(f"Device {dev}: ERROR {e}")
                    continue
                states = decode_inputs(inputs)
                lines.append(f"── Device {dev} ───────────────────────────────────")
                for machine, state in states.items():
                    lines.append(format_state(machine, state))

            if prev_lines:
                sys.stdout.write(f"\033[{prev_lines}A")
            prev_lines = len(lines) + 1

            ts = time.strftime("%H:%M:%S")
            sys.stdout.write(f"\r[{ts}]\n")
            for line in lines:
                sys.stdout.write(f"\r{line:<60}\n")
            sys.stdout.flush()

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        t.close()


# ── CLI ────────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Waveshare ESP32 RS485 control tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--port",     default="/dev/ttyUSB0",
                        help="Serial port (default: /dev/ttyUSB0)")
    parser.add_argument("--baudrate", type=int, default=9600,
                        help="Baud rate (default: 9600)")
    parser.add_argument("--photon",   metavar="DEVICE_ID",
                        help="Particle device ID — use Particle Cloud instead of serial. "
                             "Requires PARTICLE_TOKEN env var.")

    sub = parser.add_subparsers(dest="command", required=True)

    # relay
    p = sub.add_parser("relay", help="Pulse a relay for a given duration")
    p.add_argument("--device",   type=int, nargs=1, default=[1], metavar="N",
                   help="Device address (default: 1)")
    p.add_argument("--channel",  type=int, required=True, metavar="N",
                   help="Relay channel 1–8")
    p.add_argument("--duration", type=int, default=DEFAULT_PULSE_MS, metavar="MS",
                   help=f"Pulse duration in ms (default: {DEFAULT_PULSE_MS})")
    p.set_defaults(func=cmd_relay)

    # status
    p = sub.add_parser("status", help="One-shot DI status read")
    p.add_argument("--device", type=int, nargs="+", default=[1], metavar="N",
                   help="Device address(es) (default: 1)")
    p.set_defaults(func=cmd_status)

    # poll
    p = sub.add_parser("poll", help="Continuous DI polling")
    p.add_argument("--device",   type=int, nargs="+", default=[1], metavar="N",
                   help="Device address(es) (default: 1)")
    p.add_argument("--interval", type=float, default=0.5, metavar="SEC",
                   help="Poll interval in seconds (default: 0.5)")
    p.set_defaults(func=cmd_poll)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
