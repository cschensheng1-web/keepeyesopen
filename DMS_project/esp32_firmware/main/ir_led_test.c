/**
 * ============================================
 * 💡 近红外LED PWM 驱动测试
 * 覃晖测硬件用：TEST_MODE = 3
 * ============================================
 * 接线：IR_LED_PIN → 红外LED正极（串一个 100Ω 限流电阻）
 *       GND → 红外LED负极
 * 预期：LED 从暗变亮（0→100%占空比），然后全亮 3 秒后熄灭
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "IR_LED";

void ir_led_test(void)
{
    ESP_LOGI(TAG, "💡 红外LED PWM 测试开始（GPIO %d, %dHz）", IR_LED_PIN, IR_LED_PWM_FREQ);

    // 配置 LEDC 定时器
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,   // 0-1023 分辨率
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = IR_LED_PWM_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    // 配置 LEDC 通道
    ledc_channel_config_t channel = {
        .gpio_num   = IR_LED_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&channel);

    // 渐亮效果：0 → 1023（0% → 100%）
    ESP_LOGI(TAG, "  🌅 渐亮测试...");
    for (int duty = 0; duty <= 1023; duty += 32) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 保持全亮 3 秒
    ESP_LOGI(TAG, "  🔆 全亮 3 秒（检查红外补光覆盖范围）");
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1023 * IR_LED_PWM_DUTY / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(3000));

    // 熄灭
    ESP_LOGI(TAG, "  🌑 熄灭");
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    ESP_LOGI(TAG, "✅ 红外LED测试完成！在暗处用手机相机看应能看到LED发出淡红色光");
}
