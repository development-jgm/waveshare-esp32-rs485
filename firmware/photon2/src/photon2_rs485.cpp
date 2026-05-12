#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler(LOG_LEVEL_INFO);

// ── Pins ──────────────────────────────────────────────────────────────────────
// MAX3485 DE and RE pins tied together:  HIGH = transmit,  LOW = receive
static const pin_t DE_RE = D2;

// ── RS485 command table (mirrors esp32_control.py _CMD_TOGGLE) ────────────────
static const uint8_t CMD_TOGGLE[8][8] = {
    {0x06, 0x05, 0x00, 0x01, 0x55, 0x00, 0xA2, 0xED},  // CH1
    {0x06, 0x05, 0x00, 0x02, 0x55, 0x00, 0x52, 0xED},  // CH2
    {0x06, 0x05, 0x00, 0x03, 0x55, 0x00, 0x03, 0x2D},  // CH3
    {0x06, 0x05, 0x00, 0x04, 0x55, 0x00, 0xB2, 0xEC},  // CH4
    {0x06, 0x05, 0x00, 0x05, 0x55, 0x00, 0xE3, 0x2C},  // CH5
    {0x06, 0x05, 0x00, 0x06, 0x55, 0x00, 0x13, 0x2C},  // CH6
    {0x06, 0x05, 0x00, 0x07, 0x55, 0x00, 0x42, 0xEC},  // CH7
    {0x06, 0x05, 0x00, 0x08, 0x55, 0x00, 0x72, 0xEF},  // CH8
};

static const uint8_t CMD_QUERY_DI[8] = {
    0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ── Low-level RS485 ───────────────────────────────────────────────────────────

static void rs485Send(const uint8_t *buf, size_t len) {
    digitalWrite(DE_RE, HIGH);          // switch to transmit
    Serial1.write(buf, len);
    Serial1.flush();                    // block until shift register is empty
    delayMicroseconds(2000);            // guard time: 2 × byte period at 9600 baud
    digitalWrite(DE_RE, LOW);           // back to receive
}

// Query DI state for one device. Returns bitmask (0–255) or negative on error.
static int queryDevice(uint8_t addr) {
    while (Serial1.available()) Serial1.read();     // discard stale bytes

    uint8_t cmd[9];
    cmd[0] = addr;
    memcpy(cmd + 1, CMD_QUERY_DI, 8);
    rs485Send(cmd, 9);

    uint32_t deadline = millis() + 200;
    while (Serial1.available() < 9) {
        if (millis() > deadline) {
            Log.warn("DI query timeout (device %d)", addr);
            return -1;
        }
        delay(1);
    }

    uint8_t resp[9];
    Serial1.readBytes((char*)resp, 9);

    if (resp[0] == addr && resp[1] == 0x06 && resp[2] == 0x01) {
        return resp[3];     // DI bitmask
    }
    Log.warn("DI bad response (device %d): %02X %02X %02X", addr, resp[0], resp[1], resp[2]);
    return -2;
}

// ── Particle Cloud functions ──────────────────────────────────────────────────

// relay("device,channel[,duration_ms]")  →  0 on success, negative on error
//   e.g.  "1,3,500"  pulses relay 3 on device 1 for 500 ms
int cloudRelay(String args) {
    int c1 = args.indexOf(',');
    if (c1 < 0) return -1;
    int c2 = args.indexOf(',', c1 + 1);

    int device   = args.substring(0, c1).toInt();
    int channel  = args.substring(c1 + 1, c2 > 0 ? c2 : (int)args.length()).toInt();
    int duration = c2 > 0 ? args.substring(c2 + 1).toInt() : 100;

    if (device < 1 || device > 3)   return -1;
    if (channel < 1 || channel > 8) return -1;
    if (duration < 1)                return -1;

    uint8_t cmd[9];
    cmd[0] = (uint8_t)device;
    memcpy(cmd + 1, CMD_TOGGLE[channel - 1], 8);

    rs485Send(cmd, 9);          // relay ON  (toggle)
    delay(duration);
    rs485Send(cmd, 9);          // relay OFF (toggle again)

    Log.info("relay device=%d ch=%d dur=%d ms OK", device, channel, duration);
    return 0;
}

// queryDI("device")  →  DI bitmask 0–255, or -1 on timeout
//   e.g.  "1"  queries device 1;  bit 0 = DI1, bit 7 = DI8
int cloudQueryDI(String args) {
    int device = args.toInt();
    if (device < 1 || device > 3) return -1;
    return queryDevice((uint8_t)device);
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────

void setup() {
    pinMode(DE_RE, OUTPUT);
    digitalWrite(DE_RE, LOW);       // start in receive mode

    Serial1.begin(9600, SERIAL_8N1);

    Particle.function("relay",   cloudRelay);
    Particle.function("queryDI", cloudQueryDI);

    Log.info("Photon 2 RS485 bridge ready — relay + queryDI registered");
}

void loop() {
    // Device OS handles Wi-Fi and cloud connectivity
}
