import requests

# 你的配置
API_KEY = "xiaomimimo"  # 填入你完整的真实密钥
MODEL_NAME = "v-2.5pro"

# 尝试路径 A：如果是 Anthropic 原生格式
url_a = "https://token-plan-cn.xiaomimimo.com/anthropic/v1/messages"
headers_a = {
    "x-api-key":'tp-cb3j4yzi0zqboa43kudaevzjtvw9b9zuld30n7sdz447ztk2',
    "anthropic-version": "2023-06-01",
    "content-type": "application/json"
}
payload_a = {
    "model": MODEL_NAME,
    "max_tokens": 50,
    "messages": [{"role": "user", "content": "你好，请回复'测试成功'"}]
}

# 尝试路径 B：如果是 OpenAI 兼容格式但路径比较特殊
url_b = "https://token-plan-cn.xiaomimimo.com/anthropic/v1/chat/completions"
headers_b = {
    "Authorization": f"Bearer {API_KEY}",
    "Content-Type": "application/json"
}
payload_b = {
    "model": MODEL_NAME,
    "messages": [{"role": "user", "content": "你好，请回复'测试成功'"}]
}

print("🛰️ 正在测试 路径 A (Anthropic 原生格式)...")
try:
    r = requests.post(url_a, json=payload_a, headers=headers_a, timeout=5)
    print(f"路径 A 状态码: {r.status_code}")
    print(f"路径 A 返回内容: {r.text[:200]}")
except Exception as e:
    print(f"路径 A 崩溃: {e}")

print("\n🛰️ 正在测试 路径 B (OpenAI 兼容格式)...")
try:
    r = requests.post(url_b, json=payload_b, headers=headers_b, timeout=5)
    print(f"路径 B 状态码: {r.status_code}")
    print(f"路径 B 返回内容: {r.text[:200]}")
except Exception as e:
    print(f"路径 B 崩溃: {e}")