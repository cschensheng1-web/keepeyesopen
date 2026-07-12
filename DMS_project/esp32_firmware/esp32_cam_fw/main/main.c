/**
 * ESP32-CAM 固件：摄像头捕获 → esp-who人脸检测 → UART发送关键点坐标
 * 发给 ESP32-S3 算力板
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "CAM";

static camera_config_t cam_cfg = {
    .pin_pwdn=CAM_PIN_PWDN, .pin_reset=CAM_PIN_RESET, .pin_xclk=CAM_PIN_XCLK,
    .pin_sccb_sda=CAM_PIN_SIOD, .pin_sccb_scl=CAM_PIN_SIOC,
    .pin_d7=CAM_PIN_D7, .pin_d6=CAM_PIN_D6, .pin_d5=CAM_PIN_D5, .pin_d4=CAM_PIN_D4,
    .pin_d3=CAM_PIN_D3, .pin_d2=CAM_PIN_D2, .pin_d1=CAM_PIN_D1, .pin_d0=CAM_PIN_D0,
    .pin_vsync=CAM_PIN_VSYNC, .pin_href=CAM_PIN_HREF, .pin_pclk=CAM_PIN_PCLK,
    .xclk_freq_hz=20000000, .ledc_timer=LEDC_TIMER_0, .ledc_channel=LEDC_CHANNEL_0,
    .pixel_format=PIXFORMAT_GRAYSCALE, .frame_size=FRAMESIZE_QVGA,
    .jpeg_quality=10, .fb_count=1, .grab_mode=CAMERA_GRAB_WHEN_EMPTY,
};

void app_main(void)
{
    // 1. 摄像头初始化
    if (esp_camera_init(&cam_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败，无限重试...");
        while(1) { vTaskDelay(1000); esp_camera_init(&cam_cfg); }
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_brightness(s, 0);
    s->set_contrast(s, 2);
    s->set_whitebal(s, 0);
    ESP_LOGI(TAG, "摄像头OK PID=0x%02X %dx%d", s->id.PID, CAM_FRAME_WIDTH, CAM_FRAME_HEIGHT);

    // 2. UART 初始化
    uart_config_t uart_cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM, 2048, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_cfg);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI(TAG, "UART%d TX=GPIO%d 波特率=%d 已就绪", UART_NUM, UART_TX_PIN, UART_BAUD);

    // 3. 闪灯指示（连拍 3 次）
    gpio_set_direction(FLASH_LED_PIN, GPIO_MODE_OUTPUT);
    for (int i=0; i<3; i++) { gpio_set_level(FLASH_LED_PIN,1); vTaskDelay(100); gpio_set_level(FLASH_LED_PIN,0); vTaskDelay(100); }
    ESP_LOGI(TAG, "开始采集，以二进制帧发送...");

    uint8_t frame_id = 0;
    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) { vTaskDelay(10); continue; }

        // TODO: 廖宜乐接入 esp-who 人脸检测，得到 68 点坐标
        // point_t landmarks[68];
        // bool ok = face_detect(fb->buf, fb->width, fb->height, landmarks);

        // 当前：发灰度图前 200 字节作为占位测试（覃晖验证 UART 链路）
        uint8_t header[3] = {0xFF, 0xAA, frame_id++};
        uart_write_bytes(UART_NUM, (const char*)header, 3);
        uart_write_bytes(UART_NUM, (const char*)fb->buf, fb->len > 200 ? 200 : fb->len);

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(66));  // ~15 FPS
    }
}
