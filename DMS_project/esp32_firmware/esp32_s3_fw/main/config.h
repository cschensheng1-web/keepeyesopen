/**
 * ESP32-S3 算力板配置 —— UART接收 → EAR/MAR → PERCLOS → I2S音频 + MQTT + IR LED
 */
#ifndef CONFIG_H
#define CONFIG_H

// ── UART 接收（接 ESP32-CAM 的 TX）──
#define UART_RX_PIN     17
#define UART_TX_PIN     18    // 预留（调试用，不用接）
#define UART_BAUD       115200
#define UART_NUM        1

// ── MAX98357 I2S 音频 ──
#define I2S_BCLK_PIN    14
#define I2S_LRC_PIN     4
#define I2S_DIN_PIN     5
#define I2S_SD_PIN      6

// ── 红外 LED ──
#define IR_LED_PIN      7
#define STATUS_LED_PIN  48    // ESP32-S3 板载 LED

// ── WiFi / MQTT ──
#define WIFI_SSID       "MIFI_3319"
#define WIFI_PASSWORD   "88888888"
#define MQTT_BROKER_URL "mqtt://192.168.0.101"
#define MQTT_TOPIC      "dms/car/data"

// ── 疲劳阈值 ──
#define EAR_THRESHOLD         0.18f
#define MAR_THRESHOLD         0.60f
#define BLINK_MICRO_SLEEP_MS  500
#define BLINK_DEEP_SLEEP_MS   1500
#define YAWN_DURATION_MS      2500
#define COOLDOWN_MS           4000
#define ALERT_COOLDOWN_MS      4000
#define YAWN_RECOVERY_MS       1000
#define PERCLOS_WINDOW_MS      30000
#define DMS_PERCLOS_MAX_INTERVALS  32
#define PERCLOS_MIN_VALID_OBSERVATION_MS  5000
#define PERCLOS_ALERT_RATIO    0.3f
#define PERCLOS_MAX_OBSERVATION_GAP_MS  2000

// ── 测试模式 ──
// 0=全功能 1=UART收+串口打印 2=I2S音频 3=IR LED 4=WiFi+MQTT 5=综合
#define TEST_MODE  0

#endif
