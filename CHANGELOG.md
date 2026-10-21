# 更新日志 / Changelog

本文件记录 ClauPet 每个版本的主要变化，最新的版本在最上面。
格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)。

All notable changes to ClauPet are documented here, newest first.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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

[1.1.0]: https://github.com/whywhy1352/ClauPet/releases/tag/v1.1.0
[1.0.0]: https://github.com/whywhy1352/ClauPet/releases/tag/v1.0.0
