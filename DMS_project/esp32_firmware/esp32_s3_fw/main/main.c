/**
 * ESP32-S3 执行器 —— MQTT订阅 → I2S语音 + OLED显示 + IR LED
 * I2C OLED: SDA=GPIO10 SCL=GPIO11 (4针 SSD1306 128x64)
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "config.h"
#include "alert_sleep.h"
#include "alert_yawn.h"
#include "alert_wake.h"
#include "alert_rest.h"
#include "alert_safe.h"

static const char *TAG = "S3";
static i2s_chan_handle_t i2s_h;
static i2c_master_dev_handle_t oled_dev;
static uint32_t last_alert = 0;

/* ========== I2C OLED SSD1306 基础驱动 ========== */
#define OLED_ADDR  0x3C
#define OLED_WIDTH  128
#define OLED_HEIGHT  64

static void oled_cmd(uint8_t cmd){
    uint8_t buf[2] = {0x00, cmd};
    i2c_master_transmit(oled_dev, buf, 2, -1);
}
static void oled_data(uint8_t *d, int len){
    uint8_t *buf = malloc(len + 1);
    buf[0] = 0x40;
    memcpy(buf + 1, d, len);
    i2c_master_transmit(oled_dev, buf, len + 1, -1);
    free(buf);
}
static void oled_init(void){
    i2c_master_bus_config_t bc = {.clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0, .scl_io_num = 11, .sda_io_num = 10,
        .glitch_ignore_cnt = 7, .flags = {.enable_internal_pullup = true}};
    i2c_master_bus_handle_t bus;
    i2c_new_master_bus(&bc, &bus);
    i2c_device_config_t dc = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_ADDR, .scl_speed_hz = 100000};
    i2c_master_bus_add_device(bus, &dc, &oled_dev);

    // SSD1306 初始化序列
    uint8_t init[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF,
        0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
    for (int i = 0; i < sizeof(init); i++) oled_cmd(init[i]);
    ESP_LOGI(TAG, "OLED OK");
}
static void oled_clear(void){
    for (int p = 0; p < 8; p++) {
        oled_cmd(0xB0 + p); oled_cmd(0x00); oled_cmd(0x10);
        uint8_t z[128] = {0};
        oled_data(z, 128);
    }
}
// 6x8 字体，ASCII 32-95
static const uint8_t font6x8[][6] = {
    {0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x00,0x00,0x5F,0x00,0x00,0x00}, // !
    {0x00,0x07,0x00,0x07,0x00,0x00}, // "
    {0x14,0x7F,0x14,0x7F,0x14,0x00}, // #
    {0x24,0x2A,0x7F,0x2A,0x12,0x00}, // $
    {0x23,0x13,0x08,0x64,0x62,0x00}, // %
    {0x36,0x49,0x55,0x22,0x50,0x00}, // &
    {0x00,0x05,0x03,0x00,0x00,0x00}, // '
    {0x00,0x1C,0x22,0x41,0x00,0x00}, // (
    {0x00,0x41,0x22,0x1C,0x00,0x00}, // )
    {0x08,0x2A,0x1C,0x2A,0x08,0x00}, // *
    {0x08,0x08,0x3E,0x08,0x08,0x00}, // +
    {0x00,0x50,0x30,0x00,0x00,0x00}, // ,
    {0x08,0x08,0x08,0x08,0x08,0x00}, // -
    {0x00,0x60,0x60,0x00,0x00,0x00}, // .
    {0x20,0x10,0x08,0x04,0x02,0x00}, // /
    {0x3E,0x51,0x49,0x45,0x3E,0x00}, // 0
    {0x00,0x42,0x7F,0x40,0x00,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46,0x00}, // 2
    {0x21,0x41,0x45,0x4B,0x31,0x00}, // 3
    {0x18,0x14,0x12,0x7F,0x10,0x00}, // 4
    {0x27,0x45,0x45,0x45,0x39,0x00}, // 5
    {0x3C,0x4A,0x49,0x49,0x30,0x00}, // 6
    {0x01,0x71,0x09,0x05,0x03,0x00}, // 7
    {0x36,0x49,0x49,0x49,0x36,0x00}, // 8
    {0x06,0x49,0x49,0x29,0x1E,0x00}, // 9
    {0x00,0x36,0x36,0x00,0x00,0x00}, // :
    {0x00,0x56,0x36,0x00,0x00,0x00}, // ;
    {0x00,0x08,0x14,0x22,0x41,0x00}, // <
    {0x14,0x14,0x14,0x14,0x14,0x00}, // =
    {0x41,0x22,0x14,0x08,0x00,0x00}, // >
    {0x02,0x01,0x51,0x09,0x06,0x00}, // ?
    {0x32,0x49,0x79,0x41,0x3E,0x00}, // @
    {0x7E,0x11,0x11,0x11,0x7E,0x00}, // A
    {0x7F,0x49,0x49,0x49,0x36,0x00}, // B
    {0x3E,0x41,0x41,0x41,0x22,0x00}, // C
    {0x7F,0x41,0x41,0x22,0x1C,0x00}, // D
    {0x7F,0x49,0x49,0x49,0x41,0x00}, // E
    {0x7F,0x09,0x09,0x01,0x01,0x00}, // F
    {0x3E,0x41,0x41,0x51,0x32,0x00}, // G
    {0x7F,0x08,0x08,0x08,0x7F,0x00}, // H
    {0x00,0x41,0x7F,0x41,0x00,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01,0x00}, // J
    {0x7F,0x08,0x14,0x22,0x41,0x00}, // K
    {0x7F,0x40,0x40,0x40,0x40,0x00}, // L
    {0x7F,0x02,0x04,0x02,0x7F,0x00}, // M
    {0x7F,0x04,0x08,0x10,0x7F,0x00}, // N
    {0x3E,0x41,0x41,0x41,0x3E,0x00}, // O
    {0x7F,0x09,0x09,0x09,0x06,0x00}, // P
    {0x3E,0x41,0x51,0x21,0x5E,0x00}, // Q
    {0x7F,0x09,0x19,0x29,0x46,0x00}, // R
    {0x46,0x49,0x49,0x49,0x31,0x00}, // S
    {0x01,0x01,0x7F,0x01,0x01,0x00}, // T
    {0x3F,0x40,0x40,0x40,0x3F,0x00}, // U
    {0x1F,0x20,0x40,0x20,0x1F,0x00}, // V
    {0x7F,0x20,0x18,0x20,0x7F,0x00}, // W
    {0x63,0x14,0x08,0x14,0x63,0x00}, // X
    {0x03,0x04,0x78,0x04,0x03,0x00}, // Y
    {0x61,0x51,0x49,0x45,0x43,0x00}, // Z
};

static void oled_char(int x, int y, char c){
    if (c < 32 || c > 95) c = 32;
    oled_cmd(0xB0 + y); oled_cmd(0x00 + ((x*6) & 0x0F)); oled_cmd(0x10 + ((x*6) >> 4));
    oled_data((uint8_t*)font6x8[c-32], 6);
}
static void oled_str(int x, int y, const char *s){
    while (*s) { oled_char(x, y, *s++); x++; if (x > 20) break; }
}
static void oled_show(int line, const char *s){
    oled_str(0, line, "                    "); // clear line
    oled_str(0, line, s);
}

/* ========== I2S 音频 ========== */
static void audio_init(void){
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&cc, &i2s_h, NULL);
    i2s_std_config_t sc = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = I2S_BCLK_PIN, .ws = I2S_LRC_PIN,
                     .dout = I2S_DIN_PIN, .din = I2S_GPIO_UNUSED}
    };
    i2s_channel_init_std_mode(i2s_h, &sc); i2s_channel_enable(i2s_h);
    gpio_set_direction(I2S_SD_PIN, GPIO_MODE_OUTPUT); gpio_set_level(I2S_SD_PIN, 1);
}
static void beep(int hz, int ms){
    if (!i2s_h) return;
    int n = 16000 * ms / 1000;
    int16_t *b = malloc(n * 2);
    for (int i = 0; i < n; i++) b[i] = (int16_t)(6000 * sinf(2 * M_PI * hz * i / 16000.0f));
    size_t w; i2s_channel_write(i2s_h, b, n * 2, &w, 1000); free(b);
}

/* ========== WiFi + MQTT ========== */
static EventGroupHandle_t evt;
static void on_wifi_start(void *a, esp_event_base_t b, int32_t id, void *d){ esp_wifi_connect(); }
static void on_wifi_ip(void *a, esp_event_base_t b, int32_t id, void *d){
    ip_event_got_ip_t *ip = d;
    ESP_LOGI(TAG, "WiFi:" IPSTR, IP2STR(&ip->ip_info.ip));
    xEventGroupSetBits(evt, 1);
}
static void on_mqtt_ok(void *a, esp_event_base_t b, int32_t id, void *d){
    ESP_LOGI(TAG, "MQTT OK, subscribe %s", MQTT_TOPIC);
    esp_mqtt_client_subscribe((esp_mqtt_client_handle_t)a, MQTT_TOPIC, 0);
}
static void on_mqtt_data(void *a, esp_event_base_t b, int32_t id, void *d){
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)d;
    char buf[256] = {0};
    memcpy(buf, e->data, e->data_len < 255 ? e->data_len : 255);

    int level = 0;
    char *lp = strstr(buf, "\"fatigue_level\":");
    if (lp) level = atoi(lp + 16);

    float ear = 0, mar = 0;
    char *ep = strstr(buf, "\"ear\":");
    char *mp = strstr(buf, "\"mar\":");
    if (ep) ear = strtof(ep + 6, NULL);
    if (mp) mar = strtof(mp + 6, NULL);

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (level >= 2 && (now - last_alert) > COOLDOWN_MS) {
        last_alert = now;
        ESP_LOGW(TAG, "FATIGUE L%d E=%.2f M=%.2f", level, ear, mar);

        /* 三声预警 */
        for(int i=0;i<3;i++){ beep(880,150); vTaskDelay(100); }
        vTaskDelay(200);

        /* 语音播报 */
        /* 多句轮播不重复 */
        static int s_idx=0, y_idx=0;
        size_t w;
        if (level == 3) {
            oled_show(1, "!!! DEEP SLEEP !!!");
            oled_show(3, "WAKE UP!");
            /* 3句轮换 */
            int idx = s_idx++ % 3;
            if(idx==0) i2s_channel_write(i2s_h, alert_sleep, ALERT_SLEEP_LEN, &w, portMAX_DELAY);
            else if(idx==1) i2s_channel_write(i2s_h, alert_wake, ALERT_WAKE_LEN, &w, portMAX_DELAY);
            else i2s_channel_write(i2s_h, alert_safe, ALERT_SAFE_LEN, &w, portMAX_DELAY);
        } else if (strstr(buf, "Yawning")) {
            oled_show(1, "YAWNING DETECTED");
            oled_show(3, "Take a break~");
            /* 2句轮换 */
            int idx = y_idx++ % 2;
            if(idx==0) i2s_channel_write(i2s_h, alert_yawn, ALERT_YAWN_LEN, &w, portMAX_DELAY);
            else i2s_channel_write(i2s_h, alert_rest, ALERT_REST_LEN, &w, portMAX_DELAY);
        } else {
            oled_show(1, "MICRO SLEEP");
            oled_show(3, "Stay awake!");
            int idx = s_idx++ % 3;
            if(idx==0) i2s_channel_write(i2s_h, alert_sleep, ALERT_SLEEP_LEN, &w, portMAX_DELAY);
            else if(idx==1) i2s_channel_write(i2s_h, alert_wake, ALERT_WAKE_LEN, &w, portMAX_DELAY);
            else i2s_channel_write(i2s_h, alert_safe, ALERT_SAFE_LEN, &w, portMAX_DELAY);
        }
    } else if (level == 0) {
        oled_show(1, "Driver: NORMAL");
    }
    oled_show(5, buf);
}

void app_main(void){
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED_PIN, 1);
    ESP_LOGI(TAG, "=== S3 DMS Actuator ===");

    oled_init();
    oled_clear();
    oled_show(0, "DMS SYSTEM READY");
    oled_show(2, "Waiting...");

    audio_init();

    ledc_timer_config_t lt = {.speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT,
                              .timer_num = LEDC_TIMER_0, .freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&lt);
    ledc_channel_config_t lc = {.gpio_num = IR_LED_PIN, .speed_mode = LEDC_LOW_SPEED_MODE,
                                .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0, .duty = 800};
    ledc_channel_config(&lc);

    nvs_flash_init(); esp_netif_init(); esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wc = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wc);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, on_wifi_start, NULL, NULL);
    evt = xEventGroupCreate();
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_ip, NULL, NULL);
    wifi_config_t wcfg = {.sta = {.ssid = WIFI_SSID, .password = WIFI_PASSWORD}};
    esp_wifi_set_mode(WIFI_MODE_STA); esp_wifi_set_config(WIFI_IF_STA, &wcfg); esp_wifi_start();
    xEventGroupWaitBits(evt, 1, pdTRUE, pdTRUE, 20000);

    esp_mqtt_client_config_t mc = {.broker.address.uri = MQTT_BROKER_URL};
    esp_mqtt_client_handle_t mq = esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(mq, MQTT_EVENT_CONNECTED, on_mqtt_ok, mq);
    esp_mqtt_client_register_event(mq, MQTT_EVENT_DATA, on_mqtt_data, NULL);
    esp_mqtt_client_start(mq);

    oled_show(2, "MQTT connected!");
    ESP_LOGI(TAG, "Ready");

    while (1) {
        gpio_set_level(STATUS_LED_PIN, 0); vTaskDelay(3000);
        gpio_set_level(STATUS_LED_PIN, 1); vTaskDelay(100);
    }
}
