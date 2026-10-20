// 给 ClauPet-intro.mp4 配上声音：用代码合成原创的背景音乐 + 跟画面同步的音效。
// 用法：先 node render.mjs 渲染出视频，再 node audio.mjs
//   → 生成 audio.wav，并把它合成进 ClauPet-intro.mp4（原地替换音轨，可以反复运行）
//   node audio.mjs --short → 给 30 秒精简版 ClauPet-intro-30s.mp4 配音：音效时间按 cuts.js 的剪辑表换算
// 注意：下面的时间点是照着 intro.html 里的时间轴抄过来的，改动画时间时要一起改。
import { writeFileSync, renameSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import './cuts.js';

const HERE = dirname(fileURLToPath(import.meta.url));
const SHORT = process.argv.includes('--short');
const SR = 44100, DUR = SHORT ? SHORT_CUT.duration : 67, N = SR * DUR;

// 精简版：音效写的是原 67 秒时间轴上的时间，在这里换算成新时间；被剪掉的部分返回 null（不发声）
function remap(t) {
  if (!SHORT) return t;
  for (const seg of SHORT_CUT.segments) {
    const o0 = seg.map[0][1], o1 = seg.map[seg.map.length - 1][1];
    if (t >= o0 && t <= o1) return interpKnots(seg.map, t, 1, 0);
  }
  return null;
}
const music = [new Float32Array(N), new Float32Array(N)];
const sfx = [new Float32Array(N), new Float32Array(N)];

// 固定种子的随机数，保证每次生成的声音一样
let seed = 12345;
const rand = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);

const midi = n => 440 * Math.pow(2, (n - 69) / 12);

/* ---------- 基础发声 ---------- */
// 一个音：频率可以从 f 滑到 f2；包络 = 起音 a 秒 + 指数衰减（decay 秒）或保持；结尾 release 秒淡出
function tone(bus, t0, { f, f2 = f, dur, a = 0.005, decay = 0, release = 0.03, vol = 0.2, harm = [[1, 1]], pan = 0, vib = 0, vibRate = 6, wave = 'sine' }) {
  const t = bus === sfx ? remap(t0) : t0;
  if (t === null) return;
  const start = Math.round(t * SR), len = Math.round(dur * SR);
  const gl = Math.cos((pan + 1) * Math.PI / 4), gr = Math.sin((pan + 1) * Math.PI / 4);
  let phase = 0;
  for (let i = 0; i < len; i++) {
    const n = start + i;
    if (n < 0 || n >= N) continue;
    const x = i / SR;
    const freq = f * Math.pow(f2 / f, x / dur) * (1 + vib * Math.sin(2 * Math.PI * vibRate * x));
    phase += freq / SR;
    let env = x < a ? x / a : (decay ? Math.exp(-(x - a) / decay) : 1);
    env *= Math.min(1, (dur - x) / release);
    let s = 0;
    for (const [m, amp] of harm) {
      const ph = (phase * m) % 1;
      s += amp * (wave === 'tri' ? 1 - 4 * Math.abs(ph - 0.5) : wave === 'square' ? (ph < 0.5 ? 1 : -1) : Math.sin(2 * Math.PI * ph));
    }
    bus[0][n] += s * env * vol * gl;
    bus[1][n] += s * env * vol * gr;
  }
}
// 噪声：低通截止频率从 lp 扫到 lp2；hp=true 时只留高频（像沙沙声）；shape 决定音量包络
function noise(bus, t0, { dur, vol = 0.1, lp = 2000, lp2 = lp, hp = false, pan = 0, shape = 'tri' }) {
  const t = bus === sfx ? remap(t0) : t0;
  if (t === null) return;
  const start = Math.round(t * SR), len = Math.round(dur * SR);
  const gl = Math.cos((pan + 1) * Math.PI / 4), gr = Math.sin((pan + 1) * Math.PI / 4);
  let y = 0;
  for (let i = 0; i < len; i++) {
    const n = start + i;
    if (n < 0 || n >= N) continue;
    const x = i / len;
    const cut = lp * Math.pow(lp2 / lp, x);
    const k = 1 - Math.exp(-2 * Math.PI * cut / SR);
    const w = rand() * 2 - 1;
    y += k * (w - y);
    const s = hp ? w - y : y;
    const env = shape === 'tri' ? 1 - Math.abs(2 * x - 1) : Math.exp(-x * 6);
    bus[0][n] += s * env * vol * gl;
    bus[1][n] += s * env * vol * gr;
  }
}
// 八音盒风格的音符
const bell = (bus, t, f, vol = 0.12, pan = 0, decay = 0.35) =>
  tone(bus, t, { f, dur: decay * 4, decay, vol, pan, harm: [[1, 1], [2, 0.25], [4.02, 0.12]] });

/* ---------- 音效 ---------- */
const S = sfx;
const pop = (t, f = 520, vol = 0.22) => tone(S, t, { f, f2: f * 1.9, dur: 0.13, decay: 0.05, vol, harm: [[1, 1], [2, 0.2]] });
const boing = t => tone(S, t, { f: 260, f2: 720, dur: 0.32, decay: 0.15, vol: 0.2, vib: 0.04, vibRate: 18 });
const click = (t, pan = 0.3) => { noise(S, t, { dur: 0.025, vol: 0.5, lp: 8000, hp: true, shape: 'exp', pan }); tone(S, t, { f: 1800, dur: 0.03, decay: 0.01, vol: 0.12, pan }); };
const tick = t => { noise(S, t, { dur: 0.02, vol: 0.18 + rand() * 0.08, lp: 5000, hp: true, shape: 'exp', pan: -0.3 }); tone(S, t, { f: 1100 + rand() * 300, dur: 0.015, decay: 0.006, vol: 0.05, pan: -0.3 }); };
const whoosh = (t, dur = 0.5, vol = 0.12, up = true) => noise(S, t, { dur, vol, lp: up ? 300 : 2500, lp2: up ? 2500 : 250 });
const fwip = (t, vol = 0.07) => tone(S, t, { f: 500, f2: 1300, dur: 0.16, decay: 0.08, vol, pan: 0.2 });
const typing = (a, b, n) => { for (let i = 0; i < n; i++) tick(a + (b - a) * (i / Math.max(1, n - 1)) + (rand() - 0.5) * 0.03); };

// 宠物切换状态时的提示音，每种状态一个
function chime(t, state) {
  if (state === 'working') { bell(S, t, midi(76), 0.16, 0.3); bell(S, t + 0.09, midi(81), 0.16, 0.3); }
  if (state === 'waiting') {
    for (const dt of [0, 0.16]) tone(S, t + dt, { f: midi(83), dur: 0.5, decay: 0.12, vol: 0.2, pan: 0.3, harm: [[1, 1], [2, 0.4], [3, 0.15]] });
  }
  if (state === 'idle') [84, 79, 76].forEach((m, i) => bell(S, t + i * 0.08, midi(m), 0.12, 0.3));
  if (state === 'error') {
    tone(S, t, { f: midi(69), f2: midi(66), dur: 0.32, a: 0.02, decay: 0.2, vol: 0.14, wave: 'tri', vib: 0.015, pan: 0.3 });
    tone(S, t + 0.3, { f: midi(65), f2: midi(61), dur: 0.6, a: 0.02, decay: 0.3, vol: 0.14, wave: 'tri', vib: 0.02, pan: 0.3 });
  }
}

/* S1 标题 */
boing(0.15);
[84, 86, 88, 91, 93, 96, 98].forEach((m, i) => bell(S, 0.6 + i * 0.07, midi(m), 0.07, -0.2, 0.25));
pop(1.3, 700, 0.1);
[[2.0, 'working'], [2.6, 'waiting'], [3.2, 'error'], [3.8, 'idle']].forEach(([t, s]) => chime(t, s));
/* 转场 */
for (const t of [4.6, 9.8, 34.8, 44.9, 53.1, 59.9]) whoosh(t, 0.6, 0.06);
/* S2 痛点 */
pop(5.2, 420, 0.12); whoosh(6.2, 0.6, 0.14); boing(8.7); chime(9.0, 'waiting');
/* S3 桌面演示 */
pop(10.4, 600, 0.1);
typing(11.6, 13.3, 11); typing(29.0, 30.0, 8);
const FLY = 0.6;
let prev = 'idle';
for (const [t0, s] of [[13.8, 'working'], [14.7, 'working'], [15.9, 'working'], [17.1, 'working'], [19.5, 'waiting'],
                       [25.3, 'working'], [27.4, 'idle'], [30.4, 'working'], [31.6, 'error']]) {
  fwip(t0);
  if (s !== prev) chime(t0 + FLY, s); else pop(t0 + FLY, 900, 0.05);
  prev = s;
}
whoosh(13.8, 0.25, 0.08);                     // 发送
whoosh(30.4, 0.25, 0.08);
whoosh(18.0, 0.7, 0.14);                      // 浏览器滑入
click(23.0); whoosh(23.15, 0.45, 0.1, false); // 单击宠物，浏览器最小化
click(25.2, -0.1);                            // 点“允许”
/* S4 原理 */
pop(35.2, 480, 0.1); pop(35.4, 560, 0.1); whoosh(35.8, 0.4, 0.07);
[[36.6, 'working'], [38.2, 'waiting'], [39.8, 'idle']].forEach(([t0, s]) => { fwip(t0, 0.09); chime(t0 + 0.7, s); });
fwip(41.0, 0.09);
tone(S, 41.7, { f: 520, f2: 70, dur: 0.6, decay: 0.3, vol: 0.06, wave: 'square' });   // ClauPet 离线
fwip(42.4, 0.09);
tone(S, 42.75, { f: 110, dur: 0.22, decay: 0.12, vol: 0.08, wave: 'square' });         // 请求失败
noise(S, 42.75, { dur: 0.15, vol: 0.12, lp: 3000, shape: 'exp' });
bell(S, 43.2, midi(84), 0.12); bell(S, 43.3, midi(88), 0.12);                           // 照常工作
/* S5 多会话 */
[45.5, 45.7, 45.9].forEach((t, i) => pop(t, 480 + i * 80, 0.1));
chime(46.1, 'working');
chime(47.7, 'waiting'); chime(49.4, 'working'); pop(50.6, 700, 0.06); chime(52.0, 'idle');
/* S6 小细节 */
[53.6, 53.85, 54.1, 54.35].forEach((t, i) => pop(t, 500 + i * 70, 0.1));
click(54.8, -0.5); click(56.2, -0.5); pop(56.5, 800, 0.14);   // 拖动、放下、钉住
pop(55.4, 650, 0.1);                                          // 右键菜单
click(55.9, -0.1); chime(56.1, 'working');                    // 单击切回
/* S7 结尾 */
[72, 76, 79, 84].forEach((m, i) => bell(S, 60.3 + i * 0.15, midi(m), 0.14, -0.3 + i * 0.2));
[84, 88, 91, 96].forEach((m, i) => bell(S, 61.0 + i * 0.06, midi(m), 0.09, 0, 0.6));

/* ---------- 背景音乐：96 BPM，C 大调，Fmaj7 → G → Em7 → Am7 循环 ---------- */
const M = music;
const BEAT = 60 / 96, BAR = BEAT * 4;
const CHORDS = [[53, 57, 60, 64], [55, 59, 62, 67], [52, 55, 59, 62], [57, 60, 64, 67]];
const MUSIC_END = DUR - 2.5;
const [BEAT_FROM, BEAT_TO] = SHORT ? [3, 25] : [10, 59];   // 节拍只在演示段出现
for (let bar = 0; bar * BAR < MUSIC_END; bar++) {
  const t = bar * BAR, ch = CHORDS[bar % 4];
  const last = (bar + 1) * BAR >= MUSIC_END;
  const len = last ? DUR - t - 0.1 : BAR + 0.3;
  // 铺底和弦
  ch.forEach((m, i) => tone(M, t, { f: midi(m + 12), dur: len, a: 0.5, release: 0.6, vol: 0.022, pan: -0.4 + i * 0.27, harm: [[1, 1], [2, 0.15]], vib: 0.003, vibRate: 4 + i }));
  // 低音
  tone(M, t, { f: midi(ch[0] - 12), dur: Math.min(len, BAR * 0.9), a: 0.02, decay: 1.2, vol: 0.09, harm: [[1, 1], [2, 0.3]] });
  if (!last) tone(M, t + BEAT * 2.5, { f: midi(ch[0] - 12), dur: BEAT * 1.3, a: 0.02, decay: 0.5, vol: 0.06, harm: [[1, 1], [2, 0.3]] });
  if (last) continue;
  // 八音盒琶音
  const pat = [0, 1, 2, 3, 2, 1, 3, 2];
  pat.forEach((k, j) => bell(M, t + j * BEAT / 2, midi(ch[k] + 24), 0.032 * (j % 2 ? 0.7 : 1), j % 2 ? 0.35 : -0.35, 0.3));
  // 演示段加上轻轻的节拍
  if (t >= BEAT_FROM && t < BEAT_TO) {
    for (let b = 0; b < 4; b++) {
      const tb = t + b * BEAT;
      if (b % 2 === 0) tone(M, tb, { f: 110, f2: 45, dur: 0.25, decay: 0.08, vol: 0.16 });
      noise(M, tb + BEAT / 2, { dur: 0.05, vol: 0.05, lp: 9000, hp: true, shape: 'exp', pan: 0.2 });
    }
  }
}
// 结尾长音
tone(M, MUSIC_END, { f: midi(72), dur: DUR - MUSIC_END - 0.05, a: 0.05, decay: 1.2, vol: 0.05, harm: [[1, 1], [2, 0.2], [4.02, 0.08]] });

/* ---------- 混音：简单回声 + 音量包络 + 标准化 ---------- */
function echo(bus, delay, fb, mix) {
  const d = Math.round(delay * SR);
  const L = Float32Array.from(bus[0]), R = Float32Array.from(bus[1]);
  for (let n = d; n < N; n++) { L[n] += R[n - d] * fb; R[n] += L[n - d] * fb; }   // 左右交替的回声
  for (let n = 0; n < N; n++) { bus[0][n] += (L[n] - bus[0][n]) * mix; bus[1][n] += (R[n] - bus[1][n]) * mix; }
}
echo(music, 0.31, 0.35, 0.35);
echo(sfx, 0.19, 0.2, 0.2);

const MUSIC_GAIN = 0.6;   // 背景音乐比音效低约 4.5 dB
const out = new Int16Array(N * 2);
let peak = 0;
const mix = [new Float32Array(N), new Float32Array(N)];
for (let n = 0; n < N; n++) {
  const t = n / SR;
  const fade = Math.min(1, t / 1.0, (DUR - t) / 1.5);
  for (let c = 0; c < 2; c++) {
    mix[c][n] = (music[c][n] * MUSIC_GAIN + sfx[c][n]) * fade;
    peak = Math.max(peak, Math.abs(mix[c][n]));
  }
}
const gain = 0.89 / peak;
for (let n = 0; n < N; n++) for (let c = 0; c < 2; c++) {
  const v = Math.tanh(mix[c][n] * gain * 1.1) / Math.tanh(1.1);   // 轻微软削波，防止爆音
  out[n * 2 + c] = Math.round(Math.max(-1, Math.min(1, v)) * 32767);
}

// 写 WAV 文件（16 位立体声 PCM）
const wav = Buffer.alloc(44 + out.byteLength);
wav.write('RIFF', 0); wav.writeUInt32LE(36 + out.byteLength, 4); wav.write('WAVE', 8);
wav.write('fmt ', 12); wav.writeUInt32LE(16, 16); wav.writeUInt16LE(1, 20); wav.writeUInt16LE(2, 22);
wav.writeUInt32LE(SR, 24); wav.writeUInt32LE(SR * 4, 28); wav.writeUInt16LE(4, 32); wav.writeUInt16LE(16, 34);
wav.write('data', 36); wav.writeUInt32LE(out.byteLength, 40);
Buffer.from(out.buffer).copy(wav, 44);
const wavFile = join(HERE, SHORT ? 'audio-30s.wav' : 'audio.wav');
writeFileSync(wavFile, wav);
console.log(wavFile, '已生成');

// 合成进视频：保留原视频画面，换上新音轨
const video = join(HERE, SHORT ? 'ClauPet-intro-30s.mp4' : 'ClauPet-intro.mp4'), tmp = join(HERE, 'ClauPet-intro.tmp.mp4');
const r = spawnSync('ffmpeg', ['-y', '-loglevel', 'error', '-i', video, '-i', wavFile, '-map', '0:v', '-map', '1:a',
  '-c:v', 'copy', '-c:a', 'aac', '-b:a', '192k', '-shortest', '-movflags', '+faststart', tmp], { stdio: 'inherit' });
if (r.status !== 0) throw new Error('ffmpeg 合成失败');
renameSync(tmp, video);
console.log('done →', video);
