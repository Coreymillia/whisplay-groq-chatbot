#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

extern "C" {
#include "es8311.h"
}

namespace {

static constexpr int PIN_I2C_SDA = 15;
static constexpr int PIN_I2C_SCL = 14;
static constexpr int PIN_BOOT = 0;
static constexpr int PIN_ES8311_EN = 46;

static constexpr uint8_t ES8311_ADDR = 0x18;
static constexpr uint8_t ES7210_ADDR = 0x40;

static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t CAPTURE_MS = 1500;
static constexpr size_t READ_CHUNK_BYTES = 1280;   // 320 stereo frames, 16-bit
static constexpr size_t TX_CHUNK_BYTES = 1280;
static constexpr size_t MAX_CAPTURE_BYTES = SAMPLE_RATE * 2 * sizeof(int16_t) * 2; // 2 sec stereo

struct I2SPinSet {
    const char *name;
    int bclk;
    int ws;
    int dout;
    int din;
    int mclk;
};

struct TestCase {
    const char *name;
    i2s_port_t port;
    I2SPinSet pins;
    bool continuousTx;
};

struct CaptureStats {
    size_t bytes = 0;
    size_t readCalls = 0;
    size_t zeroReads = 0;
    size_t writeCalls = 0;
    size_t zeroWrites = 0;
    size_t frames = 0;
    uint64_t sumAbsL = 0;
    uint64_t sumAbsR = 0;
    uint16_t peakL = 0;
    uint16_t peakR = 0;
};

static constexpr I2SPinSet kPinsDemoExact {
    "demo-exact", 41, 45, 40, 42, 16
};
static constexpr I2SPinSet kPinsSchematicInterp {
    "schematic-interpreted", 45, 40, 16, 42, 41
};

static const TestCase kTests[] = {
    {"port1 demo exact", I2S_NUM_1, kPinsDemoExact, false},
    {"port1 demo exact + tx silence", I2S_NUM_1, kPinsDemoExact, true},
    {"port1 schematic", I2S_NUM_1, kPinsSchematicInterp, false},
    {"port1 schematic + tx silence", I2S_NUM_1, kPinsSchematicInterp, true},
    {"port0 demo exact", I2S_NUM_0, kPinsDemoExact, false},
    {"port0 schematic", I2S_NUM_0, kPinsSchematicInterp, false},
};
static constexpr size_t kTestCount = sizeof(kTests) / sizeof(kTests[0]);

uint8_t *captureBuffer = nullptr;
uint8_t txSilence[TX_CHUNK_BYTES] = {0};
size_t nextTestIndex = 0;
bool bootPrev = false;
bool addr18Present = false;
bool addr40Present = false;

bool i2cDevicePresent(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

void scanI2C() {
    Serial.println();
    Serial.println("=== I2C scan ===");
    addr18Present = false;
    addr40Present = false;
    for (uint8_t addr = 1; addr < 127; ++addr) {
        if (i2cDevicePresent(addr)) {
            Serial.printf("found 0x%02X\n", addr);
            if (addr == ES8311_ADDR) addr18Present = true;
            if (addr == ES7210_ADDR) addr40Present = true;
        }
    }
    Serial.printf("0x18 ES8311 present: %s\n", addr18Present ? "yes" : "no");
    Serial.printf("0x40 ES7210/ADC present: %s\n", addr40Present ? "yes" : "no");
    if (addr40Present) {
        Serial.println("NOTE: 0x40 strongly suggests a separate mic ADC path is present.");
    }
}

esp_err_t es8311CodecInit() {
    es8311_handle_t handle = es8311_create(0, ES8311_ADDR);
    if (!handle) {
        Serial.println("[ES8311] create failed");
        return ESP_FAIL;
    }

    const es8311_clock_config_t clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = SAMPLE_RATE * 256,
        .sample_frequency = SAMPLE_RATE,
    };

    esp_err_t err = es8311_init(handle, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    if (err != ESP_OK) return err;
    err = es8311_sample_frequency_config(handle, clk.mclk_frequency, clk.sample_frequency);
    if (err != ESP_OK) return err;
    err = es8311_microphone_config(handle, false);
    if (err != ESP_OK) return err;
    err = es8311_microphone_gain_set(handle, ES8311_MIC_GAIN_42DB);
    if (err != ESP_OK) return err;
    err = es8311_voice_volume_set(handle, 85, nullptr);
    if (err != ESP_OK) return err;
    return ESP_OK;
}

void printMemory() {
    Serial.printf("psramFound=%s psram=%u freePsram=%u freeHeap=%u largestHeap=%u largestPsram=%u\n",
                  psramFound() ? "yes" : "no",
                  static_cast<unsigned>(ESP.getPsramSize()),
                  static_cast<unsigned>(ESP.getFreePsram()),
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()),
                  static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
}

bool ensureCaptureBuffer() {
    if (captureBuffer) return true;

    if (psramFound()) {
        captureBuffer = static_cast<uint8_t *>(heap_caps_malloc(MAX_CAPTURE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (!captureBuffer) {
        captureBuffer = static_cast<uint8_t *>(heap_caps_malloc(MAX_CAPTURE_BYTES, MALLOC_CAP_8BIT));
    }
    if (!captureBuffer) {
        Serial.println("capture buffer allocation failed");
        return false;
    }

    Serial.printf("capture buffer allocated: %u bytes\n", static_cast<unsigned>(MAX_CAPTURE_BYTES));
    return true;
}

void processStereoChunk(const uint8_t *data, size_t len, CaptureStats &stats) {
    const size_t usable = len - (len % 4);
    const int16_t *samples = reinterpret_cast<const int16_t *>(data);
    const size_t frames = usable / 4;
    for (size_t i = 0; i < frames; ++i) {
        const int16_t left = samples[i * 2 + 0];
        const int16_t right = samples[i * 2 + 1];
        const uint16_t absL = static_cast<uint16_t>(abs(left));
        const uint16_t absR = static_cast<uint16_t>(abs(right));
        stats.sumAbsL += absL;
        stats.sumAbsR += absR;
        if (absL > stats.peakL) stats.peakL = absL;
        if (absR > stats.peakR) stats.peakR = absR;
    }
    stats.frames += frames;
    stats.bytes += usable;
}

void printStats(const CaptureStats &stats) {
    const float seconds = static_cast<float>(stats.bytes) / (SAMPLE_RATE * 2 * sizeof(int16_t));
    const uint32_t avgL = stats.frames ? static_cast<uint32_t>(stats.sumAbsL / stats.frames) : 0;
    const uint32_t avgR = stats.frames ? static_cast<uint32_t>(stats.sumAbsR / stats.frames) : 0;

    Serial.println("--- capture stats ---");
    Serial.printf("bytes=%u stereoSeconds=%.2f frames=%u\n",
                  static_cast<unsigned>(stats.bytes), seconds, static_cast<unsigned>(stats.frames));
    Serial.printf("readCalls=%u zeroReads=%u writeCalls=%u zeroWrites=%u\n",
                  static_cast<unsigned>(stats.readCalls),
                  static_cast<unsigned>(stats.zeroReads),
                  static_cast<unsigned>(stats.writeCalls),
                  static_cast<unsigned>(stats.zeroWrites));
    Serial.printf("L avg=%u peak=%u\n", avgL, stats.peakL);
    Serial.printf("R avg=%u peak=%u\n", avgR, stats.peakR);
}

void runTest(const TestCase &test) {
    if (!ensureCaptureBuffer()) return;
    memset(captureBuffer, 0, MAX_CAPTURE_BYTES);

    Serial.println();
    Serial.println("============================================================");
    Serial.printf("Running test: %s\n", test.name);
    Serial.printf("port=%d pins=%s bclk=%d ws=%d dout=%d din=%d mclk=%d txSilence=%s\n",
                  static_cast<int>(test.port), test.pins.name,
                  test.pins.bclk, test.pins.ws, test.pins.dout, test.pins.din, test.pins.mclk,
                  test.continuousTx ? "yes" : "no");
    printMemory();

    digitalWrite(PIN_ES8311_EN, HIGH);
    delay(10);
    scanI2C();

    if (addr18Present) {
        const esp_err_t err = es8311CodecInit();
        Serial.printf("ES8311 init: %s (%d)\n", err == ESP_OK ? "OK" : "FAIL", static_cast<int>(err));
    } else {
        Serial.println("ES8311 not detected on I2C; skipping init");
    }

    I2SClass i2s(test.port);
    i2s.setTimeout(1);
    i2s.setPins(test.pins.bclk, test.pins.ws, test.pins.dout, test.pins.din, test.pins.mclk);

    if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
        Serial.println("I2S begin failed");
        return;
    }

    Serial.println("Speak after the countdown...");
    for (int i = 3; i >= 1; --i) {
        Serial.printf("%d...\n", i);
        delay(500);
    }
    Serial.println("GO");

    CaptureStats stats;
    uint32_t started = millis();
    size_t cursor = 0;

    while ((millis() - started) < CAPTURE_MS && (cursor + READ_CHUNK_BYTES) <= MAX_CAPTURE_BYTES) {
        if (test.continuousTx) {
            ++stats.writeCalls;
            const size_t wrote = i2s.write(txSilence, sizeof(txSilence));
            if (wrote == 0) ++stats.zeroWrites;
        }

        ++stats.readCalls;
        const size_t got = i2s.readBytes(reinterpret_cast<char *>(captureBuffer + cursor), READ_CHUNK_BYTES);
        if (got == 0) {
            ++stats.zeroReads;
            delay(1);
            continue;
        }

        processStereoChunk(captureBuffer + cursor, got, stats);
        cursor += got;
    }

    i2s.end();
    printStats(stats);
    Serial.printf("Next test index: %u / %u\n", static_cast<unsigned>((nextTestIndex + 1) % kTestCount), static_cast<unsigned>(kTestCount));
    Serial.println("============================================================");
}

void printMenu() {
    Serial.println();
    Serial.println("MicTest ready.");
    Serial.println("Press BOOT to run the next capture test.");
    for (size_t i = 0; i < kTestCount; ++i) {
        Serial.printf("  [%u] %s\n", static_cast<unsigned>(i), kTests[i].name);
    }
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("GroqWatch MicTest boot");

    esp_log_level_set("ESP_I2S", ESP_LOG_NONE);
    esp_log_level_set("i2c", ESP_LOG_WARN);

    pinMode(PIN_BOOT, INPUT_PULLUP);
    pinMode(PIN_ES8311_EN, OUTPUT);
    digitalWrite(PIN_ES8311_EN, HIGH);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);

    printMemory();
    scanI2C();
    printMenu();
}

void loop() {
    const bool bootNow = digitalRead(PIN_BOOT) == LOW;
    if (bootNow && !bootPrev) {
        runTest(kTests[nextTestIndex]);
        nextTestIndex = (nextTestIndex + 1) % kTestCount;
        delay(300);
        printMenu();
    }
    bootPrev = bootNow;
    delay(10);
}
