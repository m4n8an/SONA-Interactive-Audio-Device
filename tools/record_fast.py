#!/usr/bin/env python3
"""
M5StickS3 流畅录屏工具（方案 B）
===============================
让设备进入连续录屏模式（'R'），持续接收降采样帧，合成 GIF。
比逐帧截图更快更流畅（设备端主动连续发帧）。

依赖: pyserial + Pillow
    pip install pyserial pillow

用法:
    python record_fast.py [端口] [帧数] [输出.gif]
    例:  python record_fast.py COM4 120 record.gif   (120 帧 ≈ 6 秒)

提示: 先在设备上进入播放界面（按 A 或刷卡），让波形/粒子动起来。
"""
import sys
import os
import time
import serial

try:
    from PIL import Image
except ImportError:
    sys.exit("缺少 Pillow，请先运行: pip install pillow")

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM4"
N = int(sys.argv[2]) if len(sys.argv) > 2 else 120
OUT = sys.argv[3] if len(sys.argv) > 3 else "record_fast.mp4"
FPS = 20                # 输出帧率


def read_line(ser):
    line = b""
    while not line.endswith(b"\n"):
        c = ser.read(1)
        if not c:
            raise RuntimeError("读取超时（设备未响应或已停止录屏）")
        line += c
    return line


def read_frame(ser):
    marker = read_line(ser).strip()
    if marker != b"FRAME":
        raise RuntimeError(f"帧标记错误: {marker!r}")
    magic = read_line(ser).strip()
    if magic != b"P6":
        raise RuntimeError(f"数据头错误: {magic!r}")
    w, h = map(int, read_line(ser).split())
    read_line(ser)                      # 255
    rgb = ser.read(w * h * 3)
    if len(rgb) != w * h * 3:
        raise RuntimeError(f"数据不完整: {len(rgb)}/{w*h*3}")
    return Image.frombytes("RGB", (w, h), rgb)


def write_mp4(frames, out, fps=20):
    """用 ffmpeg 把帧序列合成 MP4（H.264 / yuv420p，兼容播放器）。"""
    import shutil
    import subprocess
    ff = shutil.which("ffmpeg") or r"C:\Users\ManBun\AppData\Local\Microsoft\WinGet\Links\ffmpeg.exe"
    if not os.path.exists(ff):
        sys.exit("找不到 ffmpeg，请安装或加入 PATH（可运行: winget install Gyan.FFmpeg）")
    w, h = frames[0].size
    print(f"编码 MP4: {w}x{h}, {len(frames)} 帧, {fps}fps …")
    # scale → 偶数宽高（H.264/yuv420p 要求），避免奇数尺寸编码失败/播放器不认
    base = [ff, "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{w}x{h}",
            "-r", str(fps), "-i", "-",
            "-vf", "scale=trunc(iw/2)*2:trunc(ih/2)*2"]
    # 先试 H.264，失败则回退 MPEG-4（打印 ffmpeg 错误以便诊断）
    for codec in (["-c:v", "libx264", "-pix_fmt", "yuv420p", "-preset", "fast"],
                  ["-c:v", "mpeg4", "-q:v", "5"]):
        try:
            os.remove(out)
        except OSError:
            pass
        proc = subprocess.Popen(base + codec + [out], stdin=subprocess.PIPE,
                                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            for img in frames:
                proc.stdin.write(img.tobytes())
            proc.stdin.close()
        except (BrokenPipeError, OSError):
            pass
        _, err = proc.communicate()
        if proc.returncode == 0 and os.path.exists(out) and os.path.getsize(out) > 0:
            print(f"已保存 {out}  ({os.path.getsize(out)} 字节)")
            return
        print("编码失败，ffmpeg 输出：")
        print(err.decode(errors="replace")[:2000])
    sys.exit("ffmpeg 编码失败")


def main():
    ser = serial.Serial(PORT, 115200, timeout=5)
    time.sleep(0.3)
    ser.reset_input_buffer()
    ser.write(b"R")                     # 开始录屏
    print(f"开始录屏，采集 {N} 帧…（在设备上操作界面）")
    frames = []
    try:
        for i in range(N):
            try:
                img = read_frame(ser)
            except Exception as e:
                print(f"第 {i+1} 帧失败: {e}")
                break
            frames.append(img)
            if (i + 1) % 10 == 0:
                print(f"…{i+1}/{N} 帧")
    finally:
        ser.write(b"E")                 # 停止录屏
        ser.close()

    if not frames:
        sys.exit("未捕获到任何帧")

    write_mp4(frames, OUT, fps=FPS)
    print(f"已保存 {OUT}  共 {len(frames)} 帧 ({frames[0].size[0]}x{frames[0].size[1]}, "
          f"{FPS}fps)")


if __name__ == "__main__":
    main()
