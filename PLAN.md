# Project Plan — Waveshare ESP32 RS485 Control

Laundromat machine control and monitoring for **McClean La Cuesta** (shopId 50),
part of the Lavamax SmartKiosk system.

---

## Current topology

A Particle Photon 2 bridges Particle Cloud → RS485 (9600 8N1) to three Waveshare
ESP32-S3-POE-ETH-8DI-8RO modules. Photon device ID `0a10aced202194944a05320c`.

| RS485 address | Machines | machineId |
|---|---|---|
| 1 | Secadoras 6–9 | 99, 100, 101, 102 |
| 2 | Lavadora 1 | 94 |
| 3 | Lavadoras 2–5 | 95, 96, 97, 98 |

Per machine: one relay channel for activation, two opto-isolated digital inputs
(plugged / running). Bus terminated with 120 Ω at both ends. Waveshare firmware
**v1.1.0**; machine table lives in `firmware/easyCleanApp-RS485/src/EasyClean-RS485.cpp`.

**Wiring convention.** `INPUT_PULLUP`; opto ON = pin LOW = bit 0. The dryers'
"running" output is a dedicated 4N25 whose opto is **OFF while the machine runs**,
so `isRunning` = bit is 1. Note the asymmetry this creates: stopped is a driven
low (robust), running is held only by the pull-up (high impedance, noise-prone).
Glitches can therefore only ever fake a **stop**, never a start.

---

## Done

### Base system
- 9-byte addressed RS485 protocol: `[DEVICE_ADDRESS] + [8-byte payload]`; modules
  discard packets not addressed to them. CRC-16 Modbus is computed over the
  8-byte payload only, so the command tables are address-independent.
- Explicit ON (0xFF) / OFF (0x00) relay commands instead of toggle (0x55), which
  leaves a relay stuck if the second command is lost.
- Three modules flashed, addressed and verified on a shared bus.
- Photon 2 firmware `EasyClean-RS485.cpp` with cloud functions `activateMachine`,
  `testMachineIsPowered`, `testMachineIsUnderUsage`, `testConnectionToShop`,
  `publishNetworkInfo`, `rescanBus`, and the `modules` variable.
- Cash-payment detection: a machine that starts without a cloud activation
  publishes `SupabaseCashPayment/`.
- `MachineActivated` event published on every relay pulse (audit trail).

### The 2026-07-31 incident — root cause and fix
See [Lessons learned](#lessons-learned) for the reasoning. Summary of the fixes:

| commit | change |
|---|---|
| `3bdadb0` | Serialize RS485 traffic: one frame per `loop()` cycle |
| `658d88b` | Sort the machine table by `machineId` |
| `f52120b` | Poll the activated device first on the cycle after the relay pulse |
| `e2629d8` | Publish `MachineActivated` on every activation |
| `e2a544f` | **Fix the RS485 receive buffer overflow in the Waveshare firmware** |
| `7386d35` | Firmware version query + bus scan |

---

## What we gained

- **The false-record source is fixed at the root**, not papered over. ~250 bogus
  cash-payment records in 22 hours traced to a memory-corruption bug, not to
  sensors, wiring or the machines.
- **Remote visibility into the modules.** The `modules` Particle variable reports
  each module's firmware version, and distinguishes a healthy module from one
  still running old firmware (`legacy`) or from two modules sharing an address
  (`conflict`). Previously this could only be inferred from behaviour.
- **A bus that tolerates growth.** Traffic is serialized and the receive path is
  bounded, so adding a fourth module no longer risks corrupting the others.
- **Cloud functions that cannot stall the bus.** All of them either raise a flag
  or read cached state; none touches `Serial1`.
- **A measured basis for tuning.** Real machine turnaround times were derived
  from a week of clean data rather than guessed.

---

## Lessons learned

**A latent bug can stay unreachable until the system scales.** `RS485_Loop()` read
`available()` bytes straight into `buf[20]` with no bound. With one module the most
that can ever be buffered is a 9-byte query plus a 9-byte reply — 18 bytes, just
under the limit. Adding two modules put six frames (~54 bytes) on the wire per poll
cycle, and every module sees all of them because the bus is shared. The linker
places `lidarSerial` immediately after `buf`:

```
0x3fca1c24  buf          (20 bytes)
0x3fca1c38  lidarSerial  <-- overwritten
```

so the overflow corrupted the UART object itself. The bug had existed since day
one; the expansion merely made it reachable.

**Correlation pointed at the wrong culprit until we had a control window.** The
false records were 94.7% concentrated on the dryers, which made the dryers look
guilty — two hypotheses (drum-reversal cutting the sensor, then RS485 frame
corruption) were built on that and both were wrong. What settled it was exporting
the week *before* the change: 168 records with exactly **1** interval under 120 s,
versus 300 records with **217** afterwards, with the onset pinned to 2026-07-31
16:12 local. Same machines, same wiring, same optos. That turned a guessing game
into a regression with a known start time.

**Ask for the before-data early.** It cost less than any of the hypotheses it
disproved.

**A manufacturer's status output is not a motor contactor.** Reading the 4N25
schematic ruled out the drum-reversal theory in one step. Get the hardware
documentation before theorising about hardware behaviour.

**Particle cloud functions must be non-blocking.** They run in the application
thread; a `delay()` inside one lets `Particle.process()` re-enter `loop()`, and
both then write to the same RS485 bus. The rule: validate the argument, raise a
flag, return. Reads return cached state and never touch the bus.

**One RS485 frame per loop cycle.** The Waveshare polls its UART from a 50 ms task
and only parses packets that are exactly 9 bytes. Frames sent back to back merge
in its buffer and are dropped — which is what lost `CMD_OFF` commands and left a
relay stuck on.

**A flash that reports success has not necessarily taken effect.** The module keeps
running the old firmware until it is power-cycled over USB; `--after hard-reset`
is not enough. This cost debugging time twice, which is why the version query
exists now.

**Never take an irreversible action from a single sample.** Nothing in the chain
filtered anything: `digitalRead` every 20 ms with no debounce → `DIN_Flag` → RS485
reply → a single-sample decision on the Photon → a payment record in Supabase.
One bad reading was enough to invent a sale.

**Instrument before patching.** Applying the defensive filters before fixing the
overflow would have hidden whether the real fix worked.

---

## Tests in progress

**1. A full drying cycle must produce one record per use.** This is the acceptance
test for `e2a544f`. Before the fix, machine 100 produced 17 records in 13 minutes.

```sql
select machine_id, count(*), min(created_at), max(created_at)
from machine_usages
where machine_id between 94 and 102
  and created_at > '2026-08-01 13:00:00+00'
group by machine_id
order by machine_id;
```

One record per use closes the case. A dryer showing fifteen means a second cause
remains.

**2. Bus scan stability on a cold start.** The first scan after the module reflash
reported address 3 as `conflict`, then `legacy`, then settled — 8 consecutive clean
scans since. Most likely the module was still settling after its power-cycle, but
address 3 carries four machines, so recheck on the next cold start:

```bash
curl "https://api.particle.io/v1/devices/0a10aced202194944a05320c/modules?access_token=$TOKEN"
curl https://api.particle.io/v1/devices/.../rescanBus -d access_token=$TOKEN -d arg=""
```

**3. End-to-end cash payment** against the real application flow.

---

## Pending

### Defensive layers (designed, deliberately not yet applied)
Held back so they do not mask the result of test 1. Both are Photon-only, OTA.

- **Stop confirmation.** Require the running sensor to stay low for 2 minutes
  before declaring a machine stopped. Applied to the stop transition only, since
  glitches can only fake a stop — so activation detection keeps zero added latency.
  `testMachineIsUnderUsage` would then return the confirmed state instead of the
  raw bit, which also stops a dryer reading as free mid-cycle.
- **30-minute lockout** between cash-payment records for the same machine. Sized
  from a week of clean data: real turnarounds are ≥32 min (a machine cannot be
  reused before its cycle ends) and false duplicates are <14 min, leaving an empty
  band in between. A 30-minute lockout drops 3 of 159 records, and those three are
  themselves suspected duplicates. Must **not** gate `activateMachine` — an app
  payment has to fire the relay regardless.

### Frontend
- Replace the single post-activation check with a retry window of 20–30 s. The
  firmware refreshes cached state within ~1.6 s of the relay pulse; the real
  latency is the machine physically starting.

### Hardware
- Replace the MAX3485 with a galvanically isolated RS485 module.

### Integration
- Lavamax SmartKiosk connection to the real application flow.

### Data
- Decide what to do with the ~250 false records from 2026-07-31/08-01.
