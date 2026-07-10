/**
 * ============================================
 * 🚀 DMS 疲劳驾驶检测 — ESP32-S3 固件主程序
 * 全国大学生物联网设计竞赛 · 不闭眼战车队
 * ============================================
 * 编译：idf.py build
 * 烧录：idf.py -p COMx flash monitor
 * 配置：idf.py menuconfig
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "config.h"

// 测试模块声明
void camera_test(void);
void audio_test(void);
void ir_led_test(void);
void wifi_mqtt_test(void);
void all_peripherals_test(void);

// DMS 全功能模块声明
void dms_full_run(void);

static const char *TAG = "DMS_MAIN";

void app_main(void)
{
    // 初始化 NVS（WiFi 用）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  不闭眼战车 DMS 固件启动");
    ESP_LOGI(TAG, "  ESP32-S3 + OV2640 近红外");
    ESP_LOGI(TAG, "  TEST_MODE = %d", TEST_MODE);
    ESP_LOGI(TAG, "========================================");

    // ── 根据 TEST_MODE 选择运行模式 ──
    switch (TEST_MODE) {
        case 0:
            ESP_LOGI(TAG, "▶ 全功能 DMS 运行模式");
            dms_full_run();
            break;
        case 1:
            ESP_LOGI(TAG, "▶ 摄像头测试模式");
            ESP_LOGI(TAG, "  预期：串口输出分辨率、FPS、首帧图像信息");
            camera_test();
            break;
        case 2:
            ESP_LOGI(TAG, "▶ MAX98357 音频测试模式");
            ESP_LOGI(TAG, "  预期：听到 3 声不同音高的测试音（440/660/880Hz）");
            audio_test();
            break;
        case 3:
            ESP_LOGI(TAG, "▶ 红外LED测试模式");
            ESP_LOGI(TAG, "  预期：LED 从暗到亮渐变，观察补光效果");
            ir_led_test();
            break;
        case 4:
            ESP_LOGI(TAG, "▶ WiFi + MQTT 连接测试模式");
            ESP_LOGI(TAG, "  预期：连上 WiFi → 连上 MQTT Broker → 发测试消息");
            wifi_mqtt_test();
            break;
        case 5:
            ESP_LOGI(TAG, "▶ 全外设综合测试模式");
            ESP_LOGI(TAG, "  预期：摄像头+蜂鸣器+LED 同时验证");
            all_peripherals_test();
            break;
        default:
            ESP_LOGE(TAG, "❌ 未知 TEST_MODE=%d，请检查 config.h", TEST_MODE);
            break;
    }
}
