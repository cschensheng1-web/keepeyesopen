"""
桥接脚本：订阅 ESP32-CAM 的 dms/car/raw → PERCLOS 判定 → 发布 dms/car/data
与 face_capture.py 输出格式完全一致，web_backend 和 S3 无需修改
"""
import json
import time
import paho.mqtt.client as mqtt
import os

from dotenv import load_dotenv
load_dotenv()

BROKER = os.getenv("MQTT_BROKER", "broker.emqx.io")
PORT = int(os.getenv("MQTT_PORT", "1883"))
TOPIC_RAW = "dms/car/raw"
TOPIC_ALERT = "dms/car/data"

# PERCLOS 参数
EAR_TH = float(os.getenv("EAR_THRESHOLD", "0.18"))
MAR_TH = float(os.getenv("MAR_THRESHOLD", "0.60"))
BLINK_MICRO_MS = float(os.getenv("BLINK_MICRO_SLEEP_SEC", "0.5")) * 1000
BLINK_DEEP_MS = float(os.getenv("BLINK_DEEP_SLEEP_SEC", "1.5")) * 1000
YAWN_MS = float(os.getenv("YAWN_DURATION_SEC", "2.5")) * 1000
COOLDOWN_S = int(os.getenv("COOLDOWN_SECONDS", "4"))

blink_start = None
yawn_start = None
last_send = 0

pub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
pub.connect(BROKER, PORT, 60)
pub.loop_start()

def on_msg(client, userdata, msg):
    global blink_start, yawn_start, last_send
    try:
        data = json.loads(msg.payload.decode())
        ear = data["ear"]
        mar = data["mar"]
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
        pub.publish(TOPIC_ALERT, json.dumps(out))

    except Exception as e:
        pass

sub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
sub.on_connect = lambda c, u, f, rc, p: c.subscribe(TOPIC_RAW)
sub.on_message = on_msg
sub.connect(BROKER, PORT, 60)
print(f"Bridge: {TOPIC_RAW} → PERCLOS → {TOPIC_ALERT}")
print(f"Broker: {BROKER}:{PORT}")
sub.loop_forever()
