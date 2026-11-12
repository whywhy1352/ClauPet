"""用两张状态原图做一段"只播一次"的交叉淡化过渡动画 WebP（占位用，等 Seedance 生成的正式过渡片段）。

第 0 帧就是起点原图，最后一帧就是终点原图，中间按缓动曲线（先慢后快再慢）在预乘透明度下混合。
这样和各状态的循环动画（都以自己的原图起止）首尾都能无缝接上。

用法（在项目根目录运行）：
  python anim/make_crossfade.py 起点.png 终点.png 输出.webp [--frames 36] [--ms 42] [--reverse 反向输出.webp]
--reverse：顺便把同样的帧倒过来再写一份（比如 idle→working 倒过来就是 working→idle）。
"""
import os
import sys
import argparse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from process_clip import np, Image, resize_rgba, write_webp_anim  # noqa: E402  （process_clip 会把 .pylib 加进路径）


def load_360(path):
    """读入原图，按预乘透明度缩到 360×360，返回预乘后的 RGB（0~255）和透明度（0~1）"""
    img = np.asarray(Image.open(path).convert("RGBA"), dtype=np.float32)
    rgb, alpha = resize_rgba(img[..., :3], img[..., 3] / 255.0, 360)
    return rgb * alpha[..., None], alpha


def to_image(premul, alpha):
    a = np.clip(alpha, 0, 1)
    rgb = np.where(a[..., None] > 1e-4, premul / np.maximum(a[..., None], 1e-4), 0)
    out = np.dstack([np.clip(rgb, 0, 255), a * 255]).round().astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("start")
    ap.add_argument("end")
    ap.add_argument("out")
    ap.add_argument("--frames", type=int, default=36, help="总帧数，含首尾两张原图")
    ap.add_argument("--ms", type=int, default=42, help="每帧时长（毫秒）")
    ap.add_argument("--reverse", help="倒序版本的输出路径")
    args = ap.parse_args()

    p0, a0 = load_360(args.start)
    p1, a1 = load_360(args.end)
    frames = []
    for i in range(args.frames):
        t = i / (args.frames - 1)
        t = t * t * (3 - 2 * t)  # smoothstep：开头和结尾慢，中间快
        frames.append(to_image(p0 * (1 - t) + p1 * t, a0 * (1 - t) + a1 * t))
    durations = [args.ms] * len(frames)
    write_webp_anim(args.out, frames, durations, quality=85, loop=1)
    print(f"{args.out}：{len(frames)} 帧，{sum(durations)} 毫秒，只播一次")
    if args.reverse:
        write_webp_anim(args.reverse, frames[::-1], durations, quality=85, loop=1)
        print(f"{args.reverse}：倒序版本")


if __name__ == "__main__":
    main()
