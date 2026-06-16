#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"

#define TAG "groqwatch_idf_audio"

#define BOOT_BUTTON_PIN              GPIO_NUM_0
#define SAMPLE_RATE_HZ               16000
#define BITS_PER_SAMPLE              16
#define CHANNELS                     2
#define CHUNK_MS                     20
#define CHUNK_FRAMES                 ((SAMPLE_RATE_HZ * CHUNK_MS) / 1000)
#define WINDOW_MS                    400
#define WINDOW_CHUNKS                (WINDOW_MS / CHUNK_MS)
#define TONE_HZ                      880.0f
#define SPEAKER_VOL_PERCENT          70.0f
#define MIC_GAIN_DB                  30.0f
#define PREROLL_SILENCE_CHUNKS       8
#define WINDOW_COUNT                 8
#define CHUNK_SAMPLES                (CHUNK_FRAMES * CHANNELS)

typedef struct {
    uint64_t sum_abs_l;
    uint64_t sum_abs_r;
    uint16_t peak_l;
    uint16_t peak_r;
    size_t frames;
    size_t read_errors;
    size_t write_errors;
} window_stats_t;

static esp_codec_dev_handle_t s_play_dev = NULL;
static esp_codec_dev_handle_t s_mic_dev = NULL;
static bool s_audio_ready = false;
static bool s_boot_prev = false;
static float s_phase = 0.0f;

static int16_t s_tx_buf[CHUNK_SAMPLES];
static int16_t s_rx_buf[CHUNK_SAMPLES];

static void fill_tx_chunk(bool tone_on)
{
    const float phase_step = 2.0f * (float)M_PI * TONE_HZ / (float)SAMPLE_RATE_HZ;
    for (int i = 0; i < CHUNK_FRAMES; ++i) {
        int16_t sample = 0;
        if (tone_on) {
            sample = (int16_t)(sinf(s_phase) * 12000.0f);
            s_phase += phase_step;
            if (s_phase > 2.0f * (float)M_PI) {
                s_phase -= 2.0f * (float)M_PI;
            }
        }
        s_tx_buf[i * 2 + 0] = sample;
        s_tx_buf[i * 2 + 1] = sample;
    }
}

static void reset_stats(window_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
}

static void accumulate_rx_stats(window_stats_t *stats, const int16_t *samples, size_t frame_count)
{
    for (size_t i = 0; i < frame_count; ++i) {
        const int16_t l = samples[i * 2 + 0];
        const int16_t r = samples[i * 2 + 1];
        const uint16_t abs_l = (uint16_t)abs(l);
        const uint16_t abs_r = (uint16_t)abs(r);
        stats->sum_abs_l += abs_l;
        stats->sum_abs_r += abs_r;
        if (abs_l > stats->peak_l) stats->peak_l = abs_l;
        if (abs_r > stats->peak_r) stats->peak_r = abs_r;
    }
    stats->frames += frame_count;
}

static void print_window_stats(int window_idx, bool tone_on, const window_stats_t *stats)
{
    const uint32_t avg_l = stats->frames ? (uint32_t)(stats->sum_abs_l / stats->frames) : 0;
    const uint32_t avg_r = stats->frames ? (uint32_t)(stats->sum_abs_r / stats->frames) : 0;
    ESP_LOGI(TAG,
             "window=%d tone=%s frames=%u avgL=%u peakL=%u avgR=%u peakR=%u readErr=%u writeErr=%u",
             window_idx,
             tone_on ? "ON" : "OFF",
             (unsigned)stats->frames,
             avg_l,
             stats->peak_l,
             avg_r,
             stats->peak_r,
             (unsigned)stats->read_errors,
             (unsigned)stats->write_errors);
}

static esp_err_t init_audio_duplex(void)
{
    if (s_audio_ready) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Waveshare BSP audio devices");

    s_play_dev = bsp_audio_codec_speaker_init();
    ESP_RETURN_ON_FALSE(s_play_dev != NULL, ESP_FAIL, TAG, "speaker init failed");

    s_mic_dev = bsp_audio_codec_microphone_init();
    ESP_RETURN_ON_FALSE(s_mic_dev != NULL, ESP_FAIL, TAG, "microphone init failed");

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE_HZ,
        .channel = CHANNELS,
        .bits_per_sample = BITS_PER_SAMPLE,
    };

    ESP_RETURN_ON_ERROR(esp_codec_dev_open(s_play_dev, &fs), TAG, "speaker open failed");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(s_play_dev, SPEAKER_VOL_PERCENT), TAG, "speaker volume failed");
    ESP_LOGI(TAG, "Speaker opened first; priming clocks with silence");

    memset(s_tx_buf, 0, sizeof(s_tx_buf));
    for (int i = 0; i < PREROLL_SILENCE_CHUNKS; ++i) {
        esp_err_t err = esp_codec_dev_write(s_play_dev, s_tx_buf, sizeof(s_tx_buf));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "speaker preroll write %d failed: %s", i, esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    ESP_RETURN_ON_ERROR(esp_codec_dev_open(s_mic_dev, &fs), TAG, "mic open failed");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_in_gain(s_mic_dev, MIC_GAIN_DB), TAG, "mic gain failed");
    ESP_LOGI(TAG, "Mic opened after speaker clock preroll");

    s_audio_ready = true;
    return ESP_OK;
}

static void run_audio_probe_once(void)
{
    if (init_audio_duplex() != ESP_OK) {
        ESP_LOGE(TAG, "Audio init failed; aborting probe");
        return;
    }

    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "Starting full-duplex tone/silence probe");
    ESP_LOGI(TAG, "Speak near the mic during the test. Tone windows should raise mic energy.");

    for (int w = 0; w < WINDOW_COUNT; ++w) {
        const bool tone_on = (w % 2) == 0;
        window_stats_t stats;
        reset_stats(&stats);

        for (int c = 0; c < WINDOW_CHUNKS; ++c) {
            fill_tx_chunk(tone_on);

            esp_err_t write_err = esp_codec_dev_write(s_play_dev, s_tx_buf, sizeof(s_tx_buf));
            if (write_err != ESP_OK) {
                ++stats.write_errors;
            }

            esp_err_t read_err = esp_codec_dev_read(s_mic_dev, s_rx_buf, sizeof(s_rx_buf));
            if (read_err != ESP_OK) {
                ++stats.read_errors;
                memset(s_rx_buf, 0, sizeof(s_rx_buf));
            }

            accumulate_rx_stats(&stats, s_rx_buf, CHUNK_FRAMES);
        }

        print_window_stats(w, tone_on, &stats);
        vTaskDelay(pdMS_TO_TICKS(60));
    }

    ESP_LOGI(TAG, "Probe pass complete. Press BOOT to run it again.");
    ESP_LOGI(TAG, "============================================================");
}

void app_main(void)
{
    ESP_LOGI(TAG, "GroqWatch ESP-IDF audio probe boot");
    ESP_LOGI(TAG, "Using BSP-managed full duplex path (speaker first, then mic)");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    vTaskDelay(pdMS_TO_TICKS(1500));
    run_audio_probe_once();

    while (1) {
        bool boot_now = gpio_get_level(BOOT_BUTTON_PIN) == 0;
        if (boot_now && !s_boot_prev) {
            run_audio_probe_once();
        }
        s_boot_prev = boot_now;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
