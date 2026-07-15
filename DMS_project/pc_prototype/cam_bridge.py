"""
CAM Bridge: ESP32-CAM JPEG → MediaPipe → PERCLOS → MQTT
"""
import json, time, queue, threading
import numpy as np, cv2, mediapipe as mp
import paho.mqtt.client as mqtt
import os, sys
from dotenv import load_dotenv
load_dotenv()

BROKER = "broker.emqx.io"
PORT = 1883
TOPIC_IMG = "dms/cam/img"
TOPIC_OUT = "dms/car/data"

EAR_TH = 0.18
MAR_TH = 0.60
COOLDOWN_MS = 4000

img_queue = queue.Queue(maxsize=3)

pub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
pub.connect(BROKER, PORT, 60)
pub.loop_start()

from mediapipe.tasks import python
from mediapipe.tasks.python import vision
base_options = python.BaseOptions(model_asset_path=os.path.join(os.path.dirname(__file__), 'face_landmarker.task'))
face_options = vision.FaceLandmarkerOptions(base_options=base_options, num_faces=1,
    running_mode=vision.RunningMode.IMAGE)
mp_detector = vision.FaceLandmarker.create_from_options(face_options)

def dist(a, b):
    return np.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2)

def compute_ear(pts):
    return (dist(pts[1], pts[5]) + dist(pts[2], pts[4])) / (2.0 * dist(pts[0], pts[3]) + 1e-6)

def compute_mar(pts):
    return (dist(pts[1], pts[7]) + dist(pts[2], pts[6]) + dist(pts[3], pts[5])) / (3.0 * dist(pts[0], pts[4]) + 1e-6)

def on_msg(client, userdata, msg):
    try: img_queue.put_nowait(msg.payload)
    except queue.Full: pass

def processor():
    blink_start = None; yawn_start = None; last_send = 0
    fc = 0; ff = 0
    while True:
        jpeg = img_queue.get()
        fc += 1
        try:
            img = cv2.imdecode(np.frombuffer(jpeg, dtype=np.uint8), cv2.IMREAD_COLOR)
            if img is None: continue
            h, w = img.shape[:2]

            mp_img = mp.Image(image_format=mp.ImageFormat.SRGB,
                            data=cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
            result = mp_detector.detect(mp_img)

            if not result.face_landmarks:
                if fc % 100 == 0: print(f"F#{fc} no face")
                continue

            ff += 1
            pts = [(int(lm.x * w), int(lm.y * h)) for lm in result.face_landmarks[0]]
            left_eye  = [pts[33], pts[160], pts[158], pts[133], pts[153], pts[144]]
            right_eye = [pts[362], pts[385], pts[387], pts[263], pts[373], pts[380]]
            mouth     = [pts[78], pts[81], pts[13], pts[311], pts[308], pts[178], pts[14], pts[87]]
            ear = (compute_ear(left_eye) + compute_ear(right_eye)) / 2.0
            mar = compute_mar(mouth)

            now = time.time() * 1000
            level = 0; desc = "Normal"

            if ear < EAR_TH:
                if blink_start is None: blink_start = now
                else:
                    d = now - blink_start
                    if d >= 1500: level = 3; desc = "LEVEL 3: CRITICAL!!! (Sleep)"
                    elif d >= 500: level = 2; desc = "LEVEL 2: Micro-sleep"
            else: blink_start = None

            if mar > MAR_TH:
                if yawn_start is None: yawn_start = now
                elif now - yawn_start >= 2500: level = 2; desc = "LEVEL 2: Yawning Detected"
            else:
                if yawn_start and now - yawn_start > 1000: yawn_start = None

            reported = level
            if level >= 2:
                if time.time() - last_send < 4.0: reported = 1
                else: last_send = time.time()

            out = {"device_id":"ESP32_CAM","data":{"ear":round(ear,2),"mar":round(mar,2)},
                   "status":{"fatigue_level":reported,"desc":desc}}
            pub.publish(TOPIC_OUT, json.dumps(out))

            if fc % 10 == 0:
                print(f"F#{fc} face={ff} E={ear:.2f} M={mar:.2f} Lv={reported}")

        except Exception as e:
            print(f"ERR: {e}", file=sys.stderr)

threading.Thread(target=processor, daemon=True).start()

def on_connect(client, userdata, flags, rc, p):
    client.subscribe(TOPIC_IMG)

sub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
sub.on_connect = on_connect
sub.on_message = on_msg
sub.connect(BROKER, PORT, 60)
print(f"CAM→{BROKER}→MediaPipe→{TOPIC_OUT}")
sub.loop_forever()
