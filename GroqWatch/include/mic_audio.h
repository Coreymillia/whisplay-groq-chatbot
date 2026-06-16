#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "pin_config.h"
#include "es7210_arduino.h"
#include "../src/es8311.h"

namespace MicAudio {

static constexpr int SAMPLE_RATE = 16000;
static constexpr int BITS_PER_SAMPLE = 16;
static constexpr int CHANNELS = 2;  // ES7210 stereo output (MIC1 left, MIC2 right)
static constexpr int SPARSE_CYCLE_FRAMES = 10;
static constexpr int SPARSE_KEEP_FRAMES = 2;
static constexpr size_t WAV_HEADER_BYTES = 44;
static constexpr int TARGET_RECORD_SEC = 6;
static constexpr int MIN_RECORD_SEC = 1;
static constexpr size_t TARGET_PCM_BYTES = SAMPLE_RATE * CHANNELS * sizeof(int16_t) * TARGET_RECORD_SEC;
static constexpr size_t MIN_PCM_BYTES = SAMPLE_RATE * CHANNELS * sizeof(int16_t) * MIN_RECORD_SEC;
static constexpr size_t HEAP_HEADROOM_BYTES = 48 * 1024;
static constexpr size_t READ_CHUNK_BYTES = 1280;  // ~20 ms stereo @ 16 kHz / 16-bit
static constexpr int MAX_POLL_READS_PER_CALL = 6;

inline bool initialized = false;
inline bool recording = false;
inline uint8_t *captureBuffer = nullptr;
inline size_t captureCapacityBytes = 0;
inline size_t pcmCapacityBytes = 0;
inline size_t pcmBytes = 0;
inline unsigned long recordingStartedMs = 0;
inline unsigned long lastStatsMs = 0;
inline unsigned long lastDataMs = 0;
inline uint32_t wavSampleRate = SAMPLE_RATE;
inline uint16_t wavChannels = CHANNELS;
inline es8311_handle_t speakerCodec = nullptr;
inline bool speakerCodecReady = false;
inline I2SClass i2s;

#pragma pack(push, 1)
struct WavHeader {
    char     riff[4]       = {'R','I','F','F'};
    uint32_t fileSize      = 0;
    char     wave[4]       = {'W','A','V','E'};
    char     fmt[4]        = {'f','m','t',' '};
    uint32_t fmtSize       = 16;
    uint16_t audioFormat   = 1;  // PCM
    uint16_t numChannels   = CHANNELS;
    uint32_t sampleRate    = SAMPLE_RATE;
    uint32_t byteRate      = SAMPLE_RATE * CHANNELS * sizeof(int16_t);
    uint16_t blockAlign    = CHANNELS * sizeof(int16_t);
    uint16_t bitsPerSample = 16;
    char     data[4]       = {'d','a','t','a'};
    uint32_t dataSize      = 0;
};
#pragma pack(pop)
static_assert(sizeof(WavHeader) == WAV_HEADER_BYTES, "Unexpected WAV header size");

inline unsigned long maxRecordMs() {
    if (!pcmCapacityBytes) return static_cast<unsigned long>(TARGET_RECORD_SEC) * 1000UL;
    return static_cast<unsigned long>(
        (static_cast<uint64_t>(pcmCapacityBytes) * 1000ULL) /
        (static_cast<uint64_t>(SAMPLE_RATE) * CHANNELS * sizeof(int16_t)));
}

inline bool timedOut() {
    return recording && (millis() - recordingStartedMs >= maxRecordMs());
}

inline void releaseBuffer() {
    if (captureBuffer) { free(captureBuffer); captureBuffer = nullptr; }
    captureCapacityBytes = pcmCapacityBytes = pcmBytes = 0;
}

inline bool ensureCaptureBuffer() {
    if (captureBuffer && pcmCapacityBytes >= MIN_PCM_BYTES) return true;
    releaseBuffer();
    const size_t freeHeap = ESP.getFreeHeap();
    const size_t largest  = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const size_t minNeeded = WAV_HEADER_BYTES + MIN_PCM_BYTES + HEAP_HEADROOM_BYTES;
    if (largest <= minNeeded) {
        Serial.printf("[Mic] buffer alloc blocked: free=%u largest=%u need>%u\n",
                      static_cast<unsigned>(freeHeap), static_cast<unsigned>(largest),
                      static_cast<unsigned>(minNeeded));
        return false;
    }
    size_t allocBytes = min(WAV_HEADER_BYTES + TARGET_PCM_BYTES, largest - HEAP_HEADROOM_BYTES);
    allocBytes &= ~static_cast<size_t>(1);
    if (allocBytes < WAV_HEADER_BYTES + MIN_PCM_BYTES) {
        Serial.printf("[Mic] usable buffer too small: %u bytes\n", static_cast<unsigned>(allocBytes));
        return false;
    }
    captureBuffer = static_cast<uint8_t *>(heap_caps_malloc(allocBytes, MALLOC_CAP_8BIT));
    if (!captureBuffer) return false;
    captureCapacityBytes = allocBytes;
    pcmCapacityBytes = captureCapacityBytes - WAV_HEADER_BYTES;
    memset(captureBuffer, 0, captureCapacityBytes);
    return true;
}

inline bool initIdleSpeakerCodec() {
    if (speakerCodecReady) return true;

    speakerCodec = es8311_create(0, ES8311_ADDRESS_0);
    if (!speakerCodec) {
        Serial.println("[ES8311] create failed");
        return false;
    }

    const es8311_clock_config_t clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = SAMPLE_RATE * 256,
        .sample_frequency = SAMPLE_RATE,
    };

    esp_err_t err = es8311_init(speakerCodec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    if (err == ESP_OK) err = es8311_sample_frequency_config(speakerCodec, clk.mclk_frequency, clk.sample_frequency);
    if (err == ESP_OK) err = es8311_voice_volume_set(speakerCodec, 0, nullptr);
    if (err == ESP_OK) err = es8311_voice_mute(speakerCodec, true);

    if (err != ESP_OK) {
        Serial.printf("[ES8311] idle init failed err=%d\n", static_cast<int>(err));
        es8311_delete(speakerCodec);
        speakerCodec = nullptr;
        return false;
    }

    speakerCodecReady = true;
    Serial.println("[ES8311] Idle speaker codec ready (muted)");
    return true;
}

// ── ES7210 + I2S init ─────────────────────────────────────────────────
inline bool begin() {
    if (initialized) return true;

    pinMode(ES8311_EN, OUTPUT);
    digitalWrite(ES8311_EN, HIGH);  // power the shared audio rail
    delay(10);

    // 1) Init ES7210 mic ADC via I2C (Wire already started by the main app)
    if (!Es7210::begin()) {
        Serial.println("[Mic] ES7210 init failed");
        return false;
    }

    // 2) Start I2S master (provides clocks to both ES7210 and ES8311)
    i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_DOUT, I2S_DIN, I2S_MCLK);
    Serial.printf("[Mic] I2S pins bclk=%d ws=%d dout=%d din=%d mclk=%d\n",
                  I2S_BCLK, I2S_LRCK, I2S_DOUT, I2S_DIN, I2S_MCLK);

    // Match the Waveshare / ESP-GMF path more closely: standard stereo I2S RX.
    if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
        Serial.println("[Mic] I2S begin failed");
        return false;
    }

    if (!initIdleSpeakerCodec()) {
        Serial.println("[Mic] ES8311 idle init failed (continuing)");
    }

    initialized = true;
    Serial.printf("[Mic] ES7210 + I2S ready, free=%u largest=%u\n",
                  static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    return true;
}

// ── Recording control ──────────────────────────────────────────────────
inline bool startRecording() {
    if (!initialized || recording) return false;
    if (!ensureCaptureBuffer()) return false;
    memset(captureBuffer, 0, captureCapacityBytes);
    pcmBytes = 0;
    wavSampleRate = SAMPLE_RATE;
    wavChannels = CHANNELS;
    recordingStartedMs = lastStatsMs = lastDataMs = millis();
    recording = true;
    Serial.println("[Mic] Recording started (ES7210 stereo)");
    return true;
}

inline bool stopRecording(uint8_t **outPcm, size_t *outBytes) {
    if (!captureBuffer) return false;
    recording = false;
    if (outPcm)   *outPcm   = captureBuffer + WAV_HEADER_BYTES;
    if (outBytes) *outBytes = pcmBytes;

    uint32_t avgL = 0, avgR = 0;
    uint16_t peakL = 0, peakR = 0;
    const int16_t *s = reinterpret_cast<const int16_t *>(captureBuffer + WAV_HEADER_BYTES);
    const size_t frames = pcmBytes / (CHANNELS * sizeof(int16_t));
    if (frames) {
        uint64_t sumL = 0, sumR = 0;
        for (size_t i = 0; i < frames; ++i) {
            uint16_t al = static_cast<uint16_t>(abs(s[i * 2 + 0]));
            uint16_t ar = static_cast<uint16_t>(abs(s[i * 2 + 1]));
            if (al > peakL) peakL = al;
            if (ar > peakR) peakR = ar;
            sumL += al; sumR += ar;
        }
        avgL = static_cast<uint32_t>(sumL / frames);
        avgR = static_cast<uint32_t>(sumR / frames);
    }
    Serial.printf("[Mic] Recording stopped: %zu bytes (%.2f s), age=%lu ms, "
                  "L avg=%u pk=%u  R avg=%u pk=%u\n",
                  pcmBytes,
                  static_cast<float>(pcmBytes) / (SAMPLE_RATE * CHANNELS * sizeof(int16_t)),
                  millis() - lastDataMs,
                  avgL, peakL, avgR, peakR);
    // Dump frames at 1 second, 2 seconds, and end for waveform diagnosis
    const size_t sec1 = min(frames, static_cast<size_t>(SAMPLE_RATE * 1));
    const size_t sec2 = min(frames, static_cast<size_t>(SAMPLE_RATE * 2));
    const size_t secEnd = frames > 10 ? frames - 10 : 0;
    auto dump10 = [&](size_t off, const char *label) {
        if (off + 10 > frames) return;
        Serial.printf("[Mic] %s: ", label);
        for (size_t i = off; i < off + 10; ++i)
            Serial.printf("%d/%d ", s[i*2], s[i*2+1]);
        Serial.println();
    };
    dump10(0,      "start");
    dump10(sec1,   "+1s");
    dump10(sec2,   "+2s");
    dump10(secEnd, "end");
    return true;
}

inline bool compactSparseFrames() {
    if (!captureBuffer || pcmBytes == 0) return false;

    const size_t frames = pcmBytes / (CHANNELS * sizeof(int16_t));
    if (frames < static_cast<size_t>(SPARSE_CYCLE_FRAMES) * 8) return false;

    int16_t *samples = reinterpret_cast<int16_t *>(captureBuffer + WAV_HEADER_BYTES);
    uint64_t energy[SPARSE_CYCLE_FRAMES] = {0};
    for (size_t i = 0; i < frames; ++i) {
        const size_t pos = i % SPARSE_CYCLE_FRAMES;
        energy[pos] += static_cast<uint32_t>(abs(samples[i * 2 + 0]));
        energy[pos] += static_cast<uint32_t>(abs(samples[i * 2 + 1]));
    }

    Serial.printf("[Mic] sparse energy");
    for (int i = 0; i < SPARSE_CYCLE_FRAMES; ++i) Serial.printf(" %d:%u", i, static_cast<unsigned>(energy[i]));
    Serial.println();

    int keepA = 0, keepB = 1;
    for (int i = 0; i < SPARSE_CYCLE_FRAMES; ++i) {
        if (energy[i] > energy[keepA]) keepA = i;
    }
    keepB = (keepA == 0) ? 1 : 0;
    for (int i = 0; i < SPARSE_CYCLE_FRAMES; ++i) {
        if (i == keepA) continue;
        if (energy[i] > energy[keepB]) keepB = i;
    }
    if (keepA > keepB) {
        const int tmp = keepA;
        keepA = keepB;
        keepB = tmp;
    }

    uint64_t topEnergy = energy[keepA] + energy[keepB];
    uint64_t totalEnergy = 0;
    for (int i = 0; i < SPARSE_CYCLE_FRAMES; ++i) totalEnergy += energy[i];
    if (topEnergy == 0 || topEnergy * 2 < totalEnergy) {
        wavSampleRate = SAMPLE_RATE;
        return false;
    }

    size_t keptFrames = 0;
    for (size_t i = 0; i < frames; ++i) {
        const int pos = i % SPARSE_CYCLE_FRAMES;
        if (pos != keepA && pos != keepB) continue;
        samples[keptFrames * 2 + 0] = samples[i * 2 + 0];
        samples[keptFrames * 2 + 1] = samples[i * 2 + 1];
        ++keptFrames;
    }

    if (keptFrames == 0 || keptFrames >= frames) {
        wavSampleRate = SAMPLE_RATE;
        return false;
    }

    pcmBytes = keptFrames * CHANNELS * sizeof(int16_t);
    wavSampleRate = static_cast<uint32_t>((static_cast<uint64_t>(SAMPLE_RATE) * keptFrames) / frames);
    if (wavSampleRate < 1000) wavSampleRate = 1000;

    Serial.printf("[Mic] compacted sparse frames keep=%d/%d frames=%u->%u wavRate=%u\n",
                  keepA, keepB,
                  static_cast<unsigned>(frames),
                  static_cast<unsigned>(keptFrames),
                  static_cast<unsigned>(wavSampleRate));
    return true;
}

inline bool downmixToMonoNormalize() {
    if (!captureBuffer || pcmBytes < CHANNELS * sizeof(int16_t)) return false;

    const size_t frames = pcmBytes / (CHANNELS * sizeof(int16_t));
    int16_t *samples = reinterpret_cast<int16_t *>(captureBuffer + WAV_HEADER_BYTES);

    int64_t sum = 0;
    for (size_t i = 0; i < frames; ++i) {
        const int32_t l = samples[i * 2 + 0];
        const int32_t r = samples[i * 2 + 1];
        sum += (l + r) / 2;
    }
    const int32_t dc = static_cast<int32_t>(sum / static_cast<int64_t>(frames));

    uint32_t peak = 0;
    uint64_t avgAbs = 0;
    for (size_t i = 0; i < frames; ++i) {
        const int32_t l = samples[i * 2 + 0];
        const int32_t r = samples[i * 2 + 1];
        const int32_t mono = ((l + r) / 2) - dc;
        const uint32_t a = static_cast<uint32_t>(abs(mono));
        if (a > peak) peak = a;
        avgAbs += a;
    }
    if (peak == 0) {
        wavChannels = 1;
        pcmBytes = frames * sizeof(int16_t);
        return false;
    }

    static constexpr float TARGET_PEAK = 24000.0f;
    static constexpr float MAX_GAIN = 6.0f;
    float gain = TARGET_PEAK / static_cast<float>(peak);
    if (gain > MAX_GAIN) gain = MAX_GAIN;

    int16_t *monoOut = samples;
    uint32_t outPeak = 0;
    uint64_t outAvgAbs = 0;
    for (size_t i = 0; i < frames; ++i) {
        const int32_t l = samples[i * 2 + 0];
        const int32_t r = samples[i * 2 + 1];
        const float centered = static_cast<float>(((l + r) / 2) - dc);
        int32_t scaled = static_cast<int32_t>(lroundf(centered * gain));
        if (scaled > 32767) scaled = 32767;
        if (scaled < -32768) scaled = -32768;
        monoOut[i] = static_cast<int16_t>(scaled);
        const uint32_t a = static_cast<uint32_t>(abs(scaled));
        if (a > outPeak) outPeak = a;
        outAvgAbs += a;
    }

    wavChannels = 1;
    pcmBytes = frames * sizeof(int16_t);
    Serial.printf("[Mic] mono normalize dc=%ld inPk=%u outPk=%u gain=%.2fx avg=%u\n",
                  static_cast<long>(dc),
                  static_cast<unsigned>(peak),
                  static_cast<unsigned>(outPeak),
                  gain,
                  static_cast<unsigned>(outAvgAbs / frames));
    return true;
}

inline bool wavPayload(uint8_t **outWav, size_t *outLen) {
    if (!captureBuffer || !outWav || !outLen || pcmBytes == 0) return false;
    WavHeader hdr;
    hdr.numChannels = wavChannels;
    hdr.sampleRate = wavSampleRate;
    hdr.byteRate = wavSampleRate * wavChannels * sizeof(int16_t);
    hdr.blockAlign = wavChannels * sizeof(int16_t);
    hdr.bitsPerSample = BITS_PER_SAMPLE;
    hdr.fileSize = 36 + pcmBytes;
    hdr.dataSize = pcmBytes;
    memcpy(captureBuffer, &hdr, sizeof(hdr));
    *outWav  = captureBuffer;
    *outLen  = sizeof(hdr) + pcmBytes;
    return true;
}

// ── Poll ───────────────────────────────────────────────────────────────
inline void poll() {
    if (!recording || !captureBuffer || pcmBytes >= pcmCapacityBytes) return;

    size_t totalRead = 0;
    for (int i = 0; i < MAX_POLL_READS_PER_CALL && pcmBytes < pcmCapacityBytes; ++i) {
        const size_t remaining = pcmCapacityBytes - pcmBytes;
        const size_t chunk     = min(READ_CHUNK_BYTES, remaining);
        const size_t got = i2s.readBytes(
            reinterpret_cast<char *>(captureBuffer + WAV_HEADER_BYTES + pcmBytes), chunk);
        if (!got) break;
        pcmBytes  += got;
        totalRead += got;
        lastDataMs = millis();
        if (got < chunk) break;
    }

    if (millis() - lastStatsMs >= 1000UL) {
        Serial.printf("[Mic] poll bytes=%zu readNow=%zu recMs=%lu cap=%zu\n",
                      pcmBytes, totalRead, millis() - recordingStartedMs, pcmCapacityBytes);
        lastStatsMs = millis();
    }
}

// ── Teardown ───────────────────────────────────────────────────────────
inline void end() {
    recording = false;
    releaseBuffer();
    if (initialized) { i2s.end(); initialized = false; }
    if (speakerCodec) {
        if (speakerCodecReady) es8311_voice_mute(speakerCodec, true);
        es8311_delete(speakerCodec);
        speakerCodec = nullptr;
        speakerCodecReady = false;
    }
    Es7210::end();
    digitalWrite(ES8311_EN, LOW);
}

}  // namespace MicAudio
