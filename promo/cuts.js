// 30 秒精简版的剪辑表。intro.html 和 audio.mjs 都读这个文件，保证画面和声音对得上。
// 每一段：在新时间轴的 from~to 秒，播放原来 67 秒版本里的某个场景；
// map 是 [新时间, 原时间] 的关键点，中间按直线插值——两点间原时间跨度更大，就是加速播放。
// 相邻两段在时间上重叠一点点，重叠的部分做交叉淡化。
globalThis.SHORT_CUT = {
  duration: 30,
  segments: [
    // 标题：原 0~4 秒，稍微加速
    { scene: 's1', from: 0, to: 3.2, map: [[0, 0], [3.2, 4.0]] },
    // 桌面演示：原 10~34.7 秒，整体约 1.3 倍速，打字和等待的地方压得多一点
    { scene: 's3', from: 3.0, to: 21.4, map: [
      [3.0, 10.0], [4.0, 11.6], [5.5, 13.8], [8.1, 18.0], [9.9, 20.1], [12.1, 23.0],
      [14.3, 25.4], [16.5, 28.2], [17.1, 29.0], [19.5, 32.3], [21.4, 34.7]] },
    // 工作原理：只保留前两个事件
    { scene: 's4', from: 21.1, to: 25.6, map: [[21.1, 34.9], [25.6, 39.6]] },
    // 结尾
    { scene: 's7', from: 25.3, to: 30, map: [[25.3, 59.9], [28.0, 63.3], [30, 67]] },
  ],
  // 精简版用自己的字幕（新时间轴）
  captions: [
    [3.3, 5.4, '她待在桌面一角，随时感知 Claude 的状态', 'She sits on your desktop, sensing what Claude is up to'],
    [5.6, 8.0, 'Claude 在工作，她也在认真读书', "Claude's working — and so is she"],
    [8.2, 9.8, '你切去忙别的……', 'You switch to something else…'],
    [9.9, 12.0, 'Claude 需要你批准时，她举手提醒你', 'When Claude needs your approval, she raises her hand'],
    [12.1, 14.2, '单击她，立刻切回 Claude', 'Click her to jump back to Claude'],
    [16.3, 17.9, '完成了，她安静地微笑', "Done — she smiles"],
    [19.5, 21.3, '出错时，她会难过得掉眼泪', 'If something goes wrong, she cries'],
    [21.6, 25.3, '原理：Claude Code 的 HTTP hooks 把每个事件发给她', "Under the hood: Claude Code's HTTP hooks send her every event"],
  ],
};

// 在一组 [x, y] 关键点上做直线插值（超出两端就取端点值）
globalThis.interpKnots = (knots, x, xi = 0, yi = 1) => {
  if (x <= knots[0][xi]) return knots[0][yi];
  for (let k = 0; k < knots.length - 1; k++) {
    const a = knots[k], b = knots[k + 1];
    if (x <= b[xi]) return a[yi] + (b[yi] - a[yi]) * (x - a[xi]) / (b[xi] - a[xi]);
  }
  return knots[knots.length - 1][yi];
};
