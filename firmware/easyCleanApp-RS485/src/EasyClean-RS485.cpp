#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler(LOG_LEVEL_INFO);

// ── Shop configuration ────────────────────────────────────────────────────────
// Edit this section for each shop deployment.

int shopId = 50;

struct MachineConfig {
    int  machineId;     // ID used by the backend (0 = slot disabled)
    int  tariffId;      // Tariff ID reported on cash payment events
    int  rs485Device;   // Waveshare module address on the RS485 bus (1–3)
    int  relayChannel;  // Relay channel on that module (1–8)
    int  pluggedBit;    // Bit index in queryDI bitmask for "plugged" sensor (0–7)
    int  runningBit;    // Bit index in queryDI bitmask for "running" sensor (0–7)
};

static const int NUM_MACHINES = 6;

MachineConfig machines[NUM_MACHINES] = {
    {99,  53, 1, 1, 0, 1},  // Secadora 6:  device 1, relay 1, DI1 / DI2
    {100, 54, 1, 2, 2, 3},  // Secadora 7:  device 1, relay 2, DI3 / DI4
    {101, 53, 1, 3, 4, 5},  // Secadora 8:  device 1, relay 3, DI5 / DI6
    {102, 53, 1, 4, 6, 7},  // Secadora 9:  device 1, relay 4, DI7 / DI8
    {0,    0, 0, 0, 0, 0},  // unused
    {0,    0, 0, 0, 0, 0},  // unused
};

static const int RELAY_PULSE_MS = 500;  // pulse duration for machine activation

// ── RS485 relay command tables ────────────────────────────────────────────────
static const uint8_t CMD_ON[8][8] = {
    {0x06, 0x05, 0x00, 0x01, 0xFF, 0x00, 0xDC, 0x4D},  // CH1 ON
    {0x06, 0x05, 0x00, 0x02, 0xFF, 0x00, 0x2C, 0x4D},  // CH2 ON
    {0x06, 0x05, 0x00, 0x03, 0xFF, 0x00, 0x7D, 0x8D},  // CH3 ON
    {0x06, 0x05, 0x00, 0x04, 0xFF, 0x00, 0xCC, 0x4C},  // CH4 ON
    {0x06, 0x05, 0x00, 0x05, 0xFF, 0x00, 0x9D, 0x8C},  // CH5 ON
    {0x06, 0x05, 0x00, 0x06, 0xFF, 0x00, 0x6D, 0x8C},  // CH6 ON
    {0x06, 0x05, 0x00, 0x07, 0xFF, 0x00, 0x3C, 0x4C},  // CH7 ON
    {0x06, 0x05, 0x00, 0x08, 0xFF, 0x00, 0x0C, 0x4F},  // CH8 ON
};
static const uint8_t CMD_OFF[8][8] = {
    {0x06, 0x05, 0x00, 0x01, 0x00, 0x00, 0x9D, 0xBD},  // CH1 OFF
    {0x06, 0x05, 0x00, 0x02, 0x00, 0x00, 0x6D, 0xBD},  // CH2 OFF
    {0x06, 0x05, 0x00, 0x03, 0x00, 0x00, 0x3C, 0x7D},  // CH3 OFF
    {0x06, 0x05, 0x00, 0x04, 0x00, 0x00, 0x8D, 0xBC},  // CH4 OFF
    {0x06, 0x05, 0x00, 0x05, 0x00, 0x00, 0xDC, 0x7C},  // CH5 OFF
    {0x06, 0x05, 0x00, 0x06, 0x00, 0x00, 0x2C, 0x7C},  // CH6 OFF
    {0x06, 0x05, 0x00, 0x07, 0x00, 0x00, 0x7D, 0xBC},  // CH7 OFF
    {0x06, 0x05, 0x00, 0x08, 0x00, 0x00, 0x4D, 0xBF},  // CH8 OFF
};
static const uint8_t CMD_QUERY_DI[8] = {
    0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ── RS485 low-level ───────────────────────────────────────────────────────────
static const pin_t DE_RE = D2;

static void rs485Send(const uint8_t *buf, size_t len) {
    digitalWrite(DE_RE, HIGH);
    Serial1.write(buf, len);
    Serial1.flush();
    delayMicroseconds(2000);
    digitalWrite(DE_RE, LOW);
}

static int queryDevice(uint8_t addr) {
    while (Serial1.available()) Serial1.read();

    uint8_t cmd[9];
    cmd[0] = addr;
    memcpy(cmd + 1, CMD_QUERY_DI, 8);
    rs485Send(cmd, 9);

    uint32_t deadline = millis() + 200;
    while (Serial1.available() < 9) {
        if (millis() > deadline) {
            Log.warn("queryDI timeout (device %d)", addr);
            return -1;
        }
        delay(1);
    }

    uint8_t resp[9];
    Serial1.readBytes((char*)resp, 9);

    if (resp[0] == addr && resp[1] == 0x06 && resp[2] == 0x01) {
        return resp[3];
    }
    Log.warn("queryDI bad response (device %d): %02X %02X %02X", addr, resp[0], resp[1], resp[2]);
    return -2;
}

static void relayPulse(uint8_t device, int channel, int durationMs) {
    uint8_t cmdOn[9], cmdOff[9];
    cmdOn[0]  = device;
    cmdOff[0] = device;
    memcpy(cmdOn  + 1, CMD_ON[channel - 1],  8);
    memcpy(cmdOff + 1, CMD_OFF[channel - 1], 8);
    rs485Send(cmdOn, 9);
    delay(durationMs);
    rs485Send(cmdOff, 9);
}

// ── State ─────────────────────────────────────────────────────────────────────
bool machineWasActivatedFromCloud[NUM_MACHINES] = {};
bool aNewPaymentIsPossible[NUM_MACHINES]        = {true, true, true, true, true, true};
int  previousDIBitmask[3]                       = {-1, -1, -1};  // indexed by device-1

// ── Helpers ───────────────────────────────────────────────────────────────────
static int getMachineIndex(int id) {
    for (int i = 0; i < NUM_MACHINES; i++) {
        if (machines[i].machineId == id) return i;
    }
    return -1;
}

static bool isPlugged(int bitmask, int bit) {
    return (bitmask & (1 << bit)) == 0;  // bit=0 → opto ON → plugged
}

static bool isRunning(int bitmask, int bit) {
    return (bitmask & (1 << bit)) != 0;  // bit=1 → opto OFF → NC open → running
}

static void sendCashPaymentToSupabase(int mId, int tId) {
    String data = String::format("{\"machineId\":%d, \"tariffId\":%d}", mId, tId);
    Particle.publish("SupabaseCashPayment/", data, PRIVATE);
    Log.info("Cash payment: machineId=%d tariffId=%d", mId, tId);
}

// ── Particle Cloud functions ──────────────────────────────────────────────────

// activateMachine("machineId") → 1 on success, -1 if unknown machineId
int activateMachine(String machineIdStr) {
    int idx = getMachineIndex(machineIdStr.toInt());
    if (idx < 0 || machines[idx].machineId == 0) return -1;

    relayPulse(machines[idx].rs485Device, machines[idx].relayChannel, RELAY_PULSE_MS);
    machineWasActivatedFromCloud[idx] = true;
    Log.info("activateMachine: machineId=%d OK", machines[idx].machineId);
    return 1;
}

// testMachineIsPowered("machineId") → 1 if powered, 0 if not, -1 on error
int testMachineIsPowered(String machineIdStr) {
    int idx = getMachineIndex(machineIdStr.toInt());
    if (idx < 0) return -1;

    int bitmask = queryDevice(machines[idx].rs485Device);
    if (bitmask < 0) return -1;

    return isPlugged(bitmask, machines[idx].pluggedBit) ? 1 : 0;
}

// testMachineIsUnderUsage("machineId") → 1 if running, 0 if free, -1 on error
int testMachineIsUnderUsage(String machineIdStr) {
    int idx = getMachineIndex(machineIdStr.toInt());
    if (idx < 0) return -1;

    int bitmask = queryDevice(machines[idx].rs485Device);
    if (bitmask < 0) return -1;

    return isRunning(bitmask, machines[idx].runningBit) ? 1 : 0;
}

// testConnectionToShop("N") → N (echo test)
int testConnectionToShop(String data) {
    return data.toInt();
}

// publishNetworkInfo("") → 1, publishes "NetworkInfo" event
int publishNetworkInfo(String command) {
    String interfaceActive = "None";
    String ip = "0.0.0.0";
    String mac = "";

    if (WiFi.ready()) {
        interfaceActive = "Wi-Fi";
        ip = WiFi.localIP();
        uint8_t m[6];
        WiFi.macAddress(m);
        mac = String::format("%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    }

    String payload = "{";
    payload += "\"interface\":\"" + interfaceActive + "\",";
    payload += "\"ip\":\"" + ip + "\"";
    if (mac.length() > 0) payload += ",\"mac\":\"" + mac + "\"";
    payload += "}";

    Particle.publish("NetworkInfo", payload, PRIVATE);
    Serial.println("==== Network Info ====");
    Serial.println(payload);
    Serial.println("=====================");
    return 1;
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────

void setup() {
    pinMode(DE_RE, OUTPUT);
    digitalWrite(DE_RE, LOW);
    Serial1.begin(9600, SERIAL_8N1);

    Particle.function("activateMachine",         activateMachine);
    Particle.function("testConnectionToShop",    testConnectionToShop);
    Particle.function("testMachineIsPowered",    testMachineIsPowered);
    Particle.function("testMachineIsUnderUsage", testMachineIsUnderUsage);
    Particle.function("publishNetworkInfo",      publishNetworkInfo);

    Log.info("EasyClean RS485 ready — shopId=%d", shopId);
}

void loop() {
    delay(500);

    // Poll each unique device once per cycle
    bool polled[3]       = {false, false, false};
    int  bitmask[3]      = {-1, -1, -1};

    for (int i = 0; i < NUM_MACHINES; i++) {
        if (machines[i].machineId == 0) continue;
        int d = machines[i].rs485Device - 1;
        if (!polled[d]) {
            bitmask[d] = queryDevice(machines[i].rs485Device);
            polled[d]  = true;
        }
    }

    // Detect state changes per machine and handle cash payments
    for (int i = 0; i < NUM_MACHINES; i++) {
        if (machines[i].machineId == 0) continue;
        int d = machines[i].rs485Device - 1;
        if (bitmask[d] < 0) continue;

        bool plugged     = isPlugged(bitmask[d], machines[i].pluggedBit);
        bool running     = isRunning(bitmask[d], machines[i].runningBit);
        bool prevRunning = (previousDIBitmask[d] >= 0)
                           ? isRunning(previousDIBitmask[d], machines[i].runningBit)
                           : running;

        // Machine stopped: reset state so next cash payment can be detected
        if (prevRunning && !running) {
            machineWasActivatedFromCloud[i] = false;
            aNewPaymentIsPossible[i]        = true;
        }

        // Machine just started: if not activated from cloud → cash payment
        if (!prevRunning && running) {
            if (!machineWasActivatedFromCloud[i] && aNewPaymentIsPossible[i] && plugged) {
                sendCashPaymentToSupabase(machines[i].machineId, machines[i].tariffId);
                aNewPaymentIsPossible[i] = false;
            }
        }
    }

    // Save bitmasks for next cycle
    for (int d = 0; d < 3; d++) {
        if (bitmask[d] >= 0) previousDIBitmask[d] = bitmask[d];
    }
}
