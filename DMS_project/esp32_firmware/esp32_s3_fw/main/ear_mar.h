#ifndef EAR_MAR_H
#define EAR_MAR_H
#include <stdbool.h>
#include <stdint.h>

typedef struct { float x; float y; } point_t;
typedef struct { point_t p[8]; } mouth_points_t;

/**
 * 从68点landmarks计算EAR+MAR
 * @param landmarks  [in]  dlib 68点坐标
 * @param ear_out    [out] 双眼平均EAR
 * @param mar_out    [out] MAR
 */
void dms_compute_ear_mar(point_t landmarks[68], float *ear_out, float *mar_out);
bool dms_is_hand_blocked(point_t landmarks[68]);
#endif
