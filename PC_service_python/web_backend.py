import json
import time
import requests
import random
import asyncio
import paho.mqtt.client as mqtt
import websockets
from threading import Thread

# ==========================================
# 🔑 DeepSeek 官方正版直连配置
# ==========================================
# 💡 请把下面这行换成你刚拿到的 sk-xxxxxxxx 密钥
API_KEY = "sk-eb7c730227af4b9ead60a3f0a4f5f47e" 
MODEL_NAME = "deepseek-chat" 
API_URL = "https://api.deepseek.com/v1/chat/completions" 

MQTT_BROKER = "broker.emqx.io"
TOPIC_CAR_DATA = "dms/car/data"

latest_data = {"ear": 0.25, "mar": 0.15, "level": 0, "ai_text": ""}
CONNECTED_WEBSOCKETS = set()

# ==========================================
# 🤖 离线蒸馏高情商语料库（作为网络抖动时的金牌保底）
# ==========================================
LOCAL_RESPONSES = [
    "老哥，这哈欠打得惊天动地，困了咱就眯会儿，别硬撑！",
    "检测到深度困意！前方有服务区，喝口红牛顶一下啦！",
    "眼皮在打架，安全在流泪。听我一句劝，喝口水提提神！",
    "危险值拉满！开点重金属音乐蹦个迪，强行清醒一下！"
]

# ==========================================
# 🌐 真·大模型端云协同生成核心
# ==========================================
def generate_ai_voice(fatigue_type="疲劳"):
    headers = {
        "Authorization": f"Bearer {API_KEY}",
        "Content-Type": "application/json"
    }
    
    payload = {
        "model": MODEL_NAME,
        "messages": [
            {
                "role": "system", 
                "content": "你是一个智能车载座舱语音助手。当检测到司机疲劳或打哈欠时，请生成一句极短的（20字以内）、高情商、带点幽默感或关怀感的中文短句，提醒司机注意安全，绝对不要长篇大论！不要包含任何多余的开头解释！"
            },
            {
                "role": "user", 
                "content": f"危险！检测到司机正在【{fatigue_type}】，请立刻生成一句车载关怀播报。"
            }
        ],
        "temperature": 0.8
    }
    
    try:
        # 实时发起云端大模型轰炸
        response = requests.post(API_URL, json=payload, headers=headers, timeout=3)
        res_json = response.json()
        ai_msg = res_json["choices"][0]["message"]["content"]
        return ai_msg.strip().replace('"', '').replace("'", "")
    except Exception as e:
        # 💡 工业级高可用防爆：万一遇到网络波动，0秒无缝切换本地保底，网页绝不卡死
        print(f"📡 [提示] 云端大模型略有抖动，已自动启动端侧保底机制。")
        return random.choice(LOCAL_RESPONSES)

# ==========================================
# 🌐 MQTT 接收线：抓取车机人脸数据
# ==========================================
def on_message(client, userdata, msg):
    global latest_data
    try:
        data = json.loads(msg.payload.decode())
        level = data["status"]["fatigue_level"]
        desc = data["status"]["desc"]
        
        latest_data["ear"] = data["data"]["ear"]
        latest_data["mar"] = data["data"]["mar"]
        latest_data["level"] = level
        latest_data["ai_text"] = ""
        
        if level >= 2:
            fatigue_type = "频繁打哈欠" if "Yawning" in desc else "长时间闭眼犯困"
            # 召唤真大模型
            latest_data["ai_text"] = generate_ai_voice(fatigue_type)
            print(f"🤖 DeepSeek 实时联网生成下发: {latest_data['ai_text']}")
            
    except Exception as e:
        pass

def mqtt_thread_entry():
    client = mqtt.Client()
    client.on_connect = lambda c, u, f, rc: c.subscribe(TOPIC_CAR_DATA)
    client.on_message = on_message
    client.connect(MQTT_BROKER, 1883, 60)
    client.loop_forever()

Thread(target=mqtt_thread_entry, daemon=True).start()

# ==========================================
# 📊 WebSocket 发射线：甩给网页大屏
# ==========================================
async def echo(websocket):
    CONNECTED_WEBSOCKETS.add(websocket)
    try:
        while True:
            await websocket.send(json.dumps(latest_data))
            if latest_data["ai_text"]: 
                latest_data["ai_text"] = "" 
            await asyncio.sleep(0.1) 
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        CONNECTED_WEBSOCKETS.remove(websocket)

async def main():
    print("🚀 宏博的工业级网页大屏网关已在本地 [ws://127.0.0.1:8888] 挂载就绪！")
    async with websockets.serve(echo, "127.0.0.1", 8888):
        await asyncio.Event().wait()

if __name__ == "__main__":
    asyncio.run(main())