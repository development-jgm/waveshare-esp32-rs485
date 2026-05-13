# Project Plan — Waveshare ESP32 RS485 Control

## Done

- **ESP32 multi-device firmware** — 9-byte addressed RS485 protocol; each module ignores packets not addressed to it
- **Flash 2 Waveshare modules** — device 1 and device 2 flashed and verified on shared bus
- **Python CLI** — `relay`, `status`, `poll` commands; transport abstraction (`_SerialTransport` / `_ParticleTransport`)
- **Particle Photon 2 + MAX3485** — `photon2_rs485.cpp` firmware with Cloud functions `relay` and `queryDI`
- **RS485 bus working** — relay on device 1 and device 2 activated via Particle Cloud
- **Documentation** — README updated, TX/RX labeling finding saved in memory and repo

## Pending

### Immediate
- **Restore DE switching on Photon 2** — DE is currently always HIGH; needs correct switching (HIGH for TX, LOW for RX) so that `queryDI` works (currently returns -1 because the receiver is disabled while DE is HIGH)
- **Verify `queryDI` via Particle Cloud** — once DE switching is restored, confirm it returns the correct DI bitmask

### Next phase
- **Isolated RS485 module** — replace MAX3485 with a galvanically isolated module (eliminates the need for a ground reference between devices)
- **Device 3** — flash and verify the third Waveshare module when available
- **Lavamax SmartKiosk integration** — connect the system to the real application flow
