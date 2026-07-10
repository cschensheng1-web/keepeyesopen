/**
 * ============================================
 * 🔊 MAX98357 I2S 音频放大器驱动测试
 * 覃晖测硬件用：TEST_MODE = 2
 * ============================================
 * 接线：ESP32-S3          MAX98357
 *       GPIO 14 (BCLK) ── BCLK
 *       GPIO 19 (LRC)  ── LRC (WS)
 *       GPIO 22 (DIN)  ── DIN
 *       GPIO 23 (SD)   ── SD（或外部拉到3.3V）
 *       3.3V           ── VIN
 *       GND            ── GND
 *       MAX98357 OUT   ── 喇叭 (4Ω/8Ω)
 *
 * 预期：听到 3 声 "滴——" 不同频率的测试音
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "AUDIO";

#define SAMPLE_RATE     16000    // 16kHz 采样率（语音常用）
#define TONE_DURATION_MS 800     // 单次测试音持续时长
#define TONE_PAUSE_MS    500     // 间隔

// ── 生成 16-bit PCM 正弦波到缓冲区 ──
static void generate_tone(int16_t *buf, int samples, float freq_hz, float amplitude)
{
    for (int i = 0; i < samples; i++) {
        float t = (float)i / SAMPLE_RATE;
        buf[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * freq_hz * t));
    }
}

void audio_test(void)
{
    ESP_LOGI(TAG, "🔊 MAX98357 I2S 音频测试开始");

    // ── 1. 配置 SD 引脚（shutdown，拉高使能）──
    if (I2S_SD_PIN >= 0) {
        gpio_config_t sd_conf = {
            .pin_bit_mask = (1ULL << I2S_SD_PIN),
            .mode         = GPIO_MODE_OUTPUT,
        };
        gpio_config(&sd_conf);
        gpio_set_level(I2S_SD_PIN, 1);   // 拉高 = 正常工作
        ESP_LOGI(TAG, "  SD 引脚 GPIO %d 已拉高使能", I2S_SD_PIN);
    }

    // ── 2. 初始化 I2S 标准模式 ──
    i2s_chan_handle_t tx_handle = NULL;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk       = I2S_GPIO_UNUSED,   // MAX98357 不需要 MCLK
            .bclk       = I2S_BCLK_PIN,
            .ws         = I2S_LRC_PIN,
            .dout        = I2S_DIN_PIN,
            .din         = I2S_GPIO_UNUSED,   // 只播放不录音
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    ESP_LOGI(TAG, "  I2S 初始化成功: BCLK=%d LRC=%d DIN=%d 采样率=%dHz",
             I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN, SAMPLE_RATE);

    // ── 3. 生成并播放 3 个不同频率的测试音 ──
    int tone_samples = SAMPLE_RATE * TONE_DURATION_MS / 1000;
    int16_t *buffer = malloc(tone_samples * sizeof(int16_t));
    if (!buffer) {
        ESP_LOGE(TAG, "❌ 内存分配失败！");
        goto cleanup;
    }

    float test_freqs[] = {440.0f, 660.0f, 880.0f};  // A4, E5, A5
    const char *notes[]  = {"A4 (440Hz)", "E5 (660Hz)", "A5 (880Hz)"};

    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "  🎵 播放 %d/3: %s", i + 1, notes[i]);
        generate_tone(buffer, tone_samples, test_freqs[i], 12000.0f);

        size_t bytes_written = 0;
        ESP_ERROR_CHECK(i2s_channel_write(tx_handle, buffer,
                        tone_samples * sizeof(int16_t), &bytes_written, 1000));

        if (i < 2) {
            vTaskDelay(pdMS_TO_TICKS(TONE_PAUSE_MS));
        }
    }

    free(buffer);
    ESP_LOGI(TAG, "✅ 音频测试完成！如果听到 3 声不同音高的声音，MAX98357 正常");

cleanup:
    ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));
    ESP_ERROR_CHECK(i2s_del_channel(tx_handle));
    if (I2S_SD_PIN >= 0) {
        gpio_set_level(I2S_SD_PIN, 0);  // 休眠
    }
}
