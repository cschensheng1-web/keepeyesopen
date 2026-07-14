/**
 * ESP32-CAM 配置 —— AI-Thinker ESP32-CAM 标准引脚
 * OV2640 板载焊接，引脚固定无法改
 */
#ifndef CONFIG_H
#define CONFIG_H

// ── UART 发送（接 ESP32-S3 的 RX）──
#define UART_TX_PIN     1
#define UART_BAUD       115200
#define UART_NUM        1

// ── 摄像头（AI-Thinker ESP32-CAM 固定引脚）──
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D0      5    // Y2
#define CAM_PIN_D1      18   // Y3
#define CAM_PIN_D2      19   // Y4
#define CAM_PIN_D3      21   // Y5
#define CAM_PIN_D4      36   // Y6
#define CAM_PIN_D5      39   // Y7
#define CAM_PIN_D6      34   // Y8
#define CAM_PIN_D7      35   // Y9
#define CAM_PIN_PCLK    22
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_XCLK    0
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1

// ── 板载闪光灯 ──
#define FLASH_LED_PIN   4

// ── 图像参数 ──
#define CAM_FRAME_WIDTH   160
#define CAM_FRAME_HEIGHT  120
#define CAM_FPS           25

#endif
