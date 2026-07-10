/**
 * ============================================
 * 🧠 DMS 全功能运行模式（TEST_MODE = 0）
 * 摄像头 → 人脸检测 → EAR/MAR → PERCLOS → 蜂鸣器 + MQTT
 * ============================================
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "config.h"
#include "ear_mar.h"
#include "perclos.h"

static const char *TAG = "DMS";
static i2s_chan_handle_t audio_tx_handle = NULL;

// 前向声明
extern esp_err_t camera_init(void);
static void audio_alert(int freq_hz, int duration_ms);

// ── I2S 音频告警（播放指定频率的提示音）──
static void audio_alert(int freq_hz, int duration_ms)
{
    if (!audio_tx_handle) return;

    int sample_rate = 16000;
    int samples = sample_rate * duration_ms / 1000;
    int16_t *buf = malloc(samples * sizeof(int16_t));
    if (!buf) return;

    for (int i = 0; i < samples; i++) {
        float t = (float)i / sample_rate;
        buf[i] = (int16_t)(8000.0f * sinf(2.0f * M_PI * freq_hz * t));
    }

    size_t written = 0;
    i2s_channel_write(audio_tx_handle, buf, samples * sizeof(int16_t), &written, 1000);
    free(buf);
}

// ── I2S 音频初始化 ──
static esp_err_t audio_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &audio_tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk  = I2S_GPIO_UNUSED,
            .bclk  = I2S_BCLK_PIN,
            .ws    = I2S_LRC_PIN,
            .dout   = I2S_DIN_PIN,
            .din    = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(audio_tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(audio_tx_handle));

    if (I2S_SD_PIN >= 0) {
        gpio_config_t sd = { .pin_bit_mask = (1ULL << I2S_SD_PIN), .mode = GPIO_MODE_OUTPUT };
        gpio_config(&sd);
        gpio_set_level(I2S_SD_PIN, 1);
    }

    ESP_LOGI(TAG, "🔊 I2S 音频就绪 (BCLK=%d LRC=%d DIN=%d)", I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
    return ESP_OK;
}

/**
 * DMS 全功能主循环
 *
 * 流程：
 *   1. 摄像头采帧
 *   2. 人脸检测 → 68点关键点
 *   3. 计算 EAR/MAR
 *   4. PERCLOS 判定
 *   5. 疲劳 → I2S 音频告警 + MQTT 发 JSON
 */
void dms_full_run(void)
{
    ESP_LOGI(TAG, "🚀 DMS 全功能模式启动");

    // ── 1. 初始化摄像头 ──
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "❌ 摄像头初始化失败，DMS 无法运行");
        return;
    }

    // ── 2. 初始化 I2S 音频 ──
    if (audio_init() != ESP_OK) {
        ESP_LOGE(TAG, "❌ 音频初始化失败");
        return;
    }

    // ── 3. 初始化状态机 ──
    perclos_state_t state = {0};
    perclos_reset(&state);

    // ── 4. 初始化冷却锁 ──
    uint32_t last_alert_ms = 0;
    uint32_t alert_cooldown = COOLDOWN_MS;

    // ── 5. 状态指示灯 ──
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << STATUS_LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);
    gpio_set_level(STATUS_LED_PIN, 0);

    ESP_LOGI(TAG, "✅ 初始化完成，进入主循环...");
    ESP_LOGI(TAG, "   阈值: EAR<%.2f  MAR>%.2f", EAR_THRESHOLD, MAR_THRESHOLD);
    ESP_LOGI(TAG, "   计时: 微睡眠%dms  深度睡眠%dms  哈欠%dms",
             BLINK_MICRO_SLEEP_MS, BLINK_DEEP_SLEEP_MS, YAWN_DURATION_MS);
    ESP_LOGI(TAG, "========================================");

    uint32_t frame_count = 0;

    // ── 主循环 ──
    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        frame_count++;

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

        // ═══════════════════════════════════════
        // TODO: 廖宜乐 — 在这里接入人脸检测
        // 伪代码：
        //   point_t landmarks[68];
        //   bool face_found = esp_who_detect(fb->buf, fb->width, fb->height, landmarks);
        //
        // 当前阶段用硬编码模拟数据验证流程：
        // ═══════════════════════════════════════
        float ear = 0.30f;   // 模拟正常睁眼值
        float mar = 0.15f;   // 模拟正常闭嘴值
        bool face_found = false;

        // 每 100 帧打印一次心跳
        if (frame_count % 100 == 0) {
            ESP_LOGI(TAG, "💓 帧数=%lu, 堆空闲=%lu B",
                     frame_count, esp_get_free_heap_size());
        }

        if (!face_found) {
            perclos_reset(&state);
            gpio_set_level(STATUS_LED_PIN, 0);
            esp_camera_fb_return(fb);
            continue;
        }

        gpio_set_level(STATUS_LED_PIN, 1);  // 检测到人脸，灯亮

        // ── 手部遮挡检查 ──
        // TODO: 接入真实 landmarks
        // if (dms_is_hand_blocked(landmarks)) {
        //     perclos_reset(&state);
        //     esp_camera_fb_return(fb);
        //     continue;
        // }

        // ── 计算 EAR/MAR ──
        // TODO: 接入真实 landmarks
        // dms_compute_ear_mar(landmarks, &ear, &mar);

        // ── PERCLOS 判定 ──
        perclos_update(ear, mar, now_ms, &state);

        // ── 报警逻辑 ──
        if (state.level >= 2) {
            if (now_ms - last_alert_ms > alert_cooldown) {
                ESP_LOGW(TAG, "🚨 疲劳报警！等级=%d  描述=%s  EAR=%.2f  MAR=%.2f",
                         state.level, state.status_desc, ear, mar);

                // 不同等级不同提示音
                if (state.level == 3) {
                    audio_alert(880, 300);  // Level 3: 高音急促
                    vTaskDelay(pdMS_TO_TICKS(100));
                    audio_alert(880, 300);
                } else {
                    audio_alert(660, 500);  // Level 2: 中等提示音
                }

                last_alert_ms = now_ms;

                // TODO: MQTT 发送 JSON
                // char json_buf[256];
                // snprintf(json_buf, sizeof(json_buf),
                //     "{\"device_id\":\"ESP32_DMS_001\","
                //     "\"data\":{\"ear\":%.2f,\"mar\":%.2f},"
                //     "\"status\":{\"fatigue_level\":%d,\"desc\":\"%s (%d)\"}}",
                //     ear, mar, state.level, state.status_desc, state.level);
                // esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, json_buf, 0, 1, 0);
            }
        }

        esp_camera_fb_return(fb);
    }

    // 以下代码不会运行（while(true)），但保留作为退出示意图
    // esp_camera_deinit();
    // gpio_set_level(BUZZER_PIN, 0);
}
