# 端边云协同的主动式车载DMS哨兵系统

> 全国大学生物联网设计竞赛 · 乐鑫命题  
> 团队：不闭眼战车队  
> 成员：陈俊毅（队长/全栈）、覃晖（硬件）、廖宜乐（算法）、王宏博（后端/AI）

---

## 📂 项目结构

```
DMS_project/
├── README.md                    # 本文件
├── requirements.txt             # Python 依赖
├── .env.example                 # 环境变量模板
├── protocol.json                # 数据协议定义
│
├── pc_prototype/                # PC 端算法原型（Python + MediaPipe）
│   ├── face_capture.py          #   摄像头采集 → 疲劳检测 → MQTT 发布
│   └── dms_logic.py             #   独立 PERCLOS 判定（学习用）
│
├── cloud_backend/               # 云端后端（PC 端运行）
│   ├── web_backend.py           #   MQTT 订阅 → DeepSeek LLM → WebSocket + TTS
│   └── index.html               #   ECharts 实时仪表盘大屏
│
└── esp32_firmware/              # ESP32-S3 固件（C/ESP-IDF）
    ├── CMakeLists.txt           #   项目配置
    ├── main/
    │   ├── config.h             #   ⚙️ 引脚定义 & 阈值配置（覃晖改这里）
    │   ├── main.c               #   主入口（根据 TEST_MODE 选择模式）
    │   ├── camera_test.c        #   摄像头驱动测试
    │   ├── audio_test.c         #   MAX98357 I2S 音频放大器测试
    │   ├── ir_led_test.c        #   红外 LED PWM 测试
    │   ├── wifi_mqtt_test.c     #   WiFi + MQTT 连接测试
    │   ├── peripherals_all.c    #   全外设综合测试
    │   ├── ear_mar.c/h          #   EAR/MAR 几何算法（C 实现，廖宜乐）
    │   ├── perclos.c/h          #   PERCLOS 疲劳判定状态机（廖宜乐）
    │   └── dms_algorithm.c      #   全功能 DMS 主循环（廖宜乐）
    └── components/              #   （需自行放入 esp32-camera）
```

---

## 👥 分工与代码归属

| 目录 | 负责 | 说明 |
|---|---|---|
| `pc_prototype/` | **陈俊毅**（队长） | PC 端算法原型，验证算法逻辑 |
| `cloud_backend/` | **王宏博**（后端/AI） | MQTT → DeepSeek LLM → WebSocket → TTS → 大屏 |
| `esp32_firmware/main/config.h` | **覃晖**（硬件） | 改引脚、改 TEST_MODE、改 WiFi 密码 |
| `esp32_firmware/main/*_test.c` | **覃晖**（硬件） | 4 个硬件驱动测试，逐个验证外设 |
| `esp32_firmware/main/ear_mar.c/h` | **廖宜乐**（算法） | EAR/MAR 几何计算 C 实现 |
| `esp32_firmware/main/perclos.c/h` | **廖宜乐**（算法） | PERCLOS 时间基准疲劳判定 |
| `esp32_firmware/main/dms_algorithm.c` | **廖宜乐**（算法） | 全功能 DMS 主循环（填 TODO） |

---

## 🚀 快速开始

### 1. PC 端原型（算法验证用）

```bash
cd pc_prototype
pip install -r ../requirements.txt
python face_capture.py       # 摄像头 + 疲劳检测
```

### 2. 云端后端（MQTT + LLM + 大屏）

```bash
cd cloud_backend
pip install -r ../requirements.txt
cp ../.env.example ../.env   # 编辑 .env 填入 DeepSeek API Key
python web_backend.py        # 启动 WebSocket + MQTT
# 浏览器打开 cloud_backend/index.html
```

### 3. ESP32-S3 固件

```bash
cd esp32_firmware
# 拉取 esp32-camera 组件
git submodule add https://github.com/espressif/esp32-camera.git components/esp32-camera
# 编译和烧录
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

---

## 🔧 覃晖 — 硬件驱动测试

拿到代码后，打开 `main/config.h`，改 `TEST_MODE` 的值，逐项验证硬件：

| TEST_MODE | 测什么 | 预期现象 | 通过标准 |
|---|---|---|---|
| `1` | 摄像头 | 串口输出分辨率 + FPS | 稳定 ≥15 FPS |
| `2` | MAX98357 音频 | 听到 3 声不同音高（440/660/880Hz） | 听到声音 |
| `3` | 红外 LED | 渐亮渐灭 + 全亮 3 秒 | 手机相机看到淡红光 |
| `4` | WiFi+MQTT | 连 WiFi → 连 Broker → 发测试 JSON | 队长电脑收到消息 |
| `5` | 一键综合 | 1→2→3 顺序全跑 | 全部通过 |

> 每测通一项，在群里报一句。全部测通后告诉廖宜乐：「硬件就绪，可以写算法了」

---

## ⚙️ 引脚接线

| ESP32-S3 GPIO | 连接设备 | 说明 |
|---|---|---|
| GPIO 21, 20 | OV2640 I2C (SDA, SCL) | 摄像头配置 |
| GPIO 11,9,8,10,12,18,17,16 | OV2640 DVP D0-D7 | 摄像头 8-bit 并行数据 |
| GPIO 13, 6, 7, 15 | OV2640 PCLK, VSYNC, HREF, XCLK | 摄像头控制信号 |
| GPIO 14 | MAX98357 BCLK | I2S 位时钟 |
| GPIO 19 | MAX98357 LRC (WS) | I2S 左右通道时钟 |
| GPIO 22 | MAX98357 DIN | I2S 数据输入 |
| GPIO 23 | MAX98357 SD | 音频放大器使能（或直接接3.3V） |
| GPIO 5 | 红外 LED | 串 100Ω 限流电阻，PWM 调光 |
| GPIO 48 | 板载状态 LED | 有人脸时亮 |

> ⚠️ 如果实际接线不同，修改 `config.h` 对应宏即可

---

## 🔊 音频架构说明

ESP32-S3 使用 **MAX98357 I2S 音频放大器** 而非蜂鸣器：

- **本地告警**：ESP32 通过 MAX98357 播放提示音（不同疲劳等级不同频率）
- **AI 语音播报**：PC 云端收到 MQTT 疲劳告警 → DeepSeek 生成关怀文字 → PC 端 pyttsx3 TTS 播报
- **未来扩展**：可将预录的语音文件存入 ESP32 Flash，直接通过 MAX98357 播放中文语音

---

## 📊 数据协议（MQTT JSON）

```json
{
  "device_id": "ESP32_DMS_001",
  "data": {"ear": 0.28, "mar": 0.15},
  "status": {"fatigue_level": 0, "desc": "Normal"}
}
```

- `fatigue_level`: 0=正常, 1=持续疲劳(冷却中), 2=微睡眠/哈欠, 3=深度睡眠
- Topic: `dms/car/data`

---

## 🔑 关键技术决策

1. **时间基准，不依赖帧率** — PERCLOS 用 `esp_timer_get_time()` 毫秒计时而非帧计数
2. **双眼平均 EAR** — 避免侧脸/单眼遮挡导致误判
3. **抗干扰豁免** — 手遮眼时自动重置计时器，不误报
4. **AI 冷却锁** — 4 秒内不重复触发 LLM，节省 API 费用
5. **近红外物理过滤** — 850nm 滤光片 + 红外补光，全黑环境可用
6. **I2S 音频输出** — MAX98357 替代蜂鸣器，支持真正的语音播报
