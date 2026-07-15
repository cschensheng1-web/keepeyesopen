"""
仪表盘后端: MQTT订阅 dms/car/data → WebSocket 推送给浏览器
"""
import json, asyncio, os
import paho.mqtt.client as mqtt
import websockets
from threading import Thread
from dotenv import load_dotenv
load_dotenv()

BROKER = os.getenv("MQTT_BROKER", "broker.emqx.io")
PORT = int(os.getenv("MQTT_PORT", "1883"))
TOPIC = os.getenv("MQTT_TOPIC", "dms/car/data")

latest = {"ear": 0.25, "mar": 0.15, "level": 0, "ai_text": ""}
clients = set()

def on_msg(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        latest["ear"]  = data["data"]["ear"]
        latest["mar"]  = data["data"]["mar"]
        latest["level"] = data["status"]["fatigue_level"]
        latest["ai_text"] = data["status"]["desc"]
    except: pass

def mqtt_thread():
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    c.on_connect = lambda cl, u, f, rc, p: cl.subscribe(TOPIC)
    c.on_message = on_msg
    c.connect(BROKER, PORT, 60)
    c.loop_forever()

Thread(target=mqtt_thread, daemon=True).start()

async def echo(ws):
    clients.add(ws)
    try:
        while True:
            await ws.send(json.dumps(latest))
            await asyncio.sleep(0.1)
    except: pass
    finally: clients.remove(ws)

async def main():
    print(f"Dashboard: ws://127.0.0.1:9001  MQTT:{BROKER}")
    async with websockets.serve(echo, "127.0.0.1", 9001):
        await asyncio.Event().wait()

if __name__ == "__main__":
    asyncio.run(main())
