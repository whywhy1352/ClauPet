"""把几段动画 WebP 首尾相接拼成一个循环文件（直接搬帧数据，不重新压缩）。

要求每段都是 process_clip.py 写出的格式：画布一样大，每帧整张画布、不混合不清除。
每段都以原图开始、在原图前一帧结束，所以按任意顺序拼都没有接缝。

用法：python anim/concat_webp.py 输出.webp 第一段.webp 第二段.webp ...
"""
import os
import sys
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.stdout.reconfigure(encoding="utf-8")
from process_clip import _read_chunks, _chunk  # noqa: E402


def main():
    out, parts = sys.argv[1], sys.argv[2:]
    vp8x, frames, total_ms = None, [], 0
    for p in parts:
        chunks = _read_chunks(open(p, "rb").read())
        head = dict((fc, pl) for fc, pl in chunks if fc in (b"VP8X", b"ANIM"))
        if vp8x is None:
            vp8x = head[b"VP8X"]
        assert head[b"VP8X"] == vp8x, f"{p} 的画布和第一段不一样"
        anmf = [pl for fc, pl in chunks if fc == b"ANMF"]
        for pl in anmf:
            assert pl[15] == 0x02, f"{p} 不是整帧不混合的格式"
            total_ms += pl[12] | pl[13] << 8 | pl[14] << 16
        frames += anmf
        print(f"{p}：{len(anmf)} 帧")
    anim = bytes([0, 0, 0, 0]) + struct.pack("<H", 0)
    body = b"WEBP" + _chunk(b"VP8X", vp8x) + _chunk(b"ANIM", anim) + b"".join(_chunk(b"ANMF", f) for f in frames)
    with open(out, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)
    print(f"写出 {out}：{len(frames)} 帧，{total_ms} 毫秒，{os.path.getsize(out) / 1024:.0f} KB")


if __name__ == "__main__":
    main()
