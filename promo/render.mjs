// 把 intro.html 逐帧渲染成 MP4。
// 用法：
//   node render.mjs                     → 逐帧截图到 frames/，再合成 ClauPet-intro.mp4
//                                         （中途中断后再运行一次，会从已有的帧接着渲染）
//   node render.mjs --stills 3,20.5,40  → 只截这几个时间点的静帧（看效果用）
//   加上 --short                        → 渲染 30 秒精简版（剪辑表见 cuts.js），输出 ClauPet-intro-30s.mp4
// 依赖：Microsoft Edge（或 Chrome）、ffmpeg（在 PATH 里）。不需要 npm install。
import { spawn, spawnSync } from 'node:child_process';
import { mkdirSync, writeFileSync, existsSync, rmSync } from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { dirname, join } from 'node:path';
import { tmpdir } from 'node:os';

const HERE = dirname(fileURLToPath(import.meta.url));
const FPS = 30, W = 1920, H = 1080, PORT = 9333, CALL_TIMEOUT = 20000;
const args = process.argv.slice(2);
const stillsArg = args.includes('--stills') ? args[args.indexOf('--stills') + 1] : null;
const SHORT = args.includes('--short');
const outFile = join(HERE, SHORT ? 'ClauPet-intro-30s.mp4' : 'ClauPet-intro.mp4');
const framesDir = join(HERE, SHORT ? 'frames-30s' : 'frames');
const frameName = i => join(framesDir, `f${String(i).padStart(5, '0')}.png`);

const BROWSERS = [
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
];
const browserPath = BROWSERS.find(existsSync);
if (!browserPath) throw new Error('找不到 Edge 或 Chrome');

const sleep = ms => new Promise(r => setTimeout(r, ms));

// 启动一个无头浏览器，返回 { send, evaluate, close }
async function openBrowser() {
  const profile = join(tmpdir(), 'claupet-render-profile');
  const url = pathToFileURL(join(HERE, 'intro.html')).href + (SHORT ? '?render&short' : '?render');
  const proc = spawn(browserPath, [
    '--headless=new', `--remote-debugging-port=${PORT}`, `--user-data-dir=${profile}`,
    `--window-size=${W},${H}`, '--hide-scrollbars', '--force-device-scale-factor=1',
    '--allow-file-access-from-files', '--no-first-run',
    // 防止无头页面被当成“后台页面”而暂停出帧（上次卡死的原因）
    '--disable-background-timer-throttling', '--disable-renderer-backgrounding',
    '--disable-backgrounding-occluded-windows', url,
  ], { stdio: 'ignore' });

  let wsUrl = null;
  for (let i = 0; i < 100 && !wsUrl; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${PORT}/json`)).json();
      wsUrl = list.find(x => x.type === 'page' && x.url.includes('intro.html'))?.webSocketDebuggerUrl;
    } catch { /* 浏览器还没起来 */ }
    if (!wsUrl) await sleep(200);
  }
  if (!wsUrl) { proc.kill(); throw new Error('连不上浏览器的调试端口'); }

  const ws = new WebSocket(wsUrl);
  await new Promise(r => ws.addEventListener('open', r, { once: true }));
  let nextId = 1;
  const pending = new Map();
  ws.addEventListener('message', e => {
    const msg = JSON.parse(e.data);
    if (msg.id && pending.has(msg.id)) {
      const { resolve, reject, timer } = pending.get(msg.id);
      clearTimeout(timer);
      pending.delete(msg.id);
      msg.error ? reject(new Error(JSON.stringify(msg.error))) : resolve(msg.result);
    }
  });
  // 每个调用都有超时：浏览器卡住时报错，而不是永远等下去
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const id = nextId++;
    const timer = setTimeout(() => { pending.delete(id); reject(new Error(`${method} 超时`)); }, CALL_TIMEOUT);
    pending.set(id, { resolve, reject, timer });
    ws.send(JSON.stringify({ id, method, params }));
  });
  const evaluate = async expr => (await send('Runtime.evaluate', { expression: expr, returnByValue: true })).result.value;

  await send('Emulation.setDeviceMetricsOverride', { width: W, height: H, deviceScaleFactor: 1, mobile: false });
  for (let i = 0; !(await evaluate('window.__ready === true')); i++) {
    if (i > 300) throw new Error('页面没加载完');
    await sleep(100);
  }
  const close = () => { try { ws.close(); } catch {} proc.kill(); };
  return { send, evaluate, close };
}

async function frameAt(b, t) {
  await b.evaluate(`render(${t})`);
  const { data } = await b.send('Page.captureScreenshot', {
    format: 'png', optimizeForSpeed: true, clip: { x: 0, y: 0, width: W, height: H, scale: 1 },
  });
  return Buffer.from(data, 'base64');
}

if (stillsArg) {
  const b = await openBrowser();
  try {
    const dir = join(HERE, SHORT ? 'stills-30s' : 'stills');
    mkdirSync(dir, { recursive: true });
    for (const s of stillsArg.split(',')) {
      writeFileSync(join(dir, `t${s}.png`), await frameAt(b, +s));
      console.log('still', s);
    }
  } finally { b.close(); }
  process.exit(0);
}

// ---- 逐帧渲染（可续传）----
mkdirSync(framesDir, { recursive: true });
let b = await openBrowser();
const total = Math.round((await b.evaluate('window.DURATION')) * FPS);
const t0 = Date.now();
let retries = 0;
for (let i = 0; i < total; i++) {
  if (existsSync(frameName(i))) continue;
  try {
    writeFileSync(frameName(i), await frameAt(b, i / FPS));
    retries = 0;
  } catch (err) {
    // 浏览器卡住：重启它，从这一帧重新来
    console.log(`frame ${i} 出错：${err.message}，重启浏览器`);
    b.close();
    if (++retries > 5) throw err;
    await sleep(1000);
    b = await openBrowser();
    i--;
    continue;
  }
  if (i % 60 === 0) console.log(`frame ${i}/${total}  (${((Date.now() - t0) / 1000).toFixed(0)}s)`);
}
b.close();

console.log('所有帧已就绪，开始编码…');
const r = spawnSync('ffmpeg', [
  '-y', '-loglevel', 'error', '-framerate', String(FPS), '-i', join(framesDir, 'f%05d.png'),
  '-c:v', 'libx264', '-preset', 'slow', '-crf', '18', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', outFile,
], { stdio: 'inherit' });
if (r.status !== 0) throw new Error('ffmpeg 编码失败');
if (!args.includes('--keep-frames')) rmSync(framesDir, { recursive: true, force: true });
console.log('done →', outFile);
