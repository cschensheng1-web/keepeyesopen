"""自动更新CAM和S3配置里的MQTT IP地址"""
import socket, re, sys

# 获取本机IP
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.connect(("8.8.8.8", 80))
ip = s.getsockname()[0]
s.close()

print(f"当前IP: {ip}")

files = [
    "F:/netplus/keepeyesopen/DMS_project/esp32_firmware/esp32_cam_fw/main/config.h",
    "F:/netplus/keepeyesopen/DMS_project/esp32_firmware/esp32_s3_fw/main/config.h",
]

for f in files:
    with open(f, 'r', encoding='utf-8') as fh:
        content = fh.read()
    # 替换 IP
    new_content = re.sub(r'mqtt://[\d.]+', f'mqtt://{ip}', content)
    if new_content != content:
        with open(f, 'w', encoding='utf-8') as fh:
            fh.write(new_content)
        print(f"  Updated: {f.split('/')[-2]}/config.h")
    else:
        print(f"  Already: {f.split('/')[-2]}/config.h")

print("\n重新编译烧录CAM和S3即可")
