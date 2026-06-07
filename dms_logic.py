import time

# 核心配置参数（严格对照项目书 4.2 节设定）
EAR_THRESHOLD = 0.18  # 闭眼阈值
MAR_THRESHOLD = 0.60  # 哈欠阈值
BLINK_DURATION = 1.5  # 闭眼报警时限（秒）
YAWN_DURATION = 2.5   # 哈欠报警时限（秒）

class DMSJudge:
    def __init__(self):
        self.blink_start_time = None
        self.yawn_start_time = None

    def update_status(self, ear, mar):
        current_time = time.time()
        alarm_triggered = False
        alarm_type = "正常"

        # 1. 眼睛闭合判定（PERCLOS思想）
        if ear < EAR_THRESHOLD:
            if self.blink_start_time is None:
                self.blink_start_time = current_time  # 记录闭眼开始时刻
            elif current_time - self.blink_start_time >= BLINK_DURATION:
                alarm_triggered = True
                alarm_type = "🚨 疲劳报警：司机闭眼打瞌睡！"
        else:
            self.blink_start_time = None  # 睁眼了，清空计时

        # 2. 嘴巴张开判定
        if mar > MAR_THRESHOLD:
            if self.yawn_start_time is None:
                self.yawn_start_time = current_time  # 记录哈欠开始时刻
            elif current_time - self.yawn_start_time >= YAWN_DURATION:
                alarm_triggered = True
                alarm_type = "⚠️ 疲劳预警：司机连续哈欠！"
        else:
            self.yawn_start_time = None  # 闭嘴了，清空计时

        return alarm_triggered, alarm_type

# --- 本地模拟运行测试 ---
if __name__ == "__main__":
    judge = DMSJudge()
    print("开始测试组长编写的 PERCLOS 判定逻辑...")
    # 模拟司机连续闭眼 3 秒的情况
    for second in range(3):
        has_alarm, msg = judge.update_status(ear=0.10, mar=0.15) # 强行输入闭眼数据
        print(f"第 {second+1} 秒判定结果 -> {msg}")
        time.sleep(1)