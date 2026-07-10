/**
 * ⏱️ PERCLOS 疲劳判定状态机 — 头文件
 */

#ifndef PERCLOS_H
#define PERCLOS_H

#include <stdint.h>
#include <stdbool.h>

// ── 疲劳等级枚举 ──
typedef enum {
    DMS_LEVEL_NORMAL       = 0,   // 正常
    DMS_LEVEL_2_MICROSLEEP = 2,   // 微睡眠
    DMS_LEVEL_2_YAWN       = 2,   // 哈欠（同级）
    DMS_LEVEL_3_SLEEP      = 3,   // 深度睡眠
} dms_level_t;

// ── 状态机上下文 ──
typedef struct {
    uint32_t blink_start_ms;     // 闭眼开始时刻（0=未闭眼）
    uint32_t yawn_start_ms;      // 哈欠开始时刻（0=未哈欠）
    dms_level_t level;           // 当前疲劳等级
    const char *status_desc;     // 状态描述
} perclos_state_t;

/**
 * 送入一帧 EAR/MAR 数据，更新疲劳判定状态
 */
void perclos_update(float ear, float mar, uint32_t now_ms, perclos_state_t *state);

/**
 * 重置状态机（人脸丢失时调用）
 */
void perclos_reset(perclos_state_t *state);

#endif /* PERCLOS_H */
