# 音频文件存放

M5StickS3 (N8, 无 PSRAM, 仅 320KB RAM) 无法直接播放 700KB 的 m4a。

请按下面步骤把音频转成 **小体积 WAV** 放到本文件夹：

## 1. 准备音频
把你想要播放的音频（如 `Hong Kong Pedestrian Traffic Light_audio.m4a`）
放在本文件夹内。

## 2. 转换
在 `data/` 文件夹打开 PowerShell 运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\convert_to_wav.ps1
```

这会生成 `track.wav`（8000Hz 单声道 16bit，控制在 ~200KB 内）。

> 如果电脑装了 ffmpeg 但不在 PATH，可给脚本传参：
> `powershell -ExecutionPolicy Bypass -File .\convert_to_wav.ps1 -FfmpegPath "C:\ffmpeg\bin\ffmpeg.exe"`

## 3. 上传文件系统
回到项目根目录运行：

```powershell
& "C:\Users\ManBun\AppData\Local\Python\bin\python.exe" -m platformio run -e esp32-s3-devkitc-1 --target uploadfs --upload-port COM4
```

> ⚠️ 必须先烧录 firmware，再上传文件系统。

## 说明
- 设备每次开机从 `track.wav` 加载音频到内存
- 找不到 `track.wav` 时会自动回退到内置 Aphex Twin 旋律，设备不会失效
- 倾斜仍然控制 BPM（速度）——通过改变 WAV 播放采样率实现
- BtnB 切歌：可放 `track2.wav`、`track3.wav` 支持多首
