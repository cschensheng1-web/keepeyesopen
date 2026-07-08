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

# ==========================================
# 🌐 车机端 MQTT 网卡初始化
# ==========================================
MQTT_BROKER = "broker.emqx.io"
MQTT_PORT = 1883
TOPIC_CAR_DATA = "dms/car/data"

car_client = mqtt.Client()
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
    return np.linalg.norm(np.array(p1) - np.array(p2))

# ==========================================
# 🎯 状态机与防爆冷却锁配置
# ==========================================
blink_counter = 0  
yawn_counter = 0   
EAR_THRESHOLD = 0.14  
MAR_THRESHOLD = 0.50  

BLINK_FRAME_LIMIT = 15   
SLEEP_FRAME_LIMIT = 45   
YAWN_FRAME_LIMIT = 60    

# 💡 核心防爆锁变量
last_send_time = 0  
COOLDOWN_SECONDS = 4  # 冷却时间：4秒内绝对不触发第二次大模型

cap = cv2.VideoCapture(0)
print("====== 🚀 边缘端云联动 DMS 算法完全体启动 ======")

while cap.isOpened():
    success, frame = cap.read()
    if not success: break
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
        
        left_eye = [points[33], points[160], points[158], points[133], points[153], points[144]]
        mouth = [points[78], points[81], points[13], points[311], points[308], points[178], points[14], points[87]]
        
        ear = (distance(left_eye[1], left_eye[5]) + distance(left_eye[2], left_eye[4])) / (2.0 * distance(left_eye[0], left_eye[3]))
        mar = (distance(mouth[1], mouth[7]) + distance(mouth[2], mouth[6]) + distance(mouth[3], mouth[5])) / (3.0 * distance(mouth[0], mouth[4]))
        
        # 🛠️ 机制一：手部遮挡豁免
        if distance(left_eye[0], left_eye[3]) < 5 or ear < 0.02:
            blink_counter = 0
            current_status = "Exempt (Hand Blocked)"
            color = (255, 165, 0)
        else:
            if ear < EAR_THRESHOLD: blink_counter += 1
            else: blink_counter = 0
                
        # 🛠️ 机制二：时间积分过滤吃东西
        if mar > MAR_THRESHOLD: yawn_counter += 1
        else: yawn_counter = max(0, yawn_counter - 2)

        # 🛠️ 机制三：多级报警状态机
        if blink_counter >= SLEEP_FRAME_LIMIT:
            current_status = "LEVEL 3: CRITICAL!!! (Sleep)"
            color = (0, 0, 255)
            fatigue_level = 3
        elif blink_counter >= BLINK_FRAME_LIMIT:
            current_status = "LEVEL 2: Micro-sleep"
            color = (0, 69, 255)
            fatigue_level = 2
        elif yawn_counter >= YAWN_FRAME_LIMIT:
            current_status = "LEVEL 2: Yawning Detected"
            color = (0, 165, 255)
            fatigue_level = 2
            
        # ==========================================
        # 🛡️ 机制四：工业级 AI 防爆过滤与通信（保证折线图不断，但拦截疯狂的AI触发）
        # ==========================================
        current_time = time.time()
        reported_level = fatigue_level  # 准备发给云端的级别
        
        if fatigue_level >= 2:
            if current_time - last_send_time > COOLDOWN_SECONDS:
                # 冷却完毕，允许触发一次真实的 AI
                print(f"🚨 触发疲劳！精准请求云端大模型，进入 {COOLDOWN_SECONDS} 秒冷却...")
                last_send_time = current_time  # 重新上锁
            else:
                # 仍在冷却中！强制将云端级别降为 0，防止后台大模型乱叫
                reported_level = 0
                remaining = int(COOLDOWN_SECONDS - (current_time - last_send_time))
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
    if cv2.waitKey(5) & 0xFF == ord('q'): break

cap.release()
cv2.destroyAllWindows()
car_client.loop_stop()