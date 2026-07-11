import cv2
import numpy as np
import os
import urllib.request
import mediapipe as mp
import json
import paho.mqtt.client as mqtt
import time
from mediapipe.tasks import python
from mediapipe.tasks.python import vision

# 尝试加载 .env 文件（如果 python-dotenv 可用）
try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

# ==========================================
# 🌐 车机端 MQTT 网卡初始化（配置从环境变量读取）
# ==========================================
MQTT_BROKER = os.getenv("MQTT_BROKER", "broker.emqx.io")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
TOPIC_CAR_DATA = os.getenv("MQTT_TOPIC", "dms/car/data")

# ✅ 修复: paho-mqtt v2.x 必须传入 CallbackAPIVersion
car_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
try:
    car_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    car_client.loop_start()  # 开启后台网络循环
    print("🛰️ 车机算法端：成功连接大邮局，网络管道就绪！")
except Exception as e:
    print(f"⚠️ 网络连接失败，将仅进行本地判定：{e}")

# ==========================================
# 🧠 模型初始化
# ==========================================
model_path = "face_landmarker.task"
if not os.path.exists(model_path):
    print("正在下载模型文件...")
    url = "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task"
    urllib.request.urlretrieve(url, model_path)

base_options = python.BaseOptions(model_asset_path=model_path)
options = vision.FaceLandmarkerOptions(base_options=base_options, num_faces=1)
detector = vision.FaceLandmarker.create_from_options(options)

def distance(p1, p2):
    """两点之间的欧几里得距离"""
    return np.linalg.norm(np.array(p1) - np.array(p2))

def compute_ear(eye_points):
    """
    计算单只眼睛的 Eye Aspect Ratio (EAR)
    eye_points: 6个关键点 [p0(左角), p1(上1), p2(上2), p3(右角), p4(下2), p5(下1)]
    """
    numerator = distance(eye_points[1], eye_points[5]) + distance(eye_points[2], eye_points[4])
    denominator = 2.0 * distance(eye_points[0], eye_points[3])
    if denominator < 1e-6:
        return 1.0  # 防止除零
    return numerator / denominator

def compute_mar(mouth_points):
    """
    计算 Mouth Aspect Ratio (MAR)
    mouth_points: 8个关键点
    """
    numerator = (distance(mouth_points[1], mouth_points[7]) +
                 distance(mouth_points[2], mouth_points[6]) +
                 distance(mouth_points[3], mouth_points[5]))
    denominator = 3.0 * distance(mouth_points[0], mouth_points[4])
    if denominator < 1e-6:
        return 0.0
    return numerator / denominator

# ==========================================
# 🎯 状态机与防爆冷却锁配置（✅ 时间基准，与FPS解耦）
# ==========================================
EAR_THRESHOLD = float(os.getenv("EAR_THRESHOLD", "0.18"))
MAR_THRESHOLD = float(os.getenv("MAR_THRESHOLD", "0.60"))

# ✅ 时间基准阈值（秒），不再依赖帧计数
BLINK_MICRO_SLEEP_SEC = float(os.getenv("BLINK_MICRO_SLEEP_SEC", "0.5"))   # 微睡眠阈值
BLINK_DEEP_SLEEP_SEC = float(os.getenv("BLINK_DEEP_SLEEP_SEC", "1.5"))     # 深度睡眠阈值
YAWN_DURATION_SEC = float(os.getenv("YAWN_DURATION_SEC", "2.5"))            # 连续哈欠阈值

# 💡 核心防爆锁变量
last_send_time = 0
COOLDOWN_SECONDS = int(os.getenv("COOLDOWN_SECONDS", "4"))  # 冷却时间

# ✅ 时间追踪变量（替代帧计数器）
blink_start_time = None   # 闭眼开始的时刻
yawn_start_time = None    # 哈欠开始的时刻

cap = cv2.VideoCapture(0, cv2.CAP_DSHOW)  # Windows DSHOW 驱动
if not cap.isOpened():
    # 尝试备用索引 1
    cap = cv2.VideoCapture(1, cv2.CAP_DSHOW)
if not cap.isOpened():
    print("❌ 无法打开摄像头！请检查：")
    print("   1. 摄像头是否已连接/内置摄像头是否可用")
    print("   2. 是否有其他程序占用了摄像头")
    exit(1)

print("📷 摄像头已打开")
print("====== 🚀 边缘端云联动 DMS 算法完全体启动 ======")

frame_count = 0
while cap.isOpened():
    success, frame = cap.read()
    if not success:
        frame_count += 1
        if frame_count == 1:
            print("❌ 摄像头读不到画面！检查摄像头是否被遮挡或其他程序占用")
        if frame_count > 30:
            print("❌ 连续 30 帧失败，退出")
            break
        continue
    frame_count = 0
    frame = cv2.flip(frame, 1)
    h, w, _ = frame.shape

    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
    detection_result = detector.detect(mp_image)

    current_status = "Normal"
    color = (0, 255, 0)
    fatigue_level = 0

    if detection_result.face_landmarks:
        landmarks = detection_result.face_landmarks[0]
        points = [(int(lm.x * w), int(lm.y * h)) for lm in landmarks]

        # ✅ 双眼关键点（MediaPipe Face Mesh 标准索引）
        left_eye = [points[33], points[160], points[158], points[133], points[153], points[144]]
        right_eye = [points[362], points[385], points[387], points[263], points[373], points[380]]
        mouth = [points[78], points[81], points[13], points[311], points[308], points[178], points[14], points[87]]

        # ✅ 计算双眼平均 EAR（左右眼分别计算后取平均值）
        left_ear = compute_ear(left_eye)
        right_ear = compute_ear(right_eye)
        ear = (left_ear + right_ear) / 2.0
        mar = compute_mar(mouth)

        current_time_val = time.time()

        # 🛠️ 机制一：手部遮挡豁免
        left_eye_width = distance(left_eye[0], left_eye[3])
        right_eye_width = distance(right_eye[0], right_eye[3])
        if left_eye_width < 5 or right_eye_width < 5 or ear < 0.02:
            blink_start_time = None  # 遮挡时重置闭眼计时
            current_status = "Exempt (Hand Blocked)"
            color = (255, 165, 0)
        else:
            # ✅ 时间基准闭眼判定（替代帧计数器）
            if ear < EAR_THRESHOLD:
                if blink_start_time is None:
                    blink_start_time = current_time_val  # 记录闭眼开始时刻
                else:
                    blink_duration = current_time_val - blink_start_time
                    if blink_duration >= BLINK_DEEP_SLEEP_SEC:
                        current_status = "LEVEL 3: CRITICAL!!! (Sleep)"
                        color = (0, 0, 255)
                        fatigue_level = 3
                    elif blink_duration >= BLINK_MICRO_SLEEP_SEC:
                        current_status = "LEVEL 2: Micro-sleep"
                        color = (0, 69, 255)
                        fatigue_level = 2
            else:
                blink_start_time = None  # 睁眼了，立即清零

        # ✅ 时间基准哈欠判定（替代帧计数器）
        if mar > MAR_THRESHOLD:
            if yawn_start_time is None:
                yawn_start_time = current_time_val  # 记录哈欠开始时刻
            elif current_time_val - yawn_start_time >= YAWN_DURATION_SEC:
                current_status = "LEVEL 2: Yawning Detected"
                color = (0, 165, 255)
                fatigue_level = 2
        else:
            # 嘴巴合上后延迟重置，防止哈欠中间短暂闭口
            if yawn_start_time is not None and current_time_val - yawn_start_time > 1.0:
                yawn_start_time = None

        # ==========================================
        # 🛡️ 机制二：工业级 AI 防爆过滤与通信
        # ==========================================
        current_time_ts = time.time()
        reported_level = fatigue_level  # 准备发给云端的级别

        if fatigue_level >= 2:
            if current_time_ts - last_send_time > COOLDOWN_SECONDS:
                # 冷却完毕，允许触发一次真实的 AI
                print(f"🚨 触发疲劳！精准请求云端大模型，进入 {COOLDOWN_SECONDS} 秒冷却...")
                last_send_time = current_time_ts  # 重新上锁
            else:
                # ✅ 冷却中发送 level=1 表示持续疲劳但不触发AI（原来发 0 会让仪表盘误显示"安全"）
                reported_level = 1
                remaining = int(COOLDOWN_SECONDS - (current_time_ts - last_send_time))
                current_status += f" [AI Cooling: {remaining}s]"

        # 实时发送数据给后台：每一帧都发，保证网页上的折线图永远在丝滑滚动
        payload = {
            "device_id": "ESP32_DMS_001",
            "data": {"ear": round(ear, 2), "mar": round(mar, 2)},
            "status": {"fatigue_level": reported_level, "desc": current_status}
        }
        car_client.publish(TOPIC_CAR_DATA, json.dumps(payload))

        cv2.putText(frame, current_status, (30, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)
        cv2.putText(frame, f"EAR: {ear:.2f}  MAR: {mar:.2f}", (30, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)

    cv2.imshow('DMS End-Cloud System Test', frame)
    if cv2.waitKey(5) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
car_client.loop_stop()
