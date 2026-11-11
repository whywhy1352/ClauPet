// SPDX-License-Identifier: MIT
#pragma once	// 防止这个头文件在同一个 .cpp 里被重复包含
#include<QWidget>		// 基类 QWidget：Qt 的"窗口/控件"类
#include"Petstate.h"	// 宠物状态枚举 PetState
#include<QPixmap>		// paintEvent 里要用 QPixmap 的完整定义；只 include <QMovie> 拿不到它
#include<QTcpServer>	// 成员 m_server 是 QTcpServer 对象，需要它的完整定义
#include<QSystemTrayIcon>	// 成员 m_tray 是 QSystemTrayIcon 对象，需要它的完整定义
#include<QMenu>			// 成员 m_menu 是 QMenu 对象，需要它的完整定义
#include<map>			// 成员 m_sessionStates 是 std::map
#include<QTimer>			// 成员 m_animTimer 是 QTimer 对象，需要它的完整定义
#include<QElapsedTimer>	// 成员 m_animClock 是 QElapsedTimer 对象，需要它的完整定义
#include<QEnterEvent>	// enterEvent 的参数类型
#include<QMovie>		// 成员 m_movie 是 QMovie 对象，需要它的完整定义

// 宠物窗口：继承 QWidget，负责显示宠物、响应鼠标键盘、接收 Claude Code 发来的事件
class Petwindow : public QWidget
{
public:
	Petwindow();					// 构造函数：设置窗口外观、恢复上次的位置、开始播放动画、开始监听端口、创建菜单和托盘图标
	void setState(PetState state);	// 切换宠物状态，换成对应的动画，并通知 Qt 重画窗口（setter）
	PetState state()const;			// 查询当前状态；常函数，只读不改（getter）
	bool isListening()const;		// 是否成功监听了端口；失败说明端口被占用（可能已经有一个 ClauPet 在运行）
	// 监听的端口号；Claude Code 的 HTTP hook 要发到这个端口
	// 静态成员：属于"Petwindow 这个类"，而不属于某个对象，不用创建对象就能访问
	// 类里面直接写 kPort；类外面写 Petwindow::kPort（main.cpp 的提示文字就用到了）
	// 只在这一处定义：要改端口，只改这里（同时要修改 Claude Code 的 hooks 配置）
	static constexpr quint16 kPort{ 52790 };
protected:
	// 下面 9 个都是重写 QWidget 的虚函数，由 Qt 在对应事件发生时自动调用
	void paintEvent(QPaintEvent* event) override;		// 窗口需要重画时：画出动画的当前帧和气泡
	void mousePressEvent(QMouseEvent* event) override;	// 鼠标按下时：左键记录拖动起点，右键弹出菜单
	void mouseMoveEvent(QMouseEvent* event) override;	// 鼠标移动时：按住左键就拖动窗口
	void keyPressEvent(QKeyEvent* event) override;		// 键盘按下时：数字键 1~4 手动切换状态（测试用）
	void mouseReleaseEvent(QMouseEvent* event) override;	// 鼠标松开时：左键点击切换到 Claude 窗口，拖动结束则保存当前位置
	void hideEvent(QHideEvent* event) override;			// 窗口隐藏时：停止浮动动画和宠物动画
	void enterEvent(QEnterEvent* event) override;		// 鼠标移到宠物身上时：开始浮动
	void leaveEvent(QEvent* event) override;			// 鼠标从宠物身上移开时：停止浮动
	bool event(QEvent* event) override;					// 所有事件的总入口：这里只拦截"缩放比例变了"，按新比例重新设置动画的解码尺寸，其他事件交回 QWidget 处理
private:
	QPoint m_dragOffset;				// 左键按下时，鼠标在窗口里的位置；拖动时用它保持相对位置不变
	PetState m_state{ PetState::Idle };	// Claude 实际的状态（多个会话合并后的结果），程序启动时是空闲；hook 事件一来就会变
	// 屏幕上正在显示的是哪个状态的动画。它不跟着 m_state 立刻变：要等过渡开始（或者没有过渡素材、直接切换、结束拖动）时才更新，
	// 过渡就是让它追上 m_state 的过程（见 syncDisplayState）。循环播放器播放的永远是它对应的动画
	PetState m_displayState{ PetState::Idle };
	QMovie m_movie;						// 循环播放器：播放 m_displayState（拖动时是被拎起来）的循环动画，无限循环
	QMovie m_transitionMovie;			// 过渡播放器：播放两个状态之间的过渡片段，只播一次，播完发出 finished()
	bool m_isTransition{ false };		// 是否正在播过渡：为 true 时 paintEvent 画过渡播放器的帧，syncDisplayState 不做新的决定
	// 让 movie 换成 fileName 这个文件，并从第 0 帧开始播放（按 stop → setFileName → start 的顺序，避免播放中换文件卡住）
	void switchMovie(QMovie& movie, const QString& fileName);
	QTcpServer m_server;				// 本地服务器：在 127.0.0.1 的端口上等待 Claude Code 发来的 HTTP 请求
	// 注意顺序：成员按声明顺序构造、按相反顺序析构。
	// m_tray 记着 m_menu 的地址，所以 m_menu 必须先声明（后析构），保证托盘销毁之前菜单一直有效
	QMenu m_menu;						// 右键菜单：托盘图标和宠物本身共用
	QSystemTrayIcon m_tray;				// 任务栏右下角的托盘图标
	QPoint m_pressGlobalPos;			// 左键按下时鼠标的屏幕坐标；移动时比较距离，判断是否开始了拖动
	void handleHookEvent(const QByteArray& body);	// 解析请求体里的 JSON，更新对应会话的状态，再刷新宠物
	// 每个 Claude Code 会话各自的状态：会话 ID → 状态。同时开着多个会话时，靠它避免互相覆盖
	std::map<QString, PetState> m_sessionStates;
	PetState combinedState()const;	// 合并所有会话的状态，返回最紧急的那个；常函数，只读不改
	void scheduleErrorRecovery(const QString& sessionId);	// 某个会话出错后，安排一个定时器，过一会儿把它恢复为空闲
	void reloadMovieIfNeeded();		// 屏幕的缩放比例和当前解码尺寸所用的比例不一样了，就调用 applyScaledSize，按当前比例重新设置解码尺寸
	void activateClaudeWindow();	// 把 Claude 桌面应用的窗口切换到最前面（用 Windows API）
	QString getLoopFileName();		// 循环播放器该播哪个文件：拖动时是被拎起来的动画，否则是 m_displayState 的动画
	void syncDisplayState();		// 缓冲函数：让 m_displayState 追上 m_state，决定是开始过渡、直接切换，还是什么都不做
	// 当前的解码尺寸是按哪个缩放比例算出来的；reloadMovieIfNeeded 拿它和屏幕现在的比例比较，判断要不要重新设置
	// 只在 applyScaledSize 里紧跟着 setScaledSize 更新，所以它和解码尺寸永远一致
	qreal m_savedDpr{ 1 };
	void applyScaledSize();			// 按当前屏幕的缩放比例，给两个播放器设置解码尺寸，并记下用的比例（m_savedDpr）
	// 是否正在被拖动。这是你和宠物之间的互动状态，不是 Claude 的状态，所以不放进 PetState
	// 只通过 setDragging 修改：mouseMoveEvent 里设为 true，mouseReleaseEvent 里改回 false；为 true 时 getLoopFileName 返回被拎起来的动画
	bool m_dragging{ false };
	void setDragging(bool isTrue);	// 修改 m_dragging 的唯一入口：打断正在播的过渡，立刻换动画、请求重画（和 setState 同一个思路）
	// 浮动动画用的两个计时工具，名字很像，作用完全不同：
	QTimer m_animTimer;            // "闹钟"：悬停时每 33 毫秒响一次，每次请求重画（负责"什么时候画"）
	QElapsedTimer m_animClock;     // "秒表"：记录开始浮动后过了多久，paintEvent 按它算位置（负责"画在哪"）
	QString m_bubbleText;							// 气泡里要显示的文字（"会话标题：工具名"）；空字符串表示不显示气泡
	// 每个会话的标题：会话 ID → 标题。标题只在你发消息时才收得到，所以先记下来，调用工具时再拿出来用
	std::map<QString, QString> m_sessionTitles;
	// 构造函数拆出来的 5 个步骤，都是 private，都只在构造时调用一次
	void setUpWindow();				// 窗口大小、无边框、置顶、不在任务栏显示、背景透明
	void restorePosition();			// 恢复上次保存的位置
	void setUpAnimation();			// 准备浮动动画的"闹钟"和"秒表"，并开始播放宠物动画（必须在 restorePosition 之后）
	void setUpServer();				// 开始监听端口，登记处理请求的代码
	void setUpTrayMenu();			// 创建右键菜单和托盘图标
};