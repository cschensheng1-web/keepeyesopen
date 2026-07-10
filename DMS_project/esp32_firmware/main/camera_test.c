/**
 * ============================================
 * 📷 OV2640 近红外摄像头驱动测试
 * 覃晖测硬件用：TEST_MODE = 1（单独）或 5（综合）
 * ============================================
 * 预期：初始化摄像头 → 连续拍照 → 串口输出分辨率、FPS
 *
 * 注意：如果编译报错找不到 "esp_camera.h"，
 * 需要在项目根目录执行：
 *   git submodule add https://github.com/espressif/esp32-camera.git components/esp32-camera
 * 或者手动克隆到 components/ 目录
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "config.h"

// esp32-camera 组件头文件
#include "esp_camera.h"

static const char *TAG = "CAMERA";

// ── 摄像头引脚配置表 ──
static camera_config_t camera_config = {
    .pin_pwdn    = CAM_PIN_PWDN,
    .pin_reset   = CAM_PIN_RESET,
    .pin_xclk    = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,   // I2C SDA
    .pin_sccb_scl = CAM_PIN_SIOC,   // I2C SCL

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,

    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href  = CAM_PIN_HREF,
    .pin_pclk  = CAM_PIN_PCLK,

    .xclk_freq_hz  = 20000000,      // 20MHz 主时钟
    .ledc_timer    = LEDC_TIMER_1,  // 与 IR LED 的 TIMER_0 错开
    .ledc_channel  = LEDC_CHANNEL_1,

    .pixel_format  = PIXFORMAT_GRAYSCALE,  // 近红外下用灰度图，省内存
    .frame_size    = FRAMESIZE_QVGA,       // 320x240

    .jpeg_quality  = 12,    // JPEG 质量（0-63，越小越清晰）
    .fb_count       = 1,    // 单缓冲（省内存）
    .grab_mode      = CAMERA_GRAB_WHEN_EMPTY,
};

esp_err_t camera_init(void)
{
    ESP_LOGI(TAG, "📷 初始化 OV2640 近红外摄像头...");

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ 摄像头初始化失败！错误码: 0x%x", err);
        ESP_LOGE(TAG, "   常见原因：");
        ESP_LOGE(TAG, "   1. 杜邦线接触不良 → 逐根重插");
        ESP_LOGE(TAG, "   2. I2C 引脚不对 → 检查 CAM_PIN_SIOD/SIOC");
        ESP_LOGE(TAG, "   3. 供电不足 → 确保 5V/1A 以上供电");
        ESP_LOGE(TAG, "   4. 摄像头模块本身坏 → 换一个试试");
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "❌ 获取传感器句柄失败");
        return ESP_FAIL;
    }

    // 配置传感器参数（近红外场景优化）
    s->set_brightness(s, 0);      // 亮度
    s->set_contrast(s, 2);        // 对比度
    s->set_saturation(s, -2);     // 红外图不需要饱和度
    s->set_whitebal(s, 0);        // 关白平衡（红外场景）
    s->set_awb_gain(s, 0);        // 关自动白平衡
    s->set_exposure_ctrl(s, 1);   // 开自动曝光
    s->set_gain_ctrl(s, 1);       // 开自动增益
    s->set_quality(s, 10);        // 图像质量

    ESP_LOGI(TAG, "✅ 摄像头初始化成功！");
    ESP_LOGI(TAG, "   PID: 0x%02X  VER: 0x%02X  MIDL: 0x%02X",
             s->id.PID, s->id.VER, s->id.MIDL);
    ESP_LOGI(TAG, "   分辨率: %d x %d", CAM_FRAME_WIDTH, CAM_FRAME_HEIGHT);

    return ESP_OK;
}

void camera_test(void)
{
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "❌ 摄像头测试中止");
        return;
    }

    ESP_LOGI(TAG, "📸 开始连续采集（30秒），观察串口日志...");
    ESP_LOGI(TAG, "   提示：用手遮摄像头看图像数据变化");

    uint32_t frame_count = 0;
    TickType_t start_time = xTaskGetTickCount();
    TickType_t test_end = start_time + pdMS_TO_TICKS(30000);

    while (xTaskGetTickCount() < test_end) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "❌ 获取帧失败！检查摄像头连接");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        frame_count++;

        // 每 30 帧打印一次统计
        if (frame_count % 30 == 0) {
            TickType_t now = xTaskGetTickCount();
            float elapsed_sec = (float)(now - start_time) * portTICK_PERIOD_MS / 1000.0f;
            float fps = (elapsed_sec > 0) ? (frame_count / elapsed_sec) : 0;
            ESP_LOGI(TAG, "  📊 帧数=%lu, FPS=%.1f, 图像大小=%zu B, 时间=%ld ms",
                     frame_count, fps, fb->len, fb->timestamp.tv_sec);
        }

        esp_camera_fb_return(fb);

        // 打印首帧提示
        if (frame_count == 1) {
            ESP_LOGI(TAG, "  ✅ 首帧获取成功！摄像头硬件正常");
            ESP_LOGI(TAG, "  💡 用手机相机对准红外LED，应该能看到淡红色光");
        }
    }

    TickType_t now = xTaskGetTickCount();
    float total_sec = (float)(now - start_time) * portTICK_PERIOD_MS / 1000.0f;
    float avg_fps = (total_sec > 0) ? (frame_count / total_sec) : 0;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ 摄像头测试完成！");
    ESP_LOGI(TAG, "   总帧数: %lu", frame_count);
    ESP_LOGI(TAG, "   总时间: %.1f 秒", total_sec);
    ESP_LOGI(TAG, "   平均帧率: %.1f FPS", avg_fps);
    ESP_LOGI(TAG, "   (目标帧率: %d FPS)", CAM_FPS);
    ESP_LOGI(TAG, "========================================");

    esp_camera_deinit();
}
