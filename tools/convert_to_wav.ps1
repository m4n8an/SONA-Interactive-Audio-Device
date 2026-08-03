# ═══════════════════════════════════════════════════
#  把音频转成 M5StickS3 可播放的 WAV (完整版, 用 PSRAM)
#  用法: powershell -ExecutionPolicy Bypass -File .\convert_to_wav.ps1
# ═══════════════════════════════════════════════════
param(
  [string]$InputFile = ".\source_audio.m4a",
  [string]$OutputFile = "..\data\track.wav",
  [string]$FfmpegPath = "ffmpeg"
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $InputFile)) {
  Write-Host "找不到输入文件: $InputFile" -ForegroundColor Red
  Write-Host "请把音频文件放在本 tools 文件夹里，或 -InputFile 指定路径。"
  exit 1
}

# 检查 ffmpeg
try { & $FfmpegPath -version | Out-Null } catch {
  Write-Host "未找到 ffmpeg。请安装 https://ffmpeg.org 并加入 PATH，或用 -FfmpegPath 指定。" -ForegroundColor Red
  exit 1
}

# 完整时长, 11025Hz 单声道 16bit → 22050 字节/秒
# 36.7s ≈ 810KB → 放进 8MB PSRAM 无压力
# volume=1.8 提升音量 (+5dB), 配合代码里 setVolume(230) 更响
Write-Host "转换: $InputFile → $OutputFile (11025Hz mono 16bit, 完整版, 音量+5dB)" -ForegroundColor Cyan
& $FfmpegPath -y -i $InputFile -ac 1 -ar 11025 -af "volume=1.8" -acodec pcm_s16le $OutputFile

if ($LASTEXITCODE -ne 0) { Write-Host "转换失败!" -ForegroundColor Red; exit 1 }

$size = (Get-Item $OutputFile).Length
Write-Host "完成! 大小: $size 字节" -ForegroundColor Green

if ($size -gt 1500000) {
  Write-Host "⚠️ 文件超过 1.5MB, 可能超出 PSRAM 分配限制。可降低采样率(-ar 8000)。" -ForegroundColor Yellow
} else {
  Write-Host "✅ 大小合适(PSRAM 可容纳)。接下来: 烧录固件 + 上传文件系统" -ForegroundColor Green
  Write-Host '  cd ..; python -m platformio run -e esp32-s3-devkitc-1 --target upload --upload-port COM4'
  Write-Host '  python -m platformio run -e esp32-s3-devkitc-1 --target uploadfs --upload-port COM4'
}
