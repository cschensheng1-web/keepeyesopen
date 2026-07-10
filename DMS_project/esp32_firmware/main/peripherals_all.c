/**
 * ============================================
 * 🔄 全外设综合测试（TEST_MODE = 5）
 * 覃晖一次性验证所有硬件
 * ============================================
 * 顺序：摄像头拍照 → 蜂鸣器响 → 红外LED渐变 → 综合运行
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "ALL_TEST";

extern void camera_test(void);
extern void audio_test(void);
extern void ir_led_test(void);
extern void wifi_mqtt_test(void);

void all_peripherals_test(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  全外设综合测试开始");
    ESP_LOGI(TAG, "  请依次观察以下现象：");
    ESP_LOGI(TAG, "  1. 串口输出摄像头信息");
    ESP_LOGI(TAG, "  2. 听到 3 声不同音高的测试音（MAX98357）");
    ESP_LOGI(TAG, "  3. 红外LED渐亮渐灭");
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "\n▶▶▶ 阶段 1/3：摄像头测试\n");
    camera_test();

    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "\n▶▶▶ 阶段 2/3：MAX98357 音频测试\n");
    audio_test();

    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "\n▶▶▶ 阶段 3/3：红外LED测试\n");
    ir_led_test();

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "✅ 全外设综合测试完成！");
    ESP_LOGI(TAG, "   如果以上 3 项都正常，硬件就绪");
    ESP_LOGI(TAG, "   如果有任何一项失败，检查对应接线");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "\n💡 下一步：改为 TEST_MODE=4 测试 WiFi+MQTT");
}
