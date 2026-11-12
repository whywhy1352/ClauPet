"""把 Seedance 生成的蓝底视频处理成桌宠用的透明动画 WebP。

流程：
  1. 解码视频（即梦导出的视频没标色彩矩阵，实测按 BT.601 解码，纯蓝背景才能还原成 0,0,255）
  2. 每帧估计背景蓝色（视频中间会整体变暗，背景色每帧都不一样），按"蓝色比红绿多出多少"抠出透明度
  3. 去掉小噪点；边缘像素按已知背景色还原前景色，再去蓝边
  4. 逐帧把前景颜色的分布匹配到原图（修正中间帧整体变暗、偏暖）
  5. 第 0 帧直接用原图；去掉最后一帧（它也是原图，循环时会和下一轮的第 0 帧重复）；
     首尾各几帧和原图做交叉淡化，消除接缝
  6. 裁回原图的画布范围，按预乘透明度缩小，编码成无限循环的动画 WebP

用法（在项目根目录运行；Python 用的是本机 Python 3.14，依赖装在 anim/.pylib）：
  python anim/process_clip.py 视频.mp4 输出.webp [--preview 预览图.png]
"""
import io
import os
import sys
import json
import struct
import argparse
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".pylib"))
sys.stdout.reconfigure(encoding="utf-8")

import numpy as np  # noqa: E402
import cv2  # noqa: E402
from PIL import Image  # noqa: E402


# ---------- 解码 ----------

def probe_size(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0",
                          "-show_entries", "stream=width,height", "-of", "json", path],
                         capture_output=True, check=True).stdout
    st = json.loads(out)["streams"][0]
    return st["width"], st["height"]


def decode(path, matrix):
    w, h = probe_size(path)
    vf = ("scale=flags=bicubic+accurate_rnd+full_chroma_int:"
          f"in_color_matrix={matrix}:in_range=tv,format=rgb24")
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-an", "-fps_mode", "passthrough",
                          "-vf", vf, "-f", "rawvideo", "-"], capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.uint8).reshape(-1, h, w, 3)


# ---------- 抠图 ----------

def estimate_bg(frame, border=16):
    strips = [frame[:border], frame[-border:], frame[:, :border], frame[:, -border:]]
    return np.median(np.concatenate([s.reshape(-1, 3) for s in strips]), axis=0).astype(np.float32)


SCREEN_CH = {"blue": (2, 0, 1), "green": (1, 0, 2)}  # (幕布通道, 另外两个通道)


def key_alpha(frame, bg, lo, hi, screen):
    """色幕抠像：蓝幕 dom = B - max(R, G)，绿幕 dom = G - max(R, B)。
    背景的 dom 最大；角色里没有偏向幕布颜色的地方，dom <= 0。"""
    k, o1, o2 = SCREEN_CH[screen]
    f = frame.astype(np.float32)
    dom = f[..., k] - np.maximum(f[..., o1], f[..., o2])
    d_bg = bg[k] - max(bg[o1], bg[o2])
    return np.clip((hi * d_bg - dom) / ((hi - lo) * d_bg), 0.0, 1.0)


def clean_alpha(alpha, min_area):
    """去掉和角色不相连的小块（压缩噪点、水印之类）。"""
    solid = (alpha > 0.5).astype(np.uint8)
    n, lab, stats, _ = cv2.connectedComponentsWithStats(solid, connectivity=8)
    keep = np.zeros(n, bool)
    keep[1:] = stats[1:, cv2.CC_STAT_AREA] >= min_area
    mask = keep[lab].astype(np.uint8)
    mask = cv2.dilate(mask, np.ones((7, 7), np.uint8))
    return alpha * mask


def unmix(frame, alpha, bg, spill_margin, screen):
    """已知背景色时还原边缘像素的前景色：C = a*F + (1-a)*Bg  =>  F = (C - (1-a)*Bg) / a，再去色边。

    蓝幕：B <= G + margin，整张图都限制。这个角色的配色（橙发、白裙、黑丝带、金流苏、肤色）蓝色都不超过绿色。
    不能用常见的 B <= max(R, G)：深棕色描边的 R 大，会被染成紫色。
    绿幕（error 用，因为有淡蓝色眼泪）：只在边缘限制 G <= max(R, B)。金色流苏的 G 比 (R+B)/2 大，
    所以不能用更强的规则，也不能整张图都限制。
    """
    f = frame.astype(np.float32)
    a = alpha[..., None]
    fg = np.where(a > 0.02, (f - (1.0 - a) * bg) / np.maximum(a, 0.02), f)
    fg = np.clip(fg, 0, 255)
    if screen == "blue":
        fg[..., 2] = np.minimum(fg[..., 2], fg[..., 1] + spill_margin)
    else:
        edge = alpha < 0.999
        fg[..., 1] = np.where(edge, np.minimum(fg[..., 1], np.maximum(fg[..., 0], fg[..., 2])), fg[..., 1])
    return fg


# ---------- 颜色校正 ----------

def hist_lut(src, ref):
    """单通道直方图匹配，返回 256 项的查找表。"""
    s_cdf = np.cumsum(np.bincount(src, minlength=256)).astype(np.float64)
    r_cdf = np.cumsum(np.bincount(ref, minlength=256)).astype(np.float64)
    s_cdf /= s_cdf[-1]
    r_cdf /= r_cdf[-1]
    return np.interp(s_cdf, r_cdf, np.arange(256, dtype=np.float64))


# ---------- 缩放（预乘透明度） ----------

def resize_rgba(rgb, alpha, size):
    p = rgb.astype(np.float32) * alpha[..., None]
    p = cv2.resize(p, (size, size), interpolation=cv2.INTER_AREA)
    a = cv2.resize(alpha.astype(np.float32), (size, size), interpolation=cv2.INTER_AREA)
    out = np.where(a[..., None] > 1e-4, p / np.maximum(a[..., None], 1e-4), 0)
    return np.clip(out, 0, 255), np.clip(a, 0, 1)


# ---------- 写动画 WebP ----------

def _read_chunks(data):
    assert data[:4] == b"RIFF" and data[8:12] == b"WEBP"
    pos, out = 12, []
    while pos < len(data):
        size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        out.append((data[pos:pos + 4], data[pos + 8:pos + 8 + size]))
        pos += 8 + size + (size & 1)
    return out


def _chunk(fourcc, payload):
    return fourcc + struct.pack("<I", len(payload)) + payload + (b"\0" if len(payload) & 1 else b"")


def _u24(v):
    return struct.pack("<I", v)[:3]


def write_webp_anim(path, images, durations, quality, loop=0):
    """每一帧都是整张画布，"不混合、不清除"。

    loop：WebP 文件里的循环次数。0 = 无限循环（各状态的循环动画）；1 = 只播一次（过渡片段：
    Qt 读出来 loopCount() 是 0，QMovie 播完最后一帧就停下，并发出 finished()）。

    Pillow 自带的动画编码器会把帧裁成只包含变化部分的小块，靠"混合/清除"规则一层层叠出画面，
    这要求解码器严格按规范实现这些规则。这里每帧都整张覆盖，解码器只要逐帧照画就行，Qt 的 qwebp
    插件不可能画错。因为整个角色几乎每帧都在动，文件也不会因此变大多少。
    """
    w, h = images[0].size
    frames = []
    for img, dur in zip(images, durations):
        buf = io.BytesIO()
        img.save(buf, "WEBP", quality=quality, method=6)
        body = b"".join(_chunk(fc, pl) for fc, pl in _read_chunks(buf.getvalue())
                        if fc in (b"ALPH", b"VP8 ", b"VP8L"))
        # ANMF 帧头：X/2、Y/2、宽-1、高-1、时长（各 24 位），标志字节 0x02 = 不混合、不清除
        header = _u24(0) + _u24(0) + _u24(w - 1) + _u24(h - 1) + _u24(dur) + bytes([0x02])
        frames.append(_chunk(b"ANMF", header + body))
    vp8x = _chunk(b"VP8X", bytes([0x12, 0, 0, 0]) + _u24(w - 1) + _u24(h - 1))  # 0x12 = 有透明度 + 动画
    anim = _chunk(b"ANIM", bytes([0, 0, 0, 0]) + struct.pack("<H", loop))  # 背景色透明，然后是循环次数
    body = b"WEBP" + vp8x + anim + b"".join(frames)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("video")
    ap.add_argument("out")
    ap.add_argument("--orig", default="assets/pet/idle.png", help="原图（透明背景）")
    ap.add_argument("--canvas", type=int, default=1440, help="喂给视频模型的蓝底图边长")
    ap.add_argument("--offset", type=int, default=93, help="原图在蓝底图里的左上角偏移")
    ap.add_argument("--size", type=int, default=360, help="输出边长")
    ap.add_argument("--fps", type=float, default=24.0)
    ap.add_argument("--matrix", default="bt601")
    ap.add_argument("--screen", choices=["blue", "green"], default="blue", help="喂给视频模型的底色")
    ap.add_argument("--key-lo", type=float, default=0.04)
    ap.add_argument("--key-hi", type=float, default=0.90)
    ap.add_argument("--spill-margin", type=float, default=4.0)
    ap.add_argument("--fade", type=int, default=0, help="首尾各几帧和原图交叉淡化（头发在动时会出重影）")
    ap.add_argument("--quality", type=int, default=85)
    ap.add_argument("--preview", help="把若干处理后的帧拼在棋盘格上，存成 PNG 方便检查")
    args = ap.parse_args()

    frames = decode(args.video, args.matrix)
    n, h, w, _ = frames.shape
    assert h == w, "这个脚本只处理正方形视频"
    orig = np.asarray(Image.open(args.orig).convert("RGBA"))
    osz = orig.shape[0]
    assert orig.shape[1] == osz, "原图应为正方形"
    o_rgb = orig[..., :3].astype(np.float32)
    o_a = orig[..., 3].astype(np.float32) / 255.0

    scale = w / args.canvas
    x0 = int(round(args.offset * scale))
    side = int(round(osz * scale))
    print(f"{n} 帧，{w}x{h}；原图 {osz}x{osz}，在视频里从 {x0} 起，边长 {side}")

    # 颜色校正的目标：原图缩到视频里同样大小后，不透明像素的颜色分布
    ref_rgb, ref_a = resize_rgba(o_rgb, o_a, side)
    ref_px = ref_rgb[ref_a >= 0.98].astype(np.uint8)

    # 检查对位：视频第 0 帧和纯色底输入图应该完全重合
    inp = np.zeros((args.canvas, args.canvas, 3), np.float32)
    inp[..., SCREEN_CH[args.screen][0]] = 255
    win = (slice(args.offset, args.offset + osz), slice(args.offset, args.offset + osz))
    inp[win] = o_rgb * o_a[..., None] + inp[win] * (1 - o_a[..., None])
    inp = cv2.resize(inp, (w, w), interpolation=cv2.INTER_AREA)
    (dx, dy), resp = cv2.phaseCorrelate(cv2.cvtColor(inp, cv2.COLOR_RGB2GRAY),
                                        cv2.cvtColor(frames[0].astype(np.float32), cv2.COLOR_RGB2GRAY))
    print(f"第 0 帧相对输入图的偏移：dx={dx:+.2f} dy={dy:+.2f}（响应 {resp:.2f}）")

    # 逐帧抠图、还原前景色、算直方图匹配表
    fgs, alphas, luts, bgs = [], [], [], []
    min_area = int(0.0005 * w * h)
    for t in range(n):
        bg = estimate_bg(frames[t])
        a = clean_alpha(key_alpha(frames[t], bg, args.key_lo, args.key_hi, args.screen), min_area)
        fg = unmix(frames[t], a, bg, args.spill_margin, args.screen)
        src_px = fg[a >= 0.98].astype(np.uint8)
        luts.append(np.stack([hist_lut(src_px[:, c], ref_px[:, c]) for c in range(3)]))
        fgs.append(fg)
        alphas.append(a)
        bgs.append(bg)
    luts = np.stack(luts)  # n, 3, 256
    # 查找表在时间上做一点平滑，避免逐帧闪烁（首尾两帧本来就接近原图，不平滑）
    k = 2
    smooth = luts.copy()
    for t in range(1, n - 1):
        lo_t, hi_t = max(1, t - k), min(n - 2, t + k)
        smooth[t] = luts[lo_t:hi_t + 1].mean(axis=0)

    # 校正、裁剪、缩小
    out_rgb, out_a = [], []
    before, after = [], []
    for t in range(n - 1):  # 最后一帧丢掉
        fg, a = fgs[t], alphas[t]
        if t == 0:
            rgb, al = resize_rgba(o_rgb, o_a, args.size)
        else:
            idx = np.clip(fg, 0, 255).astype(np.uint8)
            corr = np.stack([smooth[t, c][idx[..., c]] for c in range(3)], axis=-1)
            opaque = a >= 0.98
            before.append(fg[opaque].mean(0))
            after.append(corr[opaque].mean(0))
            crop = (slice(x0, x0 + side), slice(x0, x0 + side))
            rgb, al = resize_rgba(corr[crop], a[crop], args.size)
        out_rgb.append(rgb)
        out_a.append(al)

    # 首尾交叉淡化（在预乘空间里混合原图）
    o_small_rgb, o_small_a = out_rgb[0], out_a[0]
    m = len(out_rgb)
    for i in range(1, args.fade + 1):
        wgt = 1.0 - i / (args.fade + 1)  # 越靠近原图那一端，原图占比越大
        for t in (i, m - i):
            pa = out_rgb[t] * out_a[t][..., None] * (1 - wgt) + o_small_rgb * o_small_a[..., None] * wgt
            aa = out_a[t] * (1 - wgt) + o_small_a * wgt
            out_rgb[t] = np.where(aa[..., None] > 1e-4, pa / np.maximum(aa[..., None], 1e-4), 0)
            out_a[t] = aa

    # 检查：角色有没有超出原图画布（会被窗口裁掉）
    union = np.max(np.stack(alphas[1:-1]), axis=0) > 0.05
    ys, xs = np.nonzero(union)
    print(f"所有帧里角色的范围：x {xs.min()}–{xs.max()}，y {ys.min()}–{ys.max()}（画布 {x0}–{x0 + side - 1}）")
    over = [t for t in range(1, n - 1)
            if (alphas[t][:, :x0] > 0.05).any() or (alphas[t][:, x0 + side:] > 0.05).any()
            or (alphas[t][:x0] > 0.05).any() or (alphas[t][x0 + side:] > 0.05).any()]
    if over:
        print(f"  超出画布（会被窗口裁掉一点）的帧：{len(over)} 帧，第 {over[0]}～{over[-1]} 帧之间")

    # 接缝检查：相邻两帧的差（预乘后），第 0 帧前后和循环接缝处不应明显大于平时
    def diff(i, j):
        pi = out_rgb[i] * out_a[i][..., None]
        pj = out_rgb[j] * out_a[j][..., None]
        return np.abs(pi - pj).mean() + np.abs(out_a[i] - out_a[j]).mean() * 255
    steps = [diff(t, t + 1) for t in range(1, m - 1)]
    print(f"相邻帧差：平时中位数 {np.median(steps):.2f}，最大 {max(steps):.2f}；"
          f"第 0→1 帧 {diff(0, 1):.2f}；循环接缝（最后一帧→第 0 帧）{diff(m - 1, 0):.2f}")
    bgs = np.array(bgs)
    print(f"背景蓝：第 0 帧 {np.round(bgs[0]).astype(int)}，中间 {np.round(bgs[n // 2]).astype(int)}")
    before, after = np.array(before), np.array(after)
    ref_mean = ref_px.mean(0)
    print(f"不透明像素平均颜色  原图 {np.round(ref_mean, 1)}")
    print(f"  校正前：最暗那帧 {np.round(before[before.sum(1).argmin()], 1)}")
    print(f"  校正后：各帧和原图的最大偏差 {np.round(np.abs(after - ref_mean).max(0), 1)}")

    # 编码 WebP：24fps 的每帧时长用累计取整（41/42 毫秒交替），总长不漂
    durations = [int(round((i + 1) * 1000 / args.fps)) - int(round(i * 1000 / args.fps)) for i in range(m)]
    imgs = [Image.fromarray(np.dstack([np.round(r), np.round(a * 255)]).astype(np.uint8), "RGBA")
            for r, a in zip(out_rgb, out_a)]
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    write_webp_anim(args.out, imgs, durations, args.quality)
    print(f"写出 {args.out}：{m} 帧，{sum(durations)} 毫秒，{os.path.getsize(args.out) / 1024:.0f} KB")

    if args.preview:
        pick = [0, 1, 2, 4, m // 4, m // 2, 3 * m // 4, m - 3, m - 1]
        tiles = []
        yy, xx = np.mgrid[0:args.size, 0:args.size]
        checker = np.where(((yy // 20 + xx // 20) % 2)[..., None] == 0, 200.0, 150.0)
        for t in pick:
            a = out_a[t][..., None]
            tile = (out_rgb[t] * a + checker * (1 - a)).astype(np.uint8)
            cv2.putText(tile, str(t), (6, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 2)
            tiles.append(tile)
        sheet = np.vstack([np.hstack(tiles[r * 3:(r + 1) * 3]) for r in range(3)])
        Image.fromarray(sheet).save(args.preview)
        print(f"预览：{args.preview}")


if __name__ == "__main__":
    main()
