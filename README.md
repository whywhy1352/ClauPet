# ClauPet

[中文](#中文) | [English](#english)

https://github.com/user-attachments/assets/90fec8aa-7c5c-4e0a-9311-8e61b5b5b2b7

<p align="center">
  <img src="docs/screenshot.png" width="800" alt="ClauPet：Claude 申请权限时，宠物举手提醒你 / ClauPet raises her hand when Claude needs your approval">
</p>

<p align="center">
  <img src="assets/pet/idle.png" width="150" alt="idle">
  <img src="assets/pet/working.png" width="150" alt="working">
  <img src="assets/pet/waiting.png" width="150" alt="waiting">
  <img src="assets/pet/error.png" width="150" alt="error">
</p>

---

## 中文

ClauPet 是一个给 **Claude Code** 用的 Windows 桌面宠物。它浮在桌面上，通过 Claude Code 的 hooks 实时感知 Claude 的工作状态：Claude 在工作时，她捧着书认真阅读；Claude 需要你批准或者向你提问时，她举手提醒你；回复完毕时，她安静地微笑。

> 本项目是社区作品，与 Anthropic 无关。详见文末的[免责声明](#免责声明)。

### 功能

- **实时反映 Claude Code 的状态**：空闲、工作中、需要你介入、出错，共 4 种状态，每种状态都有自己的动画
- **状态之间有过渡动画**：比如从空闲变成工作中时，她会把书拿到身前、翻开，而不是一下子换成另一张图
- **支持多个会话同时工作**：一个会话结束了、另一个还在工作时，宠物不会错误地变回空闲
- **气泡提示**：Claude 调用工具时，宠物头顶会显示"会话标题：工具名"，开着多个会话时一眼就知道是哪个在干活
- **把鼠标放在宠物身上**：她会轻轻地上下浮动
- **单击宠物**：切换到 Claude 桌面应用的窗口
- **拖动宠物**：随意摆放，拖动时她会像被拎起后领一样晃来晃去；下次启动时会出现在上次的位置
- **右键菜单 / 托盘图标**：隐藏、显示、重新加载、退出
- 透明背景、无边框、始终置顶；在高分辨率屏幕上也能清晰显示，在缩放比例不同的多块屏幕之间拖动也会保持清晰

### 状态说明

| 状态 | 样子 | 什么时候出现 |
|---|---|---|
| 空闲 | 微笑，抱着书，头发轻轻飘动、偶尔眨眼 | 程序刚启动、Claude 回复完毕（`Stop`） |
| 工作中 | 捧着打开的书，书页被风吹得翻动 | 你发出消息（`UserPromptSubmit`）、Claude 调用工具（`PreToolUse` / `PostToolUse`） |
| 需要你介入 | 举手 + 感叹号 | Claude 申请权限（`PermissionRequest`），或者向你提问（`AskUserQuestion`） |
| 出错 | 流泪 | 因为 API 错误而中断（`StopFailure`） |

同时开着多个会话时，宠物显示其中**最需要关注**的那个状态，优先级为：**出错 > 需要你介入 > 工作中 > 空闲**。会话结束（`SessionEnd`）后，它的状态会被清除。

状态改变时，宠物会先把当前这一轮动画播完，再播放过渡动画切换过去，所以画面会比实际状态晚几秒（最多约 6 秒）。

### 工作原理

```
Claude Code ── HTTP POST（hook 事件的 JSON）──▶ ClauPet（监听 127.0.0.1:52790）
```

Claude Code 的 [HTTP hooks](https://code.claude.com/docs/en/hooks) 会在发生特定事件时，把事件信息以 JSON 的形式 POST 到一个网址。ClauPet 在本机的 `127.0.0.1:52790` 上接收这些请求，读取其中的 `hook_event_name`、`session_id` 和 `tool_name`，据此切换状态。

ClauPet 没有运行时，hook 的请求会连接失败。Claude Code 把这种情况当作非阻塞错误处理，**不会影响 Claude 正常工作**。

### 系统要求

- Windows 10 / 11（64 位）
- Claude Code。已在 Claude 桌面应用的 Code 标签中测试；命令行版 Claude Code 使用同一套 hooks 机制，理论上同样可用，但尚未测试
- Microsoft Visual C++ 运行库（大多数电脑已经装有；如果启动时提示缺少 `MSVCP140.dll` 或 `VCRUNTIME140.dll`，请从[微软官网](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist)下载安装 x64 版本）

### 安装

**1. 下载并运行**

在 [Releases](../../releases) 页面下载最新的 zip，解压到任意文件夹，双击 `ClauPet.exe`。宠物会出现在桌面上，任务栏右下角会出现托盘图标。

**2. 配置 Claude Code 的 hooks**

打开（或新建）Claude Code 的用户设置文件：

```
%USERPROFILE%\.claude\settings.json
```

把下面的内容写进去。如果文件里已经有别的设置，只需要把 `"hooks"` 这一项加进去，注意 JSON 的逗号；如果已经有 `"hooks"`，就把下面各个事件合并进去。

```json
{
  "hooks": {
    "UserPromptSubmit": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "PreToolUse": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "PostToolUse": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "PermissionRequest": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "Stop": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "StopFailure": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "SessionEnd": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ]
  }
}
```

`"timeout": 2` 表示每个请求最多等待 2 秒。万一 ClauPet 出现异常、收到请求却不回复，Claude 最多只会被拖慢 2 秒。

保存后即可生效，不需要重启 Claude Code。给 Claude 发一条消息，宠物就会开始工作。

**3. 开机自启（可选）**

按 `Win + R`，输入 `shell:startup` 并回车，把 `ClauPet.exe` 的快捷方式放进打开的文件夹。

### 使用

| 操作 | 效果 |
|---|---|
| 鼠标悬停在宠物身上 | 宠物轻轻地上下浮动 |
| 左键单击宠物 | 切换到 Claude 桌面应用的窗口（如果最小化了会先还原） |
| 左键拖动宠物 | 移动位置（拖动时会显示被拎起来的样子），松开时自动保存 |
| 右键宠物 / 右键托盘图标 | 菜单：隐藏或显示宠物、重新加载（拖到另一块屏幕后画面不清晰时用）、退出 |
| 先单击宠物，再按数字键 1–4 | 手动切换 4 种状态，用于测试 |

宠物隐藏后程序仍在后台运行，照常接收事件。退出程序请使用菜单里的退出项。

### 隐私与安全

- ClauPet 只监听 `127.0.0.1`（本机），其他电脑无法连接
- 它只读取事件中的 `hook_event_name`、`session_id`、`tool_name` 和 `session_title`，**不会保存到磁盘、也不会向任何地方发送**你的对话内容
- 注意：气泡会在屏幕上显示**会话标题**和**工具名**。共享屏幕、录屏或直播时，别人也能看到
- 本地保存的唯一数据是宠物的位置，存放在注册表 `HKEY_CURRENT_USER\Software\ClauPet` 中

### 常见问题

**宠物一直不动？**
- 确认 `settings.json` 的格式正确，并且各个事件都在 `"hooks"` 下面同一层级
- 确认只运行了一个 ClauPet。同时运行两个时，只有先启动的那个能接收事件
- 确认 `52790` 端口没有被其他程序占用

**启动时提示缺少 DLL？**
请安装上面"系统要求"中提到的 Visual C++ 运行库。

**宠物不显示，或者只有一片空白？**
宠物的动画是 WebP 格式，需要 Qt 的 WebP 插件才能播放。使用发布包时，请确认 `ClauPet.exe` 旁边有 `imageformats\qwebp.dll`（解压时要保留整个文件夹）；自己编译时，请确认安装了 Qt Image Formats 模块（见下面的"从源码构建"）。

**宠物一直显示"工作中"？**
如果你按 Esc 中途打断了 Claude，Claude Code 可能不会发送 `Stop` 事件，宠物就会一直停在工作中，直到下一次回复完毕。再给 Claude 发一条消息即可恢复。

### 从源码构建

需要：
- Visual Studio 2022 或更新版本，安装"使用 C++ 的桌面开发"工作负载
- Qt 6（已在 6.12.0 上测试），安装 **MSVC 2022 64-bit** 组件，以及 **Qt Image Formats** 模块（在 Qt 维护工具里位于 Qt 6.x → Additional Libraries 下）。宠物的动画是 WebP 格式，没有这个模块就无法播放，宠物会是一片空白
  - 装好后，确认 `<Qt 安装目录>\6.12.0\msvc2022_64\plugins\imageformats\` 里有 `qwebp.dll`。维护工具的安装摘要里，这个模块的大小可能显示为 0 bytes，以实际文件为准
- CMake 3.21 或更新版本（Visual Studio 自带）

步骤：
1. 克隆本仓库
2. 让 CMake 能找到 Qt：例如设置环境变量 `CMAKE_PREFIX_PATH` 为 `C:\Qt\6.12.0\msvc2022_64`（按你的安装路径修改），或者使用 Qt VS Tools 扩展
3. 在 Visual Studio 中通过"打开文件夹"打开项目，选择 Release 配置并生成，得到 `ClauPet.exe`
4. 打包依赖：

   ```
   windeployqt --release --no-translations ClauPet.exe
   ```

   完成后确认 `ClauPet.exe` 旁边有 `imageformats\qwebp.dll`，没有它就无法播放动画

> 提示：在非英文版的 Windows / Visual Studio 中使用 Ninja 生成器时，CMake 可能无法正确识别编译器的 `/showIncludes` 输出，导致修改头文件后相关的 `.cpp` 不会重新编译。可以在 CMake 预设的 `environment` 中加入 `"VSLANG": "1033"`（并安装 Visual Studio 的英语语言包），让编译器输出英文。

### 路线图

ClauPet 还在持续开发中，接下来计划做的事情：

- **更快的提醒**：需要你介入、出错这类状态不再等当前动画播完，立刻切换过去
- **更稳的状态**：会话异常结束（没有收到 `SessionEnd`）时，过一段时间自动清除它的状态，不再一直显示
- **其他候选**：可自定义端口；托盘菜单里的"开机启动"开关；测试并支持命令行版 Claude Code

欢迎在 [Issues](../../issues) 里提建议。各版本已经完成的改动见 [更新日志](CHANGELOG.md)。

### 许可

| 内容 | 许可 |
|---|---|
| 源代码 | [MIT License](LICENSE) |
| 角色图片和动画（`assets/pet/`） | [CC BY-NC-SA 4.0](assets/pet/LICENSE)，**不可商用** |
| Qt（发布包中附带的 DLL） | [LGPL v3](https://www.qt.io/licensing/open-source-lgpl-obligations)，以动态链接方式使用 |

MIT 许可证**只适用于源代码**，不适用于角色图片和动画。

### 致谢

- 角色原作：**ZipZipPipe** 的《Claude 拟人立绘·橘白礼服ver》（[来源](https://xn--pssy23gqgbz2d718b.com/works/claude202610030001.html)），采用 CC BY-NC-SA 4.0 许可。本项目中的 Q 版桌宠图片由 whywhy1352 以原作为参考、借助 AI 绘图工具创作，桌宠动画由这些图片借助 AI 视频工具生成，均经原作者同意发布，同样采用 CC BY-NC-SA 4.0 许可、不可商用。感谢 ZipZipPipe 的授权！
- 使用 [Qt](https://www.qt.io/) 开发

### 免责声明

"Claude" 是 Anthropic, PBC 的商标。ClauPet 是独立的社区项目，与 Anthropic 无关，未获得 Anthropic 的认可或赞助。

---

## English

ClauPet is a Windows desktop pet for **Claude Code**. It floats on your desktop and reacts to what Claude is doing in real time through Claude Code hooks: she reads her book while Claude is working, raises her hand when Claude needs your approval or asks you a question, and smiles quietly when Claude has finished.

> This is a community project and is not affiliated with Anthropic. See the [Disclaimer](#disclaimer).

### Features

- **Reflects Claude Code's state in real time**: idle, working, needs your input, and error, each with its own animation
- **Animated transitions between states**: for example, when going from idle to working she brings her book forward and opens it, instead of jumping straight to another picture
- **Works with multiple sessions**: if one session finishes while another is still working, the pet keeps showing "working"
- **Speech bubble**: when Claude uses a tool, a bubble above the pet shows "session title: tool name", so you can tell which session is busy at a glance
- **Hover over the pet** and she gently floats up and down
- **Click the pet** to bring the Claude desktop app's window to the front
- **Drag the pet** anywhere; she swings as if lifted by her collar while you drag, and remembers her position across restarts
- **Right-click menu / tray icon**: hide, show, reload, or quit
- Transparent, frameless, always on top, and crisp on high-DPI displays, including when you drag her between monitors with different scaling

### States

| State | Look | When |
|---|---|---|
| Idle | Smiling, holding a book, hair swaying gently, blinking now and then | On startup, and when Claude finishes responding (`Stop`) |
| Working | Reading an open book whose pages flutter in the breeze | You send a message (`UserPromptSubmit`), Claude uses a tool (`PreToolUse` / `PostToolUse`) |
| Needs your input | Raised hand with an exclamation mark | Claude requests permission (`PermissionRequest`) or asks you a question (`AskUserQuestion`) |
| Error | Crying | The turn ended because of an API error (`StopFailure`) |

With several sessions open, the pet shows the state that **most needs your attention**, in this order: **error > needs your input > working > idle**. When a session ends (`SessionEnd`), its state is removed.

When the state changes, the pet first finishes the current loop of its animation and then plays a transition, so the display can lag behind the actual state by a few seconds (about 6 seconds at most).

### How it works

```
Claude Code ── HTTP POST (hook event JSON) ──▶ ClauPet (listening on 127.0.0.1:52790)
```

Claude Code's [HTTP hooks](https://code.claude.com/docs/en/hooks) POST event data as JSON to a URL when certain events happen. ClauPet receives these requests on `127.0.0.1:52790`, reads `hook_event_name`, `session_id` and `tool_name`, and switches its state accordingly.

When ClauPet is not running, the hook requests simply fail to connect. Claude Code treats this as a non-blocking error, so **Claude keeps working normally**.

### Requirements

- Windows 10 / 11 (64-bit)
- Claude Code. Tested with the Code tab of the Claude desktop app; the Claude Code CLI uses the same hooks mechanism and should work too, but has not been tested yet
- Microsoft Visual C++ Redistributable (already present on most PCs; if you get an error about a missing `MSVCP140.dll` or `VCRUNTIME140.dll`, install the x64 version from [Microsoft](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist))

### Installation

**1. Download and run**

Download the latest zip from the [Releases](../../releases) page, extract it anywhere, and run `ClauPet.exe`. The pet appears on your desktop, along with an icon in the system tray.

**2. Configure Claude Code hooks**

Open (or create) your Claude Code user settings file:

```
%USERPROFILE%\.claude\settings.json
```

Add the following. If the file already contains other settings, add just the `"hooks"` entry (mind the JSON commas); if it already has `"hooks"`, merge these events into it.

```json
{
  "hooks": {
    "UserPromptSubmit": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "PreToolUse": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "PostToolUse": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "PermissionRequest": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "Stop": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "StopFailure": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ],
    "SessionEnd": [
      { "hooks": [ { "type": "http", "url": "http://127.0.0.1:52790/", "timeout": 2 } ] }
    ]
  }
}
```

`"timeout": 2` caps each request at 2 seconds, so even if ClauPet misbehaves and never replies, Claude is slowed down by at most 2 seconds.

The change takes effect as soon as you save; there is no need to restart Claude Code. Send Claude a message and the pet starts working.

**3. Start with Windows (optional)**

Press `Win + R`, type `shell:startup`, press Enter, and put a shortcut to `ClauPet.exe` in the folder that opens.

### Usage

| Action | Result |
|---|---|
| Hover over the pet | She gently floats up and down |
| Left-click the pet | Bring the Claude desktop app's window to the front (restoring it if minimized) |
| Left-drag the pet | Move it (she dangles as if lifted while dragged); the position is saved when you release the mouse |
| Right-click the pet or the tray icon | Menu: hide/show the pet, reload (use it if the pet looks blurry after moving to another monitor), or quit |
| Click the pet, then press 1–4 | Switch between the four states manually, for testing |

When the pet is hidden, the program keeps running in the background and keeps receiving events. Use the menu to quit.

### Privacy and security

- ClauPet only listens on `127.0.0.1` (this computer); other machines cannot connect to it
- It only reads `hook_event_name`, `session_id`, `tool_name` and `session_title` from each event. It **does not write them to disk or send** your conversation anywhere
- Note: the bubble shows **session titles** and **tool names** on screen, so others can see them while you share your screen, record, or stream
- The only data it saves locally is the pet's position, in the registry under `HKEY_CURRENT_USER\Software\ClauPet`

### Troubleshooting

**The pet never changes state?**
- Check that `settings.json` is valid and that all events sit directly under `"hooks"`
- Make sure only one ClauPet is running. If two are running, only the first one receives events
- Make sure port `52790` is not used by another program

**A DLL is reported missing on startup?**
Install the Visual C++ Redistributable mentioned under Requirements.

**The pet is invisible or blank?**
The pet's animations are WebP files, which need Qt's WebP plugin. If you use the release package, make sure `imageformats\qwebp.dll` sits next to `ClauPet.exe` (keep the whole folder when extracting). If you built it yourself, make sure the Qt Image Formats module is installed (see "Building from source" below).

**The pet stays on "working"?**
If you interrupt Claude with Esc, Claude Code may not send a `Stop` event, so the pet keeps showing "working" until the next response finishes. Sending Claude another message brings it back.

### Building from source

You need:
- Visual Studio 2022 or later with the "Desktop development with C++" workload
- Qt 6 (tested with 6.12.0) with the **MSVC 2022 64-bit** component and the **Qt Image Formats** module (under Qt 6.x → Additional Libraries in the Qt Maintenance Tool). The pet's animations are WebP files; without this module they cannot be played and the pet stays blank
  - After installing, check that `<Qt install dir>\6.12.0\msvc2022_64\plugins\imageformats\` contains `qwebp.dll`. The Maintenance Tool's summary page may show this module as 0 bytes; go by the actual file
- CMake 3.21 or later (included with Visual Studio)

Steps:
1. Clone this repository
2. Let CMake find Qt, for example by setting the `CMAKE_PREFIX_PATH` environment variable to `C:\Qt\6.12.0\msvc2022_64` (adjust to your installation), or by using the Qt VS Tools extension
3. Open the folder in Visual Studio ("Open Folder"), select the Release configuration, and build to get `ClauPet.exe`
4. Bundle the dependencies:

   ```
   windeployqt --release --no-translations ClauPet.exe
   ```

   Afterwards, check that `imageformats\qwebp.dll` is next to `ClauPet.exe`; the animations cannot play without it

> Tip: With a non-English Windows or Visual Studio and the Ninja generator, CMake may fail to recognize the compiler's `/showIncludes` output, so editing a header does not rebuild the `.cpp` files that include it. Adding `"VSLANG": "1033"` to the `environment` of your CMake preset (and installing Visual Studio's English language pack) makes the compiler print English output and fixes this.

### Roadmap

ClauPet is under active development. Planned next steps:

- **Faster alerts**: switch to "needs your input" and "error" right away, without waiting for the current animation to finish
- **More robust state**: if a session ends abnormally (no `SessionEnd` is received), clear its state after a while instead of showing it forever
- **Other ideas**: configurable port; a "start with Windows" toggle in the tray menu; testing and supporting the Claude Code CLI

Suggestions are welcome in [Issues](../../issues). See the [Changelog](CHANGELOG.md) for what changed in each version.

### License

| Content | License |
|---|---|
| Source code | [MIT License](LICENSE) |
| Character artwork and animations (`assets/pet/`) | [CC BY-NC-SA 4.0](assets/pet/LICENSE), **non-commercial** |
| Qt (DLLs bundled in the release) | [LGPL v3](https://www.qt.io/licensing/open-source-lgpl-obligations), used via dynamic linking |

The MIT License **applies to the source code only**, not to the character artwork and animations.

### Credits

- Original character design: *《Claude 拟人立绘·橘白礼服ver》* by **ZipZipPipe** ([source](https://xn--pssy23gqgbz2d718b.com/works/claude202610030001.html)), licensed under CC BY-NC-SA 4.0. The chibi desktop-pet images in this project were created by whywhy1352 with AI image generation tools, using the original design as a reference, and the animations were generated from these images with AI video tools. Both are published with the author's permission under the same CC BY-NC-SA 4.0 license (non-commercial). Many thanks to ZipZipPipe!
- Built with [Qt](https://www.qt.io/)

### Disclaimer

"Claude" is a trademark of Anthropic, PBC. ClauPet is an independent community project. It is not affiliated with, endorsed by, or sponsored by Anthropic.
