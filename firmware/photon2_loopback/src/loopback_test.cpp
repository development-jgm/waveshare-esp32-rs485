#include "Particle.h"

SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler(LOG_LEVEL_INFO);

// MAX3485 #1 EN pin — HIGH = transmit (always TX in this test)
static const pin_t EN_TX = D2;

static const uint8_t TEST_BYTES[] = {0xAA, 0x55, 0x12, 0x34};
static const size_t  TEST_LEN     = sizeof(TEST_BYTES);

void setup() {
    pinMode(EN_TX, OUTPUT);
    digitalWrite(EN_TX, HIGH);   // MAX3485 #1 always in TX mode

    Serial1.begin(9600, SERIAL_8N1);

    // Give USB serial time to connect so first log line is visible
    delay(2000);
    Log.info("=== RS485 loopback test started ===");
    Log.info("Wiring: Photon2 TX -> MAX3485#1 -> A/B bus -> MAX3485#2 -> Photon2 RX");
}

void loop() {
    // Flush any stale bytes in RX buffer
    while (Serial1.available()) Serial1.read();

    // Send test pattern
    Serial1.write(TEST_BYTES, TEST_LEN);
    Serial1.flush();

    // Wait for loopback (generous timeout for 9600 baud)
    uint32_t deadline = millis() + 200;
    while (Serial1.available() < (int)TEST_LEN) {
        if (millis() > deadline) break;
        delay(1);
    }

    int received = Serial1.available();
    if (received < (int)TEST_LEN) {
        Log.error("FAIL — timeout: expected %d bytes, got %d", (int)TEST_LEN, received);
    } else {
        uint8_t buf[TEST_LEN];
        Serial1.readBytes((char*)buf, TEST_LEN);

        bool ok = memcmp(buf, TEST_BYTES, TEST_LEN) == 0;
        if (ok) {
            Log.info("OK   — loopback received: %02X %02X %02X %02X",
                     buf[0], buf[1], buf[2], buf[3]);
        } else {
            Log.warn("FAIL — data mismatch: got %02X %02X %02X %02X",
                     buf[0], buf[1], buf[2], buf[3]);
        }
    }

    delay(2000);
}
