/**
 * ============================================
 * ⏱️ PERCLOS 疲劳判定状态机（时间基准）
 * 参照 PC 端 dms_logic.py + face_capture.py
 * ============================================
 */

#include <stdint.h>
#include <stdbool.h>
#include "perclos.h"
#include "config.h"

/**
 * 疲劳判定主函数
 *
 * @param ear   当前帧的双眼平均 EAR
 * @param mar   当前帧的 MAR
 * @param now_ms 当前时间戳（毫秒），用 xTaskGetTickCount() * portTICK_PERIOD_MS
 * @param state 状态机上下文（由调用方分配并维护）
 */
void perclos_update(float ear, float mar, uint32_t now_ms, perclos_state_t *state)
{
    // ── 闭眼判定 ──
    if (ear < EAR_THRESHOLD) {
        if (state->blink_start_ms == 0) {
            state->blink_start_ms = now_ms;  // 刚开始闭眼
        } else {
            uint32_t duration = now_ms - state->blink_start_ms;
            if (duration >= BLINK_DEEP_SLEEP_MS) {
                state->level = DMS_LEVEL_3_SLEEP;
                state->status_desc = "LEVEL 3: CRITICAL!!! (Deep Sleep)";
            } else if (duration >= BLINK_MICRO_SLEEP_MS) {
                state->level = DMS_LEVEL_2_MICROSLEEP;
                state->status_desc = "LEVEL 2: Micro-sleep";
            }
        }
    } else {
        state->blink_start_ms = 0;  // 睁眼立即清零
    }

    // ── 哈欠判定 ──
    if (mar > MAR_THRESHOLD) {
        if (state->yawn_start_ms == 0) {
            state->yawn_start_ms = now_ms;
        } else if (now_ms - state->yawn_start_ms >= YAWN_DURATION_MS) {
            state->level = DMS_LEVEL_2_YAWN;
            state->status_desc = "LEVEL 2: Yawning Detected";
        }
    } else {
        // 闭嘴后延迟 1 秒再重置（防哈欠中途短暂闭口）
        if (state->yawn_start_ms != 0 &&
            now_ms - state->yawn_start_ms > 1000) {
            state->yawn_start_ms = 0;
        }
    }
}

/**
 * 重置状态（人脸丢失/遮挡时调用）
 */
void perclos_reset(perclos_state_t *state)
{
    state->blink_start_ms = 0;
    state->yawn_start_ms = 0;
    state->level = DMS_LEVEL_NORMAL;
    state->status_desc = "Normal";
}
