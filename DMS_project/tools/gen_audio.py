"""
用 pyttsx3 生成中文语音 → 转成 C 数组 → 嵌入 ESP32-S3 固件
用法: python gen_audio.py "请勿疲劳驾驶" alert1
输出: alert1.h（可直接放进 S3 main/ 目录）
"""
import sys
import wave
import struct
import io
import os

try:
    import pyttsx3
except ImportError:
    print("pip install pyttsx3")
    exit(1)

TEXT = sys.argv[1] if len(sys.argv) > 1 else "请勿疲劳驾驶"
NAME = sys.argv[2] if len(sys.argv) > 2 else "voice_alert"

# 1. 生成 WAV
print(f"生成语音: {TEXT}")
engine = pyttsx3.init()
engine.setProperty('rate', 180)

# 保存到临时文件
tmp_wav = "temp_voice.wav"
engine.save_to_file(TEXT, tmp_wav)
engine.runAndWait()

# 2. 读取 WAV 并转 16kHz mono 16bit PCM
# (pyttsx3 输出格式不同，用简单方式重采样)
import subprocess
try:
    # 如果有 ffmpeg
    subprocess.run(["ffmpeg", "-i", tmp_wav, "-ar", "16000", "-ac", "1",
                    "-sample_fmt", "s16", "-y", tmp_wav],
                   capture_output=True, timeout=10)
except:
    pass  # 没有 ffmpeg 就用原始 WAV

with open(tmp_wav, 'rb') as f:
    wav_data = f.read()

# 3. 跳过 WAV 头，只取 PCM 数据
# 找 "data" chunk
data_start = wav_data.find(b'data')
if data_start == -1:
    print("WAV 格式错误")
    exit(1)
# data chunk: "data" + 4-byte size + samples
pcm = wav_data[data_start + 8:]

# 4. 生成 C 头文件
out_path = f"{NAME}.h"
with open(out_path, 'w', encoding='utf-8') as f:
    f.write(f"// 自动生成: {TEXT}\n")
    f.write(f"// 采样率: 16000Hz, 16bit, Mono\n")
    f.write(f"#ifndef {NAME.upper()}_H\n")
    f.write(f"#define {NAME.upper()}_H\n\n")
    f.write(f"#include <stdint.h>\n\n")
    f.write(f"#define {NAME.upper()}_LEN  {len(pcm)}\n")
    f.write(f"static const uint8_t {NAME}[{len(pcm)}] = {{\n    ")

    for i, b in enumerate(pcm):
        f.write(f"0x{b:02X}, ")
        if (i + 1) % 16 == 0:
            f.write("\n    ")
    f.write("\n};\n\n#endif\n")

print(f"✅ 生成 {out_path} ({len(pcm)} 字节, {len(pcm)/32000:.2f} 秒)")
print(f"   放进 esp32_s3_fw/main/ 目录")
print(f"   固件里: #include \"{NAME}.h\"")
print(f"   播放: i2s_channel_write(i2s_h, {NAME}, {NAME}_LEN, &w, portMAX_DELAY);")
os.remove(tmp_wav)

# 同时生成一个播放函数方便调用
with open(f"{NAME}_play.h", 'w', encoding='utf-8') as f:
    f.write(f'#include "{NAME}.h"\n')
    f.write(f'static void play_{NAME}(i2s_chan_handle_t h) {{\n')
    f.write(f'    size_t w;\n')
    f.write(f'    i2s_channel_write(h, {NAME}, {NAME}_LEN, &w, portMAX_DELAY);\n')
    f.write(f'}}\n')
print(f"   播放函数: play_{NAME}(i2s_h);")
