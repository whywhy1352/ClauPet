"""把 Seedance 首尾帧模式生成的过渡视频（首帧 = 起点状态原图，尾帧 = 终点状态原图）处理成"只播一次"的 WebP。

和循环片段（process_clip.py）的区别：
  - 第 0 帧换成起点原图，最后一帧换成终点原图（最后一帧不丢）。这样过渡的开头接得上起点循环、
    结尾接得上终点循环（两段循环都以各自的原图起止）
  - 颜色校正的目标在两张原图之间按时间插值（开头对齐起点原图，结尾对齐终点原图）
  - 去掉视频开头和结尾"还没开始动 / 已经停下"的静止部分，再按目标时长等比例加速
  - 编码成只播一次（WebP 循环次数 1：Qt 读出来 loopCount() 是 0，QMovie 播完停在最后一帧并发出 finished()）
  - 可以顺便输出倒序的版本（不涉及眼泪的过渡，反方向直接倒放）

用法（在项目根目录运行）：
  python anim/process_transition.py 视频.mp4 --from assets/pet/idle.png --to assets/pet/working.png \
      --out anim/idle_to_working/out/idle_to_working.webp [--reverse-out .../working_to_idle.webp] \
      [--seconds 1.5] [--screen blue|green] [--fade 0] [--preview 预览.png]
"""
import os
import sys
import argparse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from process_clip import (np, cv2, Image, decode, estimate_bg, key_alpha, clean_alpha, unmix,  # noqa: E402
                          hist_lut, resize_rgba, write_webp_anim, SCREEN_CH)


def load_orig(path):
    img = np.asarray(Image.open(path).convert("RGBA"))
    assert img.shape[0] == img.shape[1], "原图应为正方形"
    return img[..., :3].astype(np.float32), img[..., 3].astype(np.float32) / 255.0


def check_align(frame, o_rgb, o_a, canvas, offset, screen):
    """视频某一帧和"原图放在纯色底上"的输入图对比，返回偏移量和相关性"""
    w = frame.shape[0]
    osz = o_rgb.shape[0]
    inp = np.zeros((canvas, canvas, 3), np.float32)
    inp[..., SCREEN_CH[screen][0]] = 255
    win = (slice(offset, offset + osz), slice(offset, offset + osz))
    inp[win] = o_rgb * o_a[..., None] + inp[win] * (1 - o_a[..., None])
    inp = cv2.resize(inp, (w, w), interpolation=cv2.INTER_AREA)
    (dx, dy), resp = cv2.phaseCorrelate(cv2.cvtColor(inp, cv2.COLOR_RGB2GRAY),
                                        cv2.cvtColor(frame.astype(np.float32), cv2.COLOR_RGB2GRAY))
    return dx, dy, resp


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("video")
    ap.add_argument("--from", dest="src", required=True, help="起点状态的原图")
    ap.add_argument("--to", dest="dst", required=True, help="终点状态的原图")
    ap.add_argument("--out", required=True)
    ap.add_argument("--reverse-out", help="同时输出倒序版本（反方向的过渡）")
    ap.add_argument("--seconds", type=float, default=1.5, help="输出时长")
    ap.add_argument("--fps", type=float, default=24.0)
    ap.add_argument("--canvas", type=int, default=1440)
    ap.add_argument("--offset", type=int, default=93)
    ap.add_argument("--size", type=int, default=360)
    ap.add_argument("--matrix", default="bt601")
    ap.add_argument("--screen", choices=["blue", "green"], default="blue")
    ap.add_argument("--key-lo", type=float, default=0.04)
    ap.add_argument("--key-hi", type=float, default=0.90)
    ap.add_argument("--spill-margin", type=float, default=4.0)
    ap.add_argument("--still", type=float, default=1.5,
                    help="去掉开头/结尾的静止部分：和首帧（或尾帧）的平均差小于这个值的帧算静止；0 = 不去")
    ap.add_argument("--fade", type=int, default=0, help="首尾各几帧和原图交叉淡化")
    ap.add_argument("--motion-weight", type=float, default=0.0,
                    help="0 = 均匀加速；越接近 1，越按动作量分配输出帧（慢的地方多跳帧、快的地方多留帧），"
                         "用于视频里某一小段动作特别快、均匀加速后像跳了一下的情况")
    ap.add_argument("--quality", type=int, default=85)
    ap.add_argument("--preview")
    args = ap.parse_args()

    frames = decode(args.video, args.matrix)
    n, h, w, _ = frames.shape
    assert h == w, "这个脚本只处理正方形视频"
    a_rgb, a_a = load_orig(args.src)
    b_rgb, b_a = load_orig(args.dst)
    assert a_rgb.shape == b_rgb.shape, "起点和终点原图的尺寸要一样"
    osz = a_rgb.shape[0]
    scale = w / args.canvas
    x0 = int(round(args.offset * scale))
    side = int(round(osz * scale))
    print(f"{n} 帧，{w}x{h}；原图 {osz}x{osz}，在视频里从 {x0} 起，边长 {side}（缩到 {args.size} 是缩小 {side / args.size:.2f} 倍）")

    dx, dy, r = check_align(frames[0], a_rgb, a_a, args.canvas, args.offset, args.screen)
    print(f"首帧 vs 起点输入图：dx={dx:+.2f} dy={dy:+.2f}（相关 {r:.2f}）")
    dx, dy, r = check_align(frames[-1], b_rgb, b_a, args.canvas, args.offset, args.screen)
    print(f"尾帧 vs 终点输入图：dx={dx:+.2f} dy={dy:+.2f}（相关 {r:.2f}）")

    # 颜色校正的两个目标
    ra_rgb, ra_a = resize_rgba(a_rgb, a_a, side)
    rb_rgb, rb_a = resize_rgba(b_rgb, b_a, side)
    ref_a = ra_rgb[ra_a >= 0.98].astype(np.uint8)
    ref_b = rb_rgb[rb_a >= 0.98].astype(np.uint8)

    fgs, alphas, luts, bgs = [], [], [], []
    min_area = int(0.0005 * w * h)
    for t in range(n):
        bg = estimate_bg(frames[t])
        al = clean_alpha(key_alpha(frames[t], bg, args.key_lo, args.key_hi, args.screen), min_area)
        fg = unmix(frames[t], al, bg, args.spill_margin, args.screen)
        src = fg[al >= 0.98].astype(np.uint8)
        wt = t / (n - 1)
        lut = np.stack([(1 - wt) * hist_lut(src[:, c], ref_a[:, c]) + wt * hist_lut(src[:, c], ref_b[:, c])
                        for c in range(3)])
        fgs.append(fg); alphas.append(al); luts.append(lut); bgs.append(bg)
    luts = np.stack(luts)
    smooth = luts.copy()
    for t in range(1, n - 1):
        smooth[t] = luts[max(1, t - 2):min(n - 2, t + 2) + 1].mean(axis=0)

    # 找动作真正开始和结束的帧（在原视频尺寸的角色区域里比较）
    crop = (slice(x0, x0 + side), slice(x0, x0 + side))
    g = [cv2.resize(cv2.cvtColor(f, cv2.COLOR_RGB2GRAY)[crop], (180, 180), interpolation=cv2.INTER_AREA).astype(np.float32)
         for f in frames]
    d_first = np.array([np.abs(x - g[0]).mean() for x in g])
    d_last = np.array([np.abs(x - g[-1]).mean() for x in g])
    s, e = 0, n - 1
    if args.still > 0:
        moving = np.nonzero(d_first > args.still)[0]
        s = max(0, moving[0] - 1) if len(moving) else 0
        moving = np.nonzero(d_last > args.still)[0]
        e = min(n - 1, moving[-1] + 1) if len(moving) else n - 1
    print(f"动作从第 {s} 帧到第 {e} 帧（{(e - s) / args.fps:.2f} 秒）；开头静止 {s} 帧，结尾静止 {n - 1 - e} 帧")

    m = int(round(args.seconds * args.fps))
    if args.motion_weight > 0:
        # "进度"= 时间和累计动作量的加权平均；输出帧在进度上均匀取点
        step = np.array([0.0] + [np.abs(g[t + 1] - g[t]).mean() for t in range(s, e)])
        prog = (1 - args.motion_weight) * np.linspace(0, 1, e - s + 1) + args.motion_weight * np.cumsum(step) / step.sum()
        src_idx = [s + int(np.argmin(np.abs(prog - i / (m - 1)))) for i in range(m)]
    else:
        src_idx = [s + round(i * (e - s) / (m - 1)) for i in range(m)]
    out_rgb, out_a = [], []
    for i, t in enumerate(src_idx):
        if i == 0:
            rgb, al = resize_rgba(a_rgb, a_a, args.size)
        elif i == m - 1:
            rgb, al = resize_rgba(b_rgb, b_a, args.size)
        else:
            idx = np.clip(fgs[t], 0, 255).astype(np.uint8)
            corr = np.stack([smooth[t, c][idx[..., c]] for c in range(3)], axis=-1)
            rgb, al = resize_rgba(corr[crop], alphas[t][crop], args.size)
        out_rgb.append(rgb); out_a.append(al)

    # 首尾和原图交叉淡化（预乘空间）
    for k in range(1, args.fade + 1):
        wt = 1.0 - k / (args.fade + 1)
        for t, ref in ((k, 0), (m - 1 - k, m - 1)):
            pa = out_rgb[t] * out_a[t][..., None] * (1 - wt) + out_rgb[ref] * out_a[ref][..., None] * wt
            aa = out_a[t] * (1 - wt) + out_a[ref] * wt
            out_rgb[t] = np.where(aa[..., None] > 1e-4, pa / np.maximum(aa[..., None], 1e-4), 0)
            out_a[t] = aa

    union = np.max(np.stack([alphas[t] for t in src_idx[1:-1]]), axis=0) > 0.05
    over = (union[:, :x0].any() or union[:, x0 + side:].any() or union[:x0].any() or union[x0 + side:].any())
    if over:
        ys, xs = np.nonzero(union)
        print(f"  注意：有帧超出画布（x {xs.min()}–{xs.max()}，y {ys.min()}–{ys.max()}，画布 {x0}–{x0 + side - 1}）")

    def diff(i, j):
        pi = out_rgb[i] * out_a[i][..., None]
        pj = out_rgb[j] * out_a[j][..., None]
        return np.abs(pi - pj).mean() + np.abs(out_a[i] - out_a[j]).mean() * 255
    steps = [diff(t, t + 1) for t in range(m - 1)]
    print(f"相邻帧差：中位数 {np.median(steps[1:-1]):.2f}，最大 {max(steps[1:-1]):.2f}；"
          f"开头 0→1 {steps[0]:.2f}，结尾 {m - 2}→{m - 1} {steps[-1]:.2f}")
    bgs = np.array(bgs)
    print(f"背景：第 0 帧 {np.round(bgs[0]).astype(int)}，中间 {np.round(bgs[n // 2]).astype(int)}，最后 {np.round(bgs[-1]).astype(int)}")

    durations = [int(round((i + 1) * 1000 / args.fps)) - int(round(i * 1000 / args.fps)) for i in range(m)]
    imgs = [Image.fromarray(np.dstack([np.round(r), np.round(a * 255)]).astype(np.uint8), "RGBA")
            for r, a in zip(out_rgb, out_a)]
    for path, seq in ((args.out, imgs), (args.reverse_out, imgs[::-1])):
        if not path:
            continue
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        write_webp_anim(path, seq, durations, args.quality, loop=1)
        print(f"写出 {path}：{m} 帧，{sum(durations)} 毫秒，只播一次，{os.path.getsize(path) / 1024:.0f} KB")

    if args.preview:
        pick = np.linspace(0, m - 1, 12).round().astype(int)
        yy, xx = np.mgrid[0:args.size, 0:args.size]
        checker = np.where(((yy // 20 + xx // 20) % 2)[..., None] == 0, 200.0, 150.0)
        tiles = []
        for t in pick:
            a = out_a[t][..., None]
            tile = np.ascontiguousarray((out_rgb[t] * a + checker * (1 - a)).astype(np.uint8))
            cv2.putText(tile, str(t), (6, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 2)
            tiles.append(cv2.resize(tile, (240, 240), interpolation=cv2.INTER_AREA))
        sheet = np.vstack([np.hstack(tiles[r * 6:(r + 1) * 6]) for r in range(2)])
        Image.fromarray(sheet).save(args.preview)
        print(f"预览：{args.preview}")


if __name__ == "__main__":
    main()
