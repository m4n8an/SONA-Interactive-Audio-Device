#!/usr/bin/env python3
"""
M5StickS3 录屏工具
==================
连续抓取多帧屏幕截图，合成 GIF 动画。

依赖: pyserial + Pillow
    pip install pyserial pillow

用法:
    python record.py [端口] [帧数] [输出.gif]
    例:  python record.py COM4 40 record.gif    (默认 40 帧, 120ms/帧 ≈ 8fps)

提示: 想看动态效果，先在设备上进入播放界面（按 A 或刷卡），让波形/粒子动起来。
"""
import sys
import time
import serial

try:
    from PIL import Image
except ImportError:
    sys.exit("缺少 Pillow，请先运行: pip install pillow")

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM4"
N = int(sys.argv[2]) if len(sys.argv) > 2 else 40
OUT = sys.argv[3] if len(sys.argv) > 3 else "record.gif"
FRAME_MS = 120          # 每帧时长(ms) → 约 8fps


def read_line(ser):
    line = b""
    while not line.endswith(b"\n"):
        c = ser.read(1)
        if not c:
            raise RuntimeError("读取超时（设备未响应）")
        line += c
    return line


def grab(ser):
    ser.reset_input_buffer()
    ser.write(b"S")
    magic = read_line(ser).strip()
    if magic != b"P6":
        raise RuntimeError(f"数据头错误: {magic!r}")
    w, h = map(int, read_line(ser).split())
    read_line(ser)                      # 255
    rgb = ser.read(w * h * 3)
    if len(rgb) != w * h * 3:
        raise RuntimeError(f"数据不完整: {len(rgb)}/{w*h*3}")
    return Image.frombytes("RGB", (w, h), rgb)


def main():
    ser = serial.Serial(PORT, 115200, timeout=5)
    time.sleep(0.3)
    frames = []
    try:
        for i in range(N):
            try:
                img = grab(ser)
            except Exception as e:
                print(f"第 {i+1} 帧失败: {e}")
                break
            frames.append(img)
            print(f"帧 {i+1}/{N}  ({img.size[0]}x{img.size[1]})")
    finally:
        ser.close()

    if not frames:
        sys.exit("未捕获到任何帧")

    frames[0].save(OUT, save_all=True, append_images=frames[1:],
                   duration=FRAME_MS, loop=0)
    print(f"已保存 {OUT}  共 {len(frames)} 帧, {FRAME_MS}ms/帧 ≈ {1000//FRAME_MS}fps")


if __name__ == "__main__":
    main()
