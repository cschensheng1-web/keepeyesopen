/**
 * ============================================
 * 🔧 硬件引脚定义 & 全局配置
 * 团队：不闭眼战车队
 * ============================================
 * 👤 覃晖（硬件）：
 *    - 根据实际接线修改引脚宏
 *    - 改 TEST_MODE 逐项验证硬件
 *    - 改 WIFI_SSID / WIFI_PASSWORD / MQTT_BROKER_URL
 *
 * 👤 廖宜乐（算法）：
 *    - 阈值在下面 EAR_THRESHOLD / MAR_THRESHOLD 等
 *    - 算法逻辑在 ear_mar.c / perclos.c / dms_algorithm.c
 *    - 不用动这里，等覃晖说硬件就绪再开始写算法
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==========================================
// 📌 OV9732 摄像头引脚（DVP 8-bit 并行接口，esp32-camera 通过 I2C 自动识别）
// ==========================================
#define CAM_PIN_SIOD     21    // I2C SDA
#define CAM_PIN_SIOC     20    // I2C SCL

#define CAM_PIN_D0       11    // DVP D0
#define CAM_PIN_D1        9    // DVP D1
#define CAM_PIN_D2        8    // DVP D2
#define CAM_PIN_D3       10    // DVP D3
#define CAM_PIN_D4       12    // DVP D4
#define CAM_PIN_D5       18    // DVP D5
#define CAM_PIN_D6       17    // DVP D6
#define CAM_PIN_D7       16    // DVP D7

#define CAM_PIN_PCLK     13    // Pixel Clock
#define CAM_PIN_VSYNC     6    // Vertical Sync
#define CAM_PIN_HREF      7    // Horizontal Reference
#define CAM_PIN_XCLK     15    // Master Clock (20MHz)

#define CAM_PIN_PWDN     -1    // Power Down（如模块无此引脚填 -1）
#define CAM_PIN_RESET    -1    // Reset（如模块无此引脚填 -1）

// ==========================================
// 📌 MAX98357 I2S 音频放大器（语音播报）
// ==========================================
#define I2S_BCLK_PIN      14    // I2S 位时钟
#define I2S_LRC_PIN       19    // I2S 左右通道时钟 (WS)
#define I2S_DIN_PIN       22    // I2S 数据输入
#define I2S_SD_PIN        23    //  shutdown（-1=不接, 外部拉到3.3V）

// ==========================================
// 📌 其他执行器引脚
// ==========================================
#define IR_LED_PIN         5    // 红外补光 LED（PWM 调光）
#define STATUS_LED_PIN    48    // 板载状态指示灯（ESP32-S3 常用 48）

// ==========================================
// 📌 PWM 参数
// ==========================================
#define IR_LED_PWM_FREQ  5000  // PWM 频率 5kHz
#define IR_LED_PWM_DUTY   80   // 默认占空比 80%（太亮会过热，可调）

// ==========================================
// 📌 摄像头图像参数
// ==========================================
#define CAM_FRAME_WIDTH   320  // QQVGA（先低分辨率保证帧率）
#define CAM_FRAME_HEIGHT  240
#define CAM_FPS           25   // 目标帧率

// ==========================================
// 📌 疲劳检测算法阈值
// ==========================================
#define EAR_THRESHOLD         0.18f   // 闭眼阈值
#define MAR_THRESHOLD         0.60f   // 哈欠阈值
#define BLINK_MICRO_SLEEP_MS  500     // 微睡眠：闭眼 > 0.5秒
#define BLINK_DEEP_SLEEP_MS   1500    // 深度睡眠：闭眼 > 1.5秒
#define YAWN_DURATION_MS      2500    // 哈欠：张嘴 > 2.5秒
#define COOLDOWN_MS           4000    // AI 触发冷却时间

// ==========================================
// 📌 WiFi & MQTT 配置
// ==========================================
#define WIFI_SSID           "your_wifi_ssid"
#define WIFI_PASSWORD       "your_wifi_password"
#define MQTT_BROKER_URL     "mqtt://192.168.1.100"  // 队长的 MQTT Broker 地址
#define MQTT_TOPIC          "dms/car/data"

// ==========================================
// 📌 测试模式开关（覃晖测硬件时用）
// ==========================================
// 编译时选择测试模式：
//   0 = 全功能 DMS 运行模式
//   1 = 摄像头测试（拍照 + 串口输出分辨率/FPS）
//   2 = 蜂鸣器测试（响1秒 → 停1秒 → 循环5次）
//   3 = 红外LED测试（PWM 0→100% 渐变）
//   4 = WiFi + MQTT 连接测试
//   5 = 全外设综合测试（摄像头→串口 + 蜂鸣器 + LED 同时跑）
#define TEST_MODE  1   // <--- 覃晖从这里改！

#endif /* CONFIG_H */
