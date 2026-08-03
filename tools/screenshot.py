#!/usr/bin/env python3
"""
M5StickS3 屏幕截图工具
======================
向设备串口发送 'S' 请求一帧 PPM 截图，解析后保存为 PNG。

依赖: pyserial (pip install pyserial)  — PNG 用标准库 zlib/struct 手写，无需 PIL

用法:
    python screenshot.py [端口] [输出.png]
    例:  python screenshot.py COM4 screen.png
         python screenshot.py COM4
"""
import sys
import time
import zlib
import struct

try:
    import serial
except ImportError:
    sys.exit("缺少 pyserial，请先运行: pip install pyserial")

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM4"
OUT = sys.argv[2] if len(sys.argv) > 2 else "screen.png"


def read_line(ser):
    line = b""
    while not line.endswith(b"\n"):
        c = ser.read(1)
        if not c:
            raise RuntimeError("读取串口超时（设备未连接或未响应）")
        line += c
    return line


def chunk(tag, data):
    c = struct.pack(">I", len(data)) + tag + data
    return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def write_png(path, w, h, rgb):
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    scan = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(scan))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main():
    ser = serial.Serial(PORT, 115200, timeout=5)
    time.sleep(0.2)                 # 等设备就绪
    ser.reset_input_buffer()
    ser.write(b"S")                 # 请求截图

    magic = read_line(ser).strip()
    if magic != b"P6":
        raise RuntimeError(f"意外的数据头: {magic!r}（确认烧录了带截图功能的固件）")

    w, h = map(int, read_line(ser).split())
    maxv = read_line(ser).strip()
    rgb = ser.read(w * h * 3)
    if len(rgb) != w * h * 3:
        raise RuntimeError(f"数据不完整: {len(rgb)}/{w*h*3} 字节")

    write_png(OUT, w, h, rgb)
    print(f"已保存 {OUT}  ({w}x{h}, 最大亮度 {maxv.decode()})")


if __name__ == "__main__":
    main()
