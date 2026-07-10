/**
 * 🧮 EAR & MAR 算法头文件
 */

#ifndef EAR_MAR_H
#define EAR_MAR_H

#include <stdbool.h>
#include <stdint.h>

// ── 二维点 ──
typedef struct {
    float x;
    float y;
} point_t;

// ── 嘴部 8 点（与 PC 端保持同一拓扑）──
typedef struct {
    point_t p[8];
} mouth_points_t;

/**
 * 从 68 点 landmarks 计算双眼平均 EAR 和 MAR
 * @param landmarks_68  dlib 格式的人脸 68 个关键点
 * @param ear_out       输出 EAR 值
 * @param mar_out       输出 MAR 值
 */
void dms_compute_ear_mar(point_t landmarks_68[68], float *ear_out, float *mar_out);

/**
 * 判断是否发生手部遮挡
 * @param landmarks_68  dlib 格式 68 点
 * @return true = 遮挡，应跳过此帧
 */
bool dms_is_hand_blocked(point_t landmarks_68[68]);

#endif /* EAR_MAR_H */
