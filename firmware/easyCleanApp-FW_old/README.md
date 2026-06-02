# EasyClean-FW — Developer Notes

Critical lessons learned from development. Read this before touching `setup()` or the Muon Ethernet configuration.

---

## Muon Ethernet (W5500) — Critical Setup Order

The Muon M524 uses a WIZnet W5500 Ethernet chip powered via the PMIC auxiliary rail (pin D7). Getting Ethernet to work reliably requires a **strict order** in `setup()`. Getting it wrong causes silent failures with no obvious error.

### Required order in `setup()`

```
1. Particle.function() registrations   — no hardware access, safe to run first
2. Particle.subscribe() registrations  — same
3. connectCableNetwork()               — Ethernet init (D7, W5500, DHCP, Particle.connect())
4. configureDeviceForShop()            — GPIO pin assignments
5. watchdogRefreshPin setup            — pinMode/digitalWrite
6. pin mode loop (startMachine, etc.)  — pinMode/digitalWrite/INPUT_PULLUP
7. attachInterrupt() calls             — interrupt handlers
```

### Why this order is mandatory

**Rule 1 — `Particle.function()` before `connectCableNetwork()`:**
With `SYSTEM_MODE(MANUAL)` + `SYSTEM_THREAD(ENABLED)`, the cloud handshake DESCRIBE message is sent during the connection process. Functions registered *before* `Particle.connect()` are included in the DESCRIBE and become visible in the Particle console. Functions registered *after* are callable (the device responds) but the cloud API shows `"functions": []` and the console shows nothing.

**Rule 2 — GPIO setup AFTER `connectCableNetwork()`:**
If any `pinMode()`, `digitalWrite()`, or `attachInterrupt()` calls run before `connectCableNetwork()`, the W5500 Ethernet physical link is never detected (`IFACE_LINK_UP` never fires). The interface reaches `IFACE_UP` but DHCP never starts. This happens even with 30+ second timeouts. The exact mechanism is unknown, but it is 100% reproducible. The PMIC auto-enables the W5500 aux rail at ~1400ms from boot — GPIO init before that point races with or interferes with this process.

---

## Muon Ethernet — `connectCableNetwork()` internals

```cpp
void connectCableNetwork() {
    // 1. Enable FEATURE_ETHERNET_DETECTION if not already (resets once, persists to flash)
    // 2. Configure PMIC aux power to D7/A7 if not already (resets once, persists to flash)
    // 3. delay(2000) — wait for PMIC to fully initialize
    // 4. Drive D7 HIGH manually to power W5500 3V3_AUX rail
    // 5. delay(1000) — W5500 power-up + link auto-negotiation
    // 6. Ethernet.on() → delay(1000) → Ethernet.connect()
    // 7. waitFor(Ethernet.ready, 30000) — wait for DHCP (up to 30s)
    // 8. Particle.connect() — or fall back to Cellular
}
```

**First-time boot:** The device resets itself once or twice to persist `FEATURE_ETHERNET_DETECTION` and the PMIC D7 config to flash. This is expected — it only happens on the very first boot after flashing.

**Expected serial log (working):**
```
[sys.power] Enable auxiliary power          ~1400ms  PMIC auto-enables W5500
[app]       Driving D7 HIGH to power 3V3_AUX  ~2500ms
[app]       Starting Ethernet...
[system.nm] IFACE_UP                          ~4600ms
[net.en]    Link up                           ~5000ms  physical link detected
[system.nm] IFACE_LINK_UP
[system.nm] IP_CONFIGURED                     ~5200ms  DHCP complete
[app]       Ethernet ready - connecting...
[system]    Cloud connected                   ~6000ms  done
```

If `Link up` never appears after `IFACE_UP`, GPIO setup ran before `connectCableNetwork()`.

---

## System Mode

```cpp
SYSTEM_MODE(MANUAL);    // Required: SEMI_AUTOMATIC interferes with W5500 init on Muon
SYSTEM_THREAD(ENABLED); // Required: allows setup() to keep running while system connects
```

`SYSTEM_THREAD(ENABLED)` is the default from Device OS 6.2.0 onward (compiler warns but it still works to be explicit).

---

## Flashing

**OTA (device must be online):**
```bash
particle compile muon . --saveTo firmware.bin
particle flash <device-name> firmware.bin
```

**USB / local (device connected via USB, put in DFU mode or use --local):**
```bash
particle compile muon . --saveTo firmware.bin
sudo particle flash --local firmware.bin
```

OTA and USB flash can conflict if issued too close together. Wait for the device to fully reboot and reconnect before issuing a second flash. Function calls during an active OTA return `"Unable to call function on device during an OTA"` — this is normal, not a code bug.

---

## Debugging Ethernet issues

Enable serial logging temporarily:

```cpp
// Uncomment this line (near the top of the file, after includes):
SerialLogHandler logHandler(LOG_LEVEL_INFO);
```

Then:
```bash
# Start monitor first, then reset — boot logs appear in the first ~6 seconds
sudo timeout 40 particle serial monitor --follow &
sleep 2
sudo particle usb reset
```

Re-comment the log handler before deploying to production (adds ~6KB to flash).

---

## Verifying functions are registered in the cloud

```bash
# Direct API check (replace token if expired)
curl -s "https://api.particle.io/v1/devices/<device-id>?access_token=<token>" \
  | python3 -m json.tool | grep -A 10 '"functions"'

# Should return all 5 functions:
# "activateMachine", "testConnectionToShop", "testMachineIsPowered",
# "testMachineIsUnderUsage", "publishNetworkInfo"
```

If `"functions": []` is returned but calls work, it means `Particle.function()` was called *after* `Particle.connect()` — fix the order in `setup()`.

---

## Per-device configuration

Set these variables at the top of `EasyClean-FW.cpp` for each shop:

```cpp
int shopId = ...;
int machineId[] = {...};
int tariffId[] = {...};
int usagePrice[] = {...};
bool isNonBrandedMachine[] = {...};
bool isMounBoard = true;   // or isPhoton2, isBoron, isArgon, isEthernetFeatherWing, isEvalBoard
```

Only one `is*` board flag should be `true`. The Muon is `isMounBoard`.

**Muon supports max 5 machines** (`maximumNumberOfMachinesForThisBoard = 5`). The pin setup loop runs up to index 5 (6 iterations) — index 5 uses uninitialized pins (default 0 = D0). This is a known issue that does not affect operation.
