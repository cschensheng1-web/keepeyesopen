/**
 * ============================================
 * 🧮 EAR & MAR 几何算法（C 语言实现）
 * 从 PC 端 face_capture.py 一对一翻译
 * ============================================
 * 廖宜乐注意：
 *   - 输入是 esp-who 输出的 68 点 dlib 格式
 *   - 左眼索引 36-41，右眼索引 42-47，嘴索引 48-67
 *   - 如果换用其他模型，重新确认索引映射
 */

#include <math.h>
#include "ear_mar.h"

/**
 * 计算两点欧几里得距离
 */
static float distance(point_t a, point_t b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

/**
 * 计算单眼 EAR（Eye Aspect Ratio）
 *
 * eye[0] = 左眼角, eye[1] = 上眼睑1, eye[2] = 上眼睑2,
 * eye[3] = 右眼角, eye[4] = 下眼睑2, eye[5] = 下眼睑1
 *
 * EAR = (||p1-p5|| + ||p2-p4||) / (2 * ||p0-p3||)
 */
static float compute_ear_single(point_t eye[6])
{
    float numerator = distance(eye[1], eye[5]) + distance(eye[2], eye[4]);
    float denominator = 2.0f * distance(eye[0], eye[3]);
    if (denominator < 1e-6f) return 1.0f;   // 防除零
    return numerator / denominator;
}

/**
 * 计算 MAR（Mouth Aspect Ratio）
 *
 * mouth[0]=左嘴角, mouth[1]=上唇1, mouth[2]=上唇2, mouth[3]=上唇3,
 * mouth[4]=右嘴角, mouth[5]=下唇3, mouth[6]=下唇2, mouth[7]=下唇1
 *
 * MAR = (||p1-p7|| + ||p2-p6|| + ||p3-p5||) / (3 * ||p0-p4||)
 */
static float compute_mar(mouth_points_t mouth)
{
    float num = distance(mouth.p[1], mouth.p[7])
              + distance(mouth.p[2], mouth.p[6])
              + distance(mouth.p[3], mouth.p[5]);
    float den = 3.0f * distance(mouth.p[0], mouth.p[4]);
    if (den < 1e-6f) return 0.0f;
    return num / den;
}

/**
 * 从 68 点 landmarks 中提取关键点并计算 EAR + MAR
 *
 * @param landmarks_68   dlib 68 点坐标数组
 * @param ear_out        输出：双眼平均 EAR
 * @param mar_out        输出：MAR
 */
void dms_compute_ear_mar(point_t landmarks_68[68], float *ear_out, float *mar_out)
{
    // ── 提取左眼 6 点（dlib 索引 36-41）──
    point_t left_eye[6] = {
        landmarks_68[36], landmarks_68[37], landmarks_68[38],
        landmarks_68[39], landmarks_68[40], landmarks_68[41]
    };

    // ── 提取右眼 6 点（dlib 索引 42-47）──
    point_t right_eye[6] = {
        landmarks_68[42], landmarks_68[43], landmarks_68[44],
        landmarks_68[45], landmarks_68[46], landmarks_68[47]
    };

    // ── 提取嘴巴 8 点（dlib 索引 48,51,62,54,57,66,56,50）──
    //   (外唇轮廓的子集，对应 MediaPipe 的 8 点拓扑)
    mouth_points_t mouth;
    mouth.p[0] = landmarks_68[48];  // 左嘴角
    mouth.p[1] = landmarks_68[51];  // 上唇上1
    mouth.p[2] = landmarks_68[62];  // 上唇上2
    mouth.p[3] = landmarks_68[54];  // 右嘴角
    mouth.p[4] = landmarks_68[57];  // 下唇下1（中心）
    mouth.p[5] = landmarks_68[66];  // 下唇下2
    mouth.p[6] = landmarks_68[56];  // 下唇下3
    mouth.p[7] = landmarks_68[50];  // 上唇下1

    *ear_out = (compute_ear_single(left_eye) + compute_ear_single(right_eye)) / 2.0f;
    *mar_out = compute_mar(mouth);
}

/**
 * 检查是否手部遮挡（任一只眼宽度 < 5px 或 EAR 极低）
 */
bool dms_is_hand_blocked(point_t landmarks_68[68])
{
    float left_width = distance(landmarks_68[36], landmarks_68[39]);
    float right_width = distance(landmarks_68[42], landmarks_68[45]);

    // 先粗略判断眼宽度
    if (left_width < 5.0f || right_width < 5.0f) return true;

    // 再算一次 EAR 辅助判断
    float ear = 0;
    float mar = 0;
    dms_compute_ear_mar(landmarks_68, &ear, &mar);
    if (ear < 0.02f) return true;

    return false;
}
