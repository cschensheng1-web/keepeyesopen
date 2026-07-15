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

import threading
_tts_lock = threading.Lock()
_tts_busy = False

def speak_text(text):
    """独立线程播报，防并发"""
    global _tts_busy
    if not tts_enabled:
        return
    if _tts_busy:
        return  # 上一句还在播，跳过
    def _speak():
        global _tts_busy
        try:
            _tts_busy = True
            tts_engine.say(text)
            tts_engine.runAndWait()
        except Exception as e:
            print(f"⚠️ TTS 播报失败: {e}")
        finally:
            _tts_busy = False
    Thread(target=_speak, daemon=True).start()

# ==========================================
# 🤖 离线蒸馏高情商语料库（作为网络抖动时的金牌保底）
# ==========================================
LOCAL_RESPONSES_EYE = [
    "眼睛都快眯成一条缝了！前面两公里有服务区，咱去洗把脸吧~",
    "检测到你在打瞌睡！来段 rap 提提神：药药切克闹，疲劳驾驶不要闹！",
    "老哥醒醒！眼皮子已经在打架了，喝口咖啡再出发！",
    "危险！你已进入微睡眠状态，要不要我给你放首《最炫民族风》？"
]

LOCAL_RESPONSES_YAWN = [
    "这哈欠打得惊天动地！看来周公在召唤你了，前方休息区等你~",
    "连续哈欠检测！车里有红牛吗？没有的话路边便利店来一罐！",
    "嘴巴张这么大是想吃风吗？困了别硬撑，安全第一！",
    "哈欠连天啊兄弟！开窗透透气，或者来段郭德纲相声提提神？"
]

# ==========================================
# 🌐 真·大模型端云协同生成核心
# ==========================================
def generate_ai_voice(fatigue_type="疲劳"):
    if not API_KEY:
        print("⚠️ 未设置 DEEPSEEK_API_KEY 环境变量，使用本地语料库")
        return random.choice(LOCAL_RESPONSES_EYE)

    headers = {
        "Authorization": f"Bearer {API_KEY}",
        "Content-Type": "application/json"
    }

    # 根据疲劳类型生成不同的 system prompt
    if "哈欠" in fatigue_type or "打哈" in fatigue_type:
        system_prompt = "你是一个智能车载座舱语音助手。司机正在连续打哈欠，说明困意来袭但还没完全睡着。请用幽默调侃的语气（像个开车的老朋友一样），生成一句20字以内的中文提醒。要求有趣、有记忆点、不重复！"
        user_prompt = "司机正连续打哈欠，给他来一句带幽默感的提神播报。"
        local_pool = LOCAL_RESPONSES_YAWN
    else:
        system_prompt = "你是一个智能车载座舱语音助手。司机正在犯困闭眼，这是非常危险的微睡眠状态！请用急切但温暖的口吻，生成一句20字以内的中文提醒。要让人一下子惊醒但又不会吓到。"
        user_prompt = "司机正在闭眼打瞌睡，快给他一句紧急但温暖的唤醒播报。"
        local_pool = LOCAL_RESPONSES_EYE

    payload = {
        "model": MODEL_NAME,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt}
        ],
        "temperature": 0.9,       # 提高温度增加随机性
        "max_tokens": 80          # 限制输出长度
    }

    try:
        print(f"📡 正在调用 DeepSeek API（{fatigue_type}）...")
        response = requests.post(API_URL, json=payload, headers=headers, timeout=10)
        res_json = response.json()
        ai_msg = res_json["choices"][0]["message"]["content"]
        result = ai_msg.strip().replace('"', '').replace("'", "")
        print(f"✅ DeepSeek 返回: {result}")
        return result
    except Exception as e:
        fallback = random.choice(local_pool)
        print(f"📡 DeepSeek 调用失败({type(e).__name__})，使用本地语料: {fallback}")
        return fallback

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
    client.on_connect = lambda c, u, f, rc, p: c.subscribe(TOPIC_CAR_DATA)
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
    print("🚀 宏博的工业级网页大屏网关已在本地 [ws://127.0.0.1:9001] 挂载就绪！")
    async with websockets.serve(echo, "127.0.0.1", 9001):
        await asyncio.Event().wait()

if __name__ == "__main__":
    asyncio.run(main())
