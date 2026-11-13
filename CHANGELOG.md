# 更新日志 / Changelog

本文件记录 ClauPet 每个版本的主要变化，最新的版本在最上面。
格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

All notable changes to ClauPet are documented here, newest first.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.2.0] - 2026-11-12

### 新增 / Added

- **动画**：4 种状态和"被拎起来"都从静态图片换成了动画：空闲时头发轻轻飘动、偶尔眨眼，工作时书页被风吹得翻动，出错时眼泪落下，被拎起来时晃来晃去
  **Animations**: all four states and the "lifted" pose are now animated instead of still images: her hair sways and she blinks while idle, the pages of her book flutter while working, tears fall on error, and she swings while being dragged
- **过渡动画**：状态之间切换时会播放一段约 1 秒的过渡动画（4 种状态两两之间共 12 段），不再一下子换成另一张图
  **Transitions**: switching between states now plays a short (about 1 second) transition animation, 12 in total covering every pair of states, instead of jumping straight to another picture
- 托盘菜单新增"重新加载"：拖到另一块屏幕后画面不清晰时，可以手动按当前屏幕重新加载动画
  New "reload" item in the tray menu: reloads the animation for the current monitor if the pet looks blurry after moving to another one

### 修复 / Fixed

- 拖到缩放比例不同的另一块屏幕时，宠物会被裁掉一部分或者发虚
  The pet could be cropped or blurry after being dragged to a monitor with a different scaling factor

### 变更 / Changed

- 状态改变时，宠物会先把当前这一轮动画播完再过渡，所以画面会比实际状态晚几秒（最多约 6 秒）
  When the state changes, the pet finishes the current animation loop before transitioning, so the display can lag behind the actual state by a few seconds (about 6 seconds at most)
- 动画素材都编进了程序，发布包变大了（`ClauPet.exe` 约 27 MB），运行时内存约 100 MB；待机时 CPU 占用很低
  The animations are built into the program, so the release package is larger (`ClauPet.exe` is about 27 MB) and memory use is about 100 MB; CPU use while idle stays low
- 从源码编译需要额外安装 Qt Image Formats 模块（用来播放 WebP 动画），打包后要确认带上了 `imageformats\qwebp.dll`
  Building from source now also requires the Qt Image Formats module (for WebP animations); make sure `imageformats\qwebp.dll` is included when packaging

### 已知问题 / Known issues

- 按 Esc 中途打断 Claude 时，宠物可能会一直显示"工作中"，直到下一次回复完毕
  If you interrupt Claude with Esc, the pet may keep showing "working" until the next response finishes

## [1.1.0] - 2026-10-21

### 新增 / Added

- **气泡提示**：Claude 调用工具时，宠物头顶显示"会话标题：工具名"，同时开着多个会话时，一眼就能看出是哪个在工作（#5）
  **Speech bubble**: when Claude uses a tool, a bubble above the pet shows "session title: tool name", so you can tell which session is busy (#5)
- **悬停浮动**：把鼠标放在宠物身上，她会轻轻地上下浮动（#4）
  **Float on hover**: hover over the pet and she gently floats up and down (#4)
- **拖动时被拎起来**：拖动宠物时，她会像被拎起后领一样悬在空中（#6）
  **Lifted while dragged**: she dangles as if lifted by her collar while you drag her (#6)

### 修复 / Fixed

- 出错状态会在几秒后自动恢复为空闲，不再一直哭下去（#2）
  The error state now returns to idle after a few seconds (#2)
- 重复启动时会弹窗提示端口已被占用并退出，不再悄悄多出一只不会动的宠物（#3）
  Starting a second instance now shows a message that the port is in use and exits, instead of leaving a pet that never reacts (#3)

### 变更 / Changed

- 窗口顶部加高，留出了显示气泡的区域。升级后第一次启动时，宠物的位置可能会比原来稍微靠下一点，拖动一次即可重新摆好
  The window is taller to make room for the bubble. After upgrading, the pet may appear slightly lower than before; drag it once to reposition

## [1.0.0] - 2026-10-19

第一个公开版本。/ First public release.

### 新增 / Added

- 通过 Claude Code 的 HTTP hooks 实时显示 4 种状态：空闲、工作中、需要你介入、出错
  Four states driven by Claude Code HTTP hooks: idle, working, needs your input, and error
- 支持多个会话同时工作 / Works with multiple sessions at once
- 单击宠物切换到 Claude 桌面应用的窗口 / Click the pet to bring the Claude desktop app's window to the front
- 拖动摆放并记住位置；托盘图标和右键菜单 / Drag it anywhere (position is remembered); tray icon and right-click menu
- 透明背景、无边框、始终置顶；在高分辨率屏幕上清晰显示 / Transparent, frameless, always on top, and crisp on high-DPI displays

[1.2.0]: https://github.com/whywhy1352/ClauPet/releases/tag/v1.2.0
[1.1.0]: https://github.com/whywhy1352/ClauPet/releases/tag/v1.1.0
[1.0.0]: https://github.com/whywhy1352/ClauPet/releases/tag/v1.0.0
