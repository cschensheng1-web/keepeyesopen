/**
 * ESP32-CAM 配置 —— 摄像头采集 + 人脸检测 + UART 发送
 * 引脚适用于标准 ESP32-CAM 开发板（OV2640 已板载焊接）
 */
#ifndef CONFIG_H
#define CONFIG_H

// ── UART 发送（接 ESP32-S3 的 RX）──
#define UART_TX_PIN     1    // ESP32-CAM 的 TX（GPIO1）
#define UART_BAUD       115200
#define UART_NUM        1    // UART1

// ── 摄像头（ESP32-CAM 板载固定引脚，一般不用改）──
#define CAM_PIN_SIOD    21
#define CAM_PIN_SIOC    22
#define CAM_PIN_D0      2
#define CAM_PIN_D1      4
#define CAM_PIN_D2      12
#define CAM_PIN_D3      13
#define CAM_PIN_D4      15
#define CAM_PIN_D5      14
#define CAM_PIN_D6      35
#define CAM_PIN_D7      34
#define CAM_PIN_PCLK    32
#define CAM_PIN_VSYNC   5
#define CAM_PIN_HREF    27
#define CAM_PIN_XCLK    0
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1

// ── 板载 LED（ESP32-CAM 自带闪光灯，GPIO4）──
#define FLASH_LED_PIN   4

// ── 图像参数 ──
#define CAM_FRAME_WIDTH   160
#define CAM_FRAME_HEIGHT  120
#define CAM_FPS           25

#endif
