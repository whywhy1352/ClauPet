#pragma once	// 防止这个头文件在同一个 .cpp 里被重复包含
#include<QWidget>		// 基类 QWidget：Qt 的"窗口/控件"类
#include"Petstate.h"	// 宠物状态枚举 PetState
#include<QPixmap>		// 成员 m_idle 等是 QPixmap 对象，需要它的完整定义
#include<QTcpServer>	// 成员 m_server 是 QTcpServer 对象，需要它的完整定义
#include<QSystemTrayIcon>	// 成员 m_tray 是 QSystemTrayIcon 对象，需要它的完整定义
#include<QMenu>			// 成员 m_menu 是 QMenu 对象，需要它的完整定义

// 宠物窗口：继承 QWidget，负责显示宠物、响应鼠标键盘、接收 Claude Code 发来的事件
class Petwindow : public QWidget
{
public:
	Petwindow();					// 构造函数：设置窗口外观、加载图片、开始监听端口、创建菜单和托盘图标
	void setState(PetState state);	// 切换宠物状态，并通知 Qt 重画窗口（setter）
	PetState state()const;			// 查询当前状态；常函数，只读不改（getter）
protected:
	// 下面 4 个都是重写 QWidget 的虚函数，由 Qt 在对应事件发生时自动调用
	void paintEvent(QPaintEvent* event) override;		// 窗口需要重画时：按当前状态画出对应的图片
	void mousePressEvent(QMouseEvent* event) override;	// 鼠标按下时：左键记录拖动起点，右键弹出菜单
	void mouseMoveEvent(QMouseEvent* event) override;	// 鼠标移动时：按住左键就拖动窗口
	void keyPressEvent(QKeyEvent* event) override;		// 键盘按下时：数字键 1~4 手动切换状态（测试用）
private:
	QPoint m_dragOffset;				// 左键按下时，鼠标在窗口里的位置；拖动时用它保持相对位置不变
	PetState m_state{ PetState::Idle };	// 当前状态，程序启动时是空闲
	QPixmap m_idle;						// 空闲状态的图片
	QPixmap m_working;					// 工作中状态的图片
	QPixmap m_waiting;					// 等待授权状态的图片
	QPixmap m_error;					// 出错状态的图片
	QTcpServer m_server;				// 本地服务器：在 127.0.0.1 的端口上等待 Claude Code 发来的 HTTP 请求
	// 注意顺序：成员按声明顺序构造、按相反顺序析构。
	// m_tray 记着 m_menu 的地址，所以 m_menu 必须先声明（后析构），保证托盘销毁之前菜单一直有效
	QMenu m_menu;						// 右键菜单：托盘图标和宠物本身共用
	QSystemTrayIcon m_tray;				// 任务栏右下角的托盘图标
	void handleHookEvent(const QByteArray& body);	// 解析请求体里的 JSON，按 hook_event_name 切换状态
};