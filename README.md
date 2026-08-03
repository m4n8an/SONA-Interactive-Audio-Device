# SONA Interactive Audio Device — M5StickS3 固件

M5StickS3 上的**律动音乐播放器**固件（PlatformIO / Arduino）。

## 功能
- 🎵 3 首内置节奏旋律（Pulse / Beat / Groove）循环播放
- 📳 HAT Vibrator 震动（跟随音频包络，双击 B 开关）
- 📇 RFID2 刷卡播放（卡放上播放、离开停止）
- 📱 倾斜控制 BPM、长按 B 变调、双击 B 开关震动
- 🔊 大音量（ES8311 DAC +16dB）

## 烧录
```powershell
python -m platformio run -e esp32-s3-devkitc-1 --target upload --upload-port COM4
```

## 截图 / 录屏
```powershell
# 单帧截图
python tools\screenshot.py COM4 screen.png
# 流畅录屏（MP4）
python tools\record_fast.py COM4
```

## 结构
```
├── src/main.cpp          # 主程序
├── platformio.ini        # 构建配置（已启用 8MB PSRAM）
└── tools/                # 配套工具（截图/录屏/音频转换）
```
