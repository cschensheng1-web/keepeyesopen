import json
import time
import requests
import random
import asyncio
import os
import paho.mqtt.client as mqtt
import websockets
from threading import Thread
from concurrent.futures import ThreadPoolExecutor

# 尝试加载 .env 文件（如果 python-dotenv 可用）
try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

# ==========================================
# 🔑 DeepSeek 配置（✅ 密钥从环境变量读取，不再硬编码）
# ==========================================
API_KEY = os.getenv("DEEPSEEK_API_KEY", "")
MODEL_NAME = os.getenv("DEEPSEEK_MODEL", "deepseek-chat")
API_URL = os.getenv("DEEPSEEK_API_URL", "https://api.deepseek.com/v1/chat/completions")

MQTT_BROKER = os.getenv("MQTT_BROKER", "broker.emqx.io")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
TOPIC_CAR_DATA = os.getenv("MQTT_TOPIC", "dms/car/data")

latest_data = {"ear": 0.25, "mar": 0.15, "level": 0, "ai_text": ""}
CONNECTED_WEBSOCKETS = set()

# ==========================================
# 🔊 TTS 语音引擎初始化（✅ 新增离线语音播报，驾驶员真正能听到）
# ==========================================
tts_enabled = False
try:
    import pyttsx3
    tts_engine = pyttsx3.init()
    tts_engine.setProperty('rate', 200)   # 语速适中
    tts_engine.setProperty('volume', 1.0)  # 最大音量
    tts_enabled = True
    print("🔊 TTS 语音引擎初始化成功（离线播报就绪）")
except ImportError:
    print("⚠️ pyttsx3 未安装，TTS 语音播报不可用。安装: pip install pyttsx3")
except Exception as e:
    print(f"⚠️ TTS 初始化失败: {e}")

def speak_text(text):
    """在独立线程中播报语音，不阻塞主流程"""
    if not tts_enabled:
        return
    try:
        tts_engine.say(text)
        tts_engine.runAndWait()
    except Exception as e:
        print(f"⚠️ TTS 播报失败: {e}")

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
    if not API_KEY:
        print("⚠️ 未设置 DEEPSEEK_API_KEY 环境变量，使用本地语料库")
        return random.choice(LOCAL_RESPONSES)

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
        response = requests.post(API_URL, json=payload, headers=headers, timeout=3)
        res_json = response.json()
        ai_msg = res_json["choices"][0]["message"]["content"]
        return ai_msg.strip().replace('"', '').replace("'", "")
    except Exception as e:
        print(f"📡 [提示] 云端大模型略有抖动，已自动启动端侧保底机制。({e})")
        return random.choice(LOCAL_RESPONSES)

# ✅ LLM 线程池（避免阻塞 MQTT 网络循环）
llm_executor = ThreadPoolExecutor(max_workers=2, thread_name_prefix="llm")

def handle_fatigue_async(fatigue_type):
    """
    ✅ 异步处理疲劳事件：调用LLM → 更新数据 → TTS播报
    整个过程在独立线程中执行，不阻塞 MQTT 消息处理
    """
    ai_text = generate_ai_voice(fatigue_type)
    latest_data["ai_text"] = ai_text
    print(f"🤖 DeepSeek 实时联网生成下发: {ai_text}")

    # ✅ TTS 语音播报（独立线程，不阻塞LLM结果更新）
    Thread(target=speak_text, args=(ai_text,), daemon=True).start()

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
            # ✅ 异步提交 LLM 任务，不阻塞 MQTT 网络循环
            llm_executor.submit(handle_fatigue_async, fatigue_type)

    except Exception as e:
        print(f"⚠️ MQTT 消息处理异常: {e}")

def mqtt_thread_entry():
    # ✅ 修复: paho-mqtt v2.x 必须传入 CallbackAPIVersion
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = lambda c, u, f, rc: c.subscribe(TOPIC_CAR_DATA)
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
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
