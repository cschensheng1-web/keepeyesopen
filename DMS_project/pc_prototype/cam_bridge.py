"""
CAM Bridge: 订阅 ESP32-CAM 的 JPEG 图像 → MediaPipe 人脸分析 → 疲劳判定 → MQTT
CAM 拍的照片真正进入算法管道，USB 摄像头不需要了
"""
import json
import time
import numpy as np
import cv2
import mediapipe as mp
import paho.mqtt.client as mqtt
import os
from dotenv import load_dotenv
load_dotenv()

BROKER = os.getenv("MQTT_BROKER", "broker.emqx.io")
PORT = int(os.getenv("MQTT_PORT", "1883"))
TOPIC_IMG = "dms/cam/img"
TOPIC_OUT = "dms/car/data"

# PERCLOS 参数
EAR_TH = float(os.getenv("EAR_THRESHOLD", "0.18"))
MAR_TH = float(os.getenv("MAR_THRESHOLD", "0.60"))
BLINK_MICRO_MS = float(os.getenv("BLINK_MICRO_SLEEP_SEC", "0.5")) * 1000
BLINK_DEEP_MS = float(os.getenv("BLINK_DEEP_SLEEP_SEC", "1.5")) * 1000
YAWN_MS = float(os.getenv("YAWN_DURATION_SEC", "2.5")) * 1000
COOLDOWN_S = int(os.getenv("COOLDOWN_SECONDS", "4"))

# MediaPipe 新版 API
from mediapipe.tasks import python
from mediapipe.tasks.python import vision
base_options = python.BaseOptions(model_asset_path='face_landmarker.task')
face_options = vision.FaceLandmarkerOptions(base_options=base_options, num_faces=1,
    running_mode=vision.RunningMode.IMAGE)
mp_detector = vision.FaceLandmarker.create_from_options(face_options)

# 状态
blink_start = None
yawn_start = None
last_send = 0
frame_count = 0

pub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
pub.connect(BROKER, PORT, 60)
pub.loop_start()

def compute_ear(pts):
    def dist(a, b):
        return np.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2)
    return (dist(pts[1], pts[5]) + dist(pts[2], pts[4])) / (2.0 * dist(pts[0], pts[3]) + 1e-6)

def compute_mar(pts):
    def dist(a, b):
        return np.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2)
    return (dist(pts[1], pts[7]) + dist(pts[2], pts[6]) + dist(pts[3], pts[5])) / (3.0 * dist(pts[0], pts[4]) + 1e-6)

def on_msg(client, userdata, msg):
    global blink_start, yawn_start, last_send, frame_count
    try:
        jpg = np.frombuffer(msg.payload, dtype=np.uint8)
        img = cv2.imdecode(jpg, cv2.IMREAD_COLOR)
        if img is None:
            return
        h, w, _ = img.shape
        frame_count += 1

        mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
        result = mp_detector.detect(mp_img)

        if not result.face_landmarks:
            return

        pts = [(int(lm.x * w), int(lm.y * h)) for lm in result.face_landmarks[0]]

        left_eye  = [pts[33], pts[160], pts[158], pts[133], pts[153], pts[144]]
        right_eye = [pts[362], pts[385], pts[387], pts[263], pts[373], pts[380]]
        mouth     = [pts[78], pts[81], pts[13], pts[311], pts[308], pts[178], pts[14], pts[87]]

        ear = (compute_ear(left_eye) + compute_ear(right_eye)) / 2.0
        mar = compute_mar(mouth)

        now = time.time() * 1000
        level = 0
        desc = "Normal"

        # 闭眼判定
        if ear < EAR_TH:
            if blink_start is None:
                blink_start = now
            else:
                d = now - blink_start
                if d >= BLINK_DEEP_MS:
                    level = 3; desc = "LEVEL 3: CRITICAL!!! (Sleep)"
                elif d >= BLINK_MICRO_MS:
                    level = 2; desc = "LEVEL 2: Micro-sleep"
        else:
            blink_start = None

        # 哈欠判定
        if mar > MAR_TH:
            if yawn_start is None:
                yawn_start = now
            elif now - yawn_start >= YAWN_MS:
                level = 2; desc = "LEVEL 2: Yawning Detected"
        else:
            if yawn_start and now - yawn_start > 1000:
                yawn_start = None

        # 冷却
        reported = level
        if level >= 2:
            if time.time() - last_send < COOLDOWN_S:
                reported = 1
            else:
                last_send = time.time()

        out = {
            "device_id": "ESP32_CAM",
            "data": {"ear": round(ear, 2), "mar": round(mar, 2)},
            "status": {"fatigue_level": reported, "desc": desc}
        }
        pub.publish(TOPIC_OUT, json.dumps(out))

        if frame_count % 30 == 0:
            print(f"F#{frame_count} EAR={ear:.2f} MAR={mar:.2f} Lv={reported} {desc}")

    except Exception as e:
        pass

sub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
sub.on_connect = lambda c, u, f, rc, p: c.subscribe(TOPIC_IMG)
sub.on_message = on_msg
sub.connect(BROKER, PORT, 60)
print(f"CAM Bridge: {TOPIC_IMG} → MediaPipe → {TOPIC_OUT}")
print(f"Broker: {BROKER}:{PORT}")
print("等待 ESP32-CAM 发图...")
sub.loop_forever()
