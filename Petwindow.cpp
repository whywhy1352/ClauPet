// SPDX-License-Identifier: MIT
#include<QPainter>		// QPainter：在窗口上画画的"画笔"
#include"Petwindow.h"	// 本文件实现的类
#include<QMouseEvent>	// 鼠标事件：要调用它的成员函数，需要完整定义
#include<QKeyEvent>		// 键盘事件：同上
#include<QTcpSocket>	// 一条 TCP 连接：用来收发 HTTP 数据
#include<QDebug>		// qDebug()：往控制台打印调试信息
#include<QJsonDocument>	// 解析 JSON 文本
#include<QJsonObject>	// JSON 里的 { ... } 对象
#include<QApplication>	// qApp 和 QApplication::quit：点"退出"时结束整个程序
#include<QIcon>			// 托盘图标用的图标类
#include<QSettings>		// 保存配置（宠物的位置）；在 Windows 上存在注册表里，程序关掉后数据还在
#include<algorithm>		// std::max_element：在多个会话里找出最紧急的状态
// Windows API：FindWindowW、SetForegroundWindow 等，用来把 Claude 窗口切换到前台
// NOMINMAX 必须写在 #include 之前：否则 windows.h 会定义名叫 min、max 的宏，弄坏 std::min / std::max
// 只在这个 .cpp 里包含，不放进头文件，免得包含 Petwindow.h 的文件都被这些宏影响
#define NOMINMAX
#include<windows.h>

constexpr quint16 kPort{ 52790 };	// 监听的端口号；Claude Code 的 HTTP hook 要发到这个端口
constexpr int kPetSize{ 180 };		// 宠物窗口和图片的大小，单位是逻辑像素（会乘上屏幕缩放比例）
// QSettings 的组织名和程序名，决定配置存在注册表的哪里：HKEY_CURRENT_USER\Software\ClauPet\ClauPet
// 读取和保存必须用同样的名字，所以定义成常量，只在这一处修改
constexpr const char* kSettingsName{ "ClauPet" };

// 加载一张宠物图片，并缩放到适合当前屏幕的大小
// path：资源路径，比如 ":/assets/pet/idle.png"
// dpr：屏幕缩放比例（device pixel ratio），175% 缩放时是 1.75
// static：这个函数只在本 .cpp 文件里可见
static QPixmap loadPetImage(const char* path,qreal dpr)
{
	const int realSize{ static_cast<int>(kPetSize * dpr) };	// 屏幕上实际需要的像素数，比如 180 × 1.75 = 315
	QPixmap img{ QPixmap( path ).scaled(realSize, realSize, Qt::KeepAspectRatio, Qt::SmoothTransformation) };	// 加载原图，保持比例、平滑地缩小到 realSize
	img.setDevicePixelRatio(dpr);	// 告诉 Qt 这张图已经按 dpr 倍准备好了，显示时不要再放大（否则会模糊）
	return img;
}

// 判断一个 HTTP 请求是否已经完整收到
// 收齐了：取走数据，把请求体存进 body（输出参数），返回 true
// 没收齐：什么都不取走，返回 false，等下一次 readyRead 再来判断
static bool tryReadyBody(QTcpSocket* socket, QByteArray& body)
{
	const QByteArray data{ socket->peek(socket->bytesAvailable()) };	// "偷看"目前收到的全部数据，数据仍留在 socket 里
	const qsizetype headerEnd{ data.indexOf("\r\n\r\n") };	// 请求头结束的位置（空行）；-1 表示请求头还没收完
	int contentLength{ 0 };	// 请求体的字节数；请求头里没写 content-length 时就当作 0
	if (headerEnd == -1)
	{
		return false;	// 请求头还没收完
	}
	const QByteArray lower{ data.toLower() };	// 转成小写：HTTP 请求头的名字不区分大小写
	const qsizetype lineStart{ lower.indexOf("content-length:") };	// "content-length:" 出现的位置
	if (lineStart != -1 && lineStart < headerEnd)	// 找到了，并且确实在请求头里（不是碰巧出现在请求体里）
	{
		const qsizetype lineEnd{ lower.indexOf("\r\n", lineStart) };	// 从这一行开头往后找，得到这一行的结尾
		const qsizetype valueStart{ lineStart + 15 };	// 值的起点："content-length:" 共 15 个字符
		contentLength = lower.mid(valueStart, lineEnd - valueStart).trimmed().toInt();	// 截出值、去掉空格、转成整数
	}
	const qsizetype bodyStart{ headerEnd + 4 };	// 请求体的起点：跳过 "\r\n\r\n" 这 4 个字符
	const qsizetype lengthIfEnough{ contentLength };	// 请求体应有的长度（就是 contentLength）
	if (data.size() < bodyStart + lengthIfEnough)
	{
		return false;	// 总长度还不够，请求体没收齐
	}
	else
	{
		socket->readAll();	// 收齐了：把数据真正从 socket 里取走，避免以后被重复处理
		body = data.mid(bodyStart, contentLength);	// 从偷看到的数据里截出请求体，通过引用交给调用者
		return true;
	}
}

// 状态的"紧急程度"：数字越大越紧急，多个会话合并时显示最紧急的那个
// 不直接比较枚举值：enum class 不能当数字比较，而且声明顺序也不代表优先级
static int urgency(PetState state)
{
	switch (state)
	{
	case PetState::Idle:
		return 0;
	case PetState::Working:
		return 1;
	case PetState::NeedsInput:
		return 2;
	case PetState::Error:
		return 3;	// 出错的优先级最高（有意的设计）
	}
	return 0;	// 走不到这里（4 种状态都已列出），只是为了让编译器知道每条路径都有返回值
}

// 根据 hook 事件名（以及工具名）换算出对应的状态
// eventName：hook_event_name；toolName：tool_name，只有工具相关的事件才有，其他事件是空字符串
// 认识这个事件：把状态存进 state（输出参数），返回 true；不认识：返回 false，state 不变
// 注意：SessionEnd 不在这里处理，它不是一种状态，而是"这个会话结束了"，由 handleHookEvent 直接删除
// 注意：规则从上到下依次判断，先匹配的先返回，所以越特殊的规则要写得越靠前
static bool stateHookEvent(const QString& eventName, const QString& toolName, PetState& state)
{
	// Claude 准备向你提问（AskUserQuestion 工具）：需要你介入
	// 实测提问时还会再触发一次 PermissionRequest，这条是保险：万一以后不再触发，照样能举手
	// 必须放在下面"PreToolUse → 工作中"之前，否则永远轮不到它
	if (eventName == "PreToolUse" && toolName == "AskUserQuestion")
	{
		state = PetState::NeedsInput;
		return true;
	}
	if (eventName == "UserPromptSubmit" || eventName == "PreToolUse" || eventName == "PostToolUse")
	{
		state = PetState::Working;
		return true;
	}
	if (eventName == "Stop")
	{
		state = PetState::Idle;
		return true;
	}
	if (eventName == "PermissionRequest" )
	{
		state = PetState::NeedsInput;
		return true;
	}
	if (eventName == "StopFailure")
	{
		state = PetState::Error;
		return true;
	}
	return false;
}

// 构造函数：程序启动时执行一次，只负责"设置"和"登记"，真正处理事件要等事件循环开始
Petwindow::Petwindow()
{
	qreal dpr{ devicePixelRatioF() };	// 当前屏幕的缩放比例（继承自 QWidget）
	resize(kPetSize, kPetSize);	// 窗口大小（逻辑像素）
	// ---- 恢复上次的位置 ----
	// 两个参数（组织名、程序名）决定数据存在注册表的哪里，见文件顶部的 kSettingsName
	// 必须和 mouseReleaseEvent 里保存时用的名字一样，才能读到同一份数据
	QSettings settings{ kSettingsName, kSettingsName };
	if (settings.contains("pos"))	// 第一次运行时还没存过，就跳过，用默认位置
	{
		const QPoint savedPos{ settings.value("pos").toPoint() };	// 上次保存的窗口左上角位置
		// 检查宠物的中心点是否落在某个屏幕上：比如上次拖到了副屏，这次副屏没接，
		// screenAt 就会返回 nullptr。这时不移动，否则宠物会出现在看不见的地方
		if (QGuiApplication::screenAt(savedPos + QPoint(kPetSize / 2, kPetSize / 2)) != nullptr)
		{
			move(savedPos);
		}
	}
	m_idle = loadPetImage(":/assets/pet/idle.png", dpr);		// 图片只在这里加载一次，paintEvent 里直接用
	m_working = loadPetImage(":/assets/pet/working.png", dpr);
	m_waiting = loadPetImage(":/assets/pet/waiting.png", dpr);
	m_error = loadPetImage(":/assets/pet/error.png", dpr);
	// 无边框 + 始终置顶 + 工具窗口（不在任务栏显示按钮，改用托盘图标）；用 | 组合三个标志位
	setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
	setAttribute(Qt::WA_TranslucentBackground);	// 背景透明：没画东西的地方完全透明，点击会穿透到后面的窗口
	if (!m_server.listen(QHostAddress::LocalHost, kPort))	// 开始在 127.0.0.1:kPort 上监听，只接受本机的连接
	{
		qDebug() << "listen failed" << m_server.errorString();	// 监听失败（比如端口被占用）时打印原因
	}
	// 登记：每当有新连接进来，就执行这个 lambda（[this]：捕获当前窗口，才能用 m_server）
	connect(&m_server, &QTcpServer::newConnection, this, [this]()
		{
			QTcpSocket* socket{ m_server.nextPendingConnection() };	// 取出这条新连接；是指针，因为 QObject 不能复制
			// 登记：这条连接每次收到数据，就执行这个 lambda
			// [socket]：复制的是指针，指向同一个 socket；[this]：捕获当前窗口，才能调用成员函数 handleHookEvent
			connect(socket, &QTcpSocket::readyRead, this, [socket,this]()
				{
					QByteArray body;	// 用来接收请求体（由 tryReadyBody 填进去）
					if (!tryReadyBody(socket, body))
					{
						return;	// 还没收齐：结束这一次执行，等下一次 readyRead
					}
					Petwindow::handleHookEvent(body);
					socket->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");	// 回复：200 成功、没有内容、回复完就关闭
					socket->disconnectFromHost();	// 断开这条连接
				});
			// 连接断开后，在安全的时机删除 socket 对象，释放内存
			connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
		});

	// ---- 右键菜单（托盘图标和宠物本身共用同一个菜单）----
	// 菜单项 1：隐藏 / 显示宠物。addAction 返回指向新菜单项的指针
	QAction* toggleAction{ m_menu.addAction("Hide pet") };
	// 点击时执行；[this]：要调用 isVisible/hide/show，[toggleAction]：要修改这一项的文字
	connect(toggleAction, &QAction::triggered, this, [this, toggleAction]()
		{
			if (isVisible())	// 宠物现在可见 → 隐藏它（隐藏后程序仍在运行，照样接收事件）
			{
				hide();
				toggleAction->setText("Show pet");
			}
			else				// 宠物现在隐藏 → 显示它
			{
				show();
				toggleAction->setText("Hide pet");
			}
		});
	// 菜单项 2：退出。qApp 指向 main 里唯一的 QApplication 对象，quit() 让 exec() 返回、程序结束
	QAction* quitAction{ m_menu.addAction("quit") };
	connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

	// ---- 托盘图标（任务栏右下角）----
	m_tray.setIcon(QIcon(m_idle));	// 用空闲状态的图片当图标
	m_tray.setToolTip("ClauPet");	// 鼠标悬停时显示的文字
	m_tray.setContextMenu(&m_menu);	// 右键托盘图标时弹出这个菜单（托盘会记住菜单的地址）
	m_tray.show();
}

// 重画窗口：Qt 在窗口需要重画时自动调用（第一次显示、调用 update() 之后等）
// 这里要尽量轻：只选一张已经加载好的图，画上去
void Petwindow::paintEvent(QPaintEvent*)	// 参数只写类型不写名字：表示这个参数用不到
{
	QPainter painter(this);	// 创建画笔，画在"我自己"这个窗口上；函数结束时析构，自动结束绘制
	painter.setRenderHint(QPainter::Antialiasing);	// 抗锯齿（以前画圆时用的，画图片用不到）
	const QPixmap* pixmap{ &m_idle };	// 指向这次要画的图片；常量指针：可以改指向，不能通过它改图片
	switch (m_state)	// 根据当前状态选择图片
	{
	case PetState::Idle:
		pixmap = &m_idle;
		break;
	case PetState::Working:
		pixmap = &m_working;
		break;
	case PetState::NeedsInput:
		pixmap = &m_waiting;
		break;
	case PetState::Error:
		pixmap = &m_error;
		break;
	}
	painter.drawPixmap(0, 0, *pixmap);	// 从窗口左上角 (0, 0) 开始画；*pixmap 是解引用，得到图片本身
}

// 鼠标按下：左键记录拖动起点，右键弹出菜单（和托盘图标的菜单是同一个）
void Petwindow::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)	// button()：这一次是哪个键触发的
	{
		m_dragOffset = event->position().toPoint();	// 记下鼠标在窗口里的位置（窗口坐标）
		m_pressGlobalPos = event->globalPosition().toPoint();	// 记下按下时的屏幕坐标，松开时用来判断是点击还是拖动
	}
	if (event->button() == Qt::RightButton)
	{
		m_menu.popup(event->globalPosition().toPoint());	// 在鼠标所在的屏幕位置弹出菜单，所以用屏幕坐标
	}
}

// 鼠标移动：按住左键时让窗口跟着鼠标走
void Petwindow::mouseMoveEvent(QMouseEvent* event)
{
	if (event->buttons() & Qt::LeftButton)	// buttons()：此刻按住了哪些键（位掩码），用 & 判断左键是否按着
	{
		QPoint temp{ event->globalPosition().toPoint() - m_dragOffset };	// 窗口左上角的新位置 = 鼠标的屏幕坐标 - 偏移量
		move(temp);	// 把窗口移过去（继承自 QWidget）
	}
}

// 切换状态：改状态的唯一入口，保证"改状态"和"重画"总是一起发生
void Petwindow::setState(PetState state)
{
	m_state = state;
	update();	// 通知 Qt 稍后重画窗口（会触发 paintEvent），不要直接调用 paintEvent
}

// 查询当前状态：常函数，可以通过 const Petwindow& 调用
PetState Petwindow::state() const
{
	return m_state;
}

// 键盘按下：数字键 1~4 手动切换 4 种状态，用来测试（要先点一下宠物让它获得焦点）
void Petwindow::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())	// key()：按下的是哪个键
	{
	case Qt::Key_1:
		setState(PetState::Idle);
		break;
	case Qt::Key_2:
		setState(PetState::Working);
		break;
	case Qt::Key_3:
		setState(PetState::NeedsInput);
		break;
	case Qt::Key_4:
		setState(PetState::Error);
		break;
	}
}

// 处理 Claude Code 发来的一个 hook 事件：
// 1. 更新发出这个事件的那个会话的状态（会话结束就从记录里删掉）
// 2. 把所有会话的状态合并，决定宠物显示什么
// 不认识的事件（或者 JSON 解析失败得到空字符串）不会改变任何会话的状态
void Petwindow::handleHookEvent(const QByteArray& body)
{
	const QJsonObject json{ QJsonDocument::fromJson(body).object() };		// 解析成 JSON 对象
	const QString eventName{ json.value("hook_event_name").toString() };	// 发生了什么事
	const QString sessionId{ json.value("session_id").toString() };		// 是哪个会话发来的
	const QString toolName{ json.value("tool_name").toString() };			// 调用的是哪个工具（没有就是空字符串）
	qDebug().noquote() << eventName;	// 调试用：在 VS 的"输出"窗口里能看到
	qDebug().noquote() << sessionId;
	qDebug().noquote() << toolName;
	PetState newState{ PetState::Idle };	// 接收 stateHookEvent 换算出的状态（输出参数）
	if (eventName == "SessionEnd")
	{
		m_sessionStates.erase(sessionId);	// 会话结束：从记录里删掉（删一个不存在的键也是安全的）
	}
	else if (stateHookEvent(eventName, toolName, newState))	// 认识这个事件才更新
	{
		m_sessionStates[sessionId] = newState;	// [] 找不到这个会话时会自动插入，正好用来记录新会话
	}
	setState(combinedState());	// 先算出合并后的状态，再通过 setState 统一修改并重画
}

// 把 Claude 桌面应用的窗口切换到最前面（左键单击宠物时调用）
// 已经在最前面时 SetForegroundWindow 什么都不会改变；Claude 没开时什么都不做
void Petwindow::activateClaudeWindow()
{
	// 按"窗口类名 + 标题"查找 Claude 的主窗口；找不到返回 nullptr
	// Claude 桌面应用是用 Electron 做的，窗口类名和 Chrome 一样是 Chrome_WidgetWin_1（已实测）
	// L"..." 是宽字符串（wchar_t，UTF-16），名字以 W 结尾的 Windows API 都要用它
	HWND hwnd{ FindWindowW(L"Chrome_WidgetWin_1", L"Claude") };
	if (hwnd == nullptr)
	{
		return;	// Claude 没开：什么都不做
	}
	else
	{
		if (IsIconic(hwnd) != 0)	// 窗口被最小化了（返回非零）
		{
			ShowWindow(hwnd, SW_RESTORE);	// 先还原
		}
		// 无论之前是否最小化，都要切换到前台（之前写在 if 里面，导致窗口只是被挡住时不会切换）
		// 刚点击了宠物，我们的程序处在前台，Windows 才允许我们把别的窗口切到前台
		SetForegroundWindow(hwnd);
	}
}

// 鼠标松开：判断这次左键操作是"点击"还是"拖动"
// 点击：切换到 Claude 窗口
// 拖动：拖动结束了，把当前位置保存下来，下次启动时恢复
// 选在这里保存，而不是在 mouseMoveEvent 里：拖一次只写一次，不会频繁写注册表；单纯点击也不会写
void Petwindow::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		// 按下到松开，鼠标移动的距离（横向 + 纵向，曼哈顿距离）小于系统规定的拖动阈值（默认 10 像素），就算点击
		// 用屏幕坐标计算：拖动时窗口本身在移动，窗口坐标的参照点会变
		if ((m_pressGlobalPos - event->globalPosition().toPoint()).manhattanLength() < QApplication::startDragDistance())
		{
			activateClaudeWindow();	// 点击
		}
		else
		{
			QSettings settings{ kSettingsName, kSettingsName };	// 和构造函数里读取时用同样的名字
			settings.setValue("pos", pos());	// pos()：窗口左上角在屏幕上的位置（继承自 QWidget）
		}
	}
}

// 合并所有会话的状态：返回其中最紧急的一个（按 urgency 比较）
// 常函数：只读不改。这里不能用 m_sessionStates[...] 查询，因为 [] 找不到时会插入新元素，属于修改
PetState Petwindow::combinedState()const
{
	if (m_sessionStates.empty())
	{
		return PetState::Idle;	// 没有任何会话：空闲。必须先判断，否则 max_element 会返回 end()，不能解引用
	}
	// max_element 在 map 上遍历，每个元素是"会话 ID → 状态"的键值对，所以比较的是 .second
	// 返回指向最紧急那一项的迭代器，->second 取出它的状态
	return std::max_element(m_sessionStates.begin(), m_sessionStates.end(), [](const auto& a, const auto& b)
		{
			return urgency(a.second) < urgency(b.second);
		})->second;
}