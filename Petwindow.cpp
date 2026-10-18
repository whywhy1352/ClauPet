#include<QPainter>
#include"Petwindow.h"
#include<QMouseEvent>
#include<QKeyEvent>
#include<QTcpSocket>
#include<QDebug>
constexpr quint16 kPort{ 52790 };
constexpr int kPetSize{ 180 };
static QPixmap loadPetImage(const char* path,qreal dpr)
{
	const int realSize{ static_cast<int>(kPetSize * dpr) };
	QPixmap img{ QPixmap( path ).scaled(realSize, realSize, Qt::KeepAspectRatio, Qt::SmoothTransformation) };
	img.setDevicePixelRatio(dpr);
	return img;
}
static bool tryReadyBody(QTcpSocket* socket, QByteArray& body)
{
	const QByteArray data{ socket->peek(socket->bytesAvailable()) };
	const qsizetype headerEnd{ data.indexOf("\r\n\r\n") };
	int contentLength{ 0 };
	if (headerEnd == -1)
	{
		return false;
	}
	const QByteArray lower{ data.toLower() };
	const qsizetype lineStart{ lower.indexOf("content-length:") };
	if (lineStart != -1 && lineStart < headerEnd)
	{
		const qsizetype lineEnd{ lower.indexOf("\r\n", lineStart) };
		const qsizetype valueStart{ lineStart + 15 };
		contentLength = lower.mid(valueStart, lineEnd - valueStart).trimmed().toInt();
	}
	const qsizetype bodyStart{ headerEnd + 4 };
	const qsizetype lengthIfEnough{ contentLength };
	if (data.size() < bodyStart + lengthIfEnough)
	{
		return false;
	}
	else
	{
		socket->readAll();
		body = data.mid(bodyStart, contentLength);
		return true;
	}
}
Petwindow::Petwindow()
{
	qreal dpr{ devicePixelRatioF() };
	resize(kPetSize, kPetSize);
	m_idle = loadPetImage(":/assets/pet/idle.png", dpr);
	m_working = loadPetImage(":/assets/pet/working.png", dpr);
	m_waiting = loadPetImage(":/assets/pet/waiting.png", dpr);
	m_error = loadPetImage(":/assets/pet/error.png", dpr);
	setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_TranslucentBackground);
	if (!m_server.listen(QHostAddress::LocalHost, kPort))
	{
		qDebug() << "listen failed" << m_server.errorString();
	}
	connect(&m_server, &QTcpServer::newConnection, this, [this]() 
		{
			QTcpSocket* socket{ m_server.nextPendingConnection() };
			connect(socket, &QTcpSocket::readyRead, this, [socket]()
				{
					QByteArray body;
					if (!tryReadyBody(socket, body))
					{
						return;
					}
					qDebug().noquote() << body;
					socket->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
					socket->disconnectFromHost();
				});
			connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
		});
}
void Petwindow::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	const QPixmap* pixmap{ &m_idle };
	switch (m_state)
	{
	case PetState::Idle:
		pixmap = &m_idle;
		break;
	case PetState::Working:
		pixmap = &m_working;
		break;
	case PetState::WaitingPermission:
		pixmap = &m_waiting;
		break;
	case PetState::Error:
		pixmap = &m_error;
		break;
	}
	painter.drawPixmap(0, 0, *pixmap);
}
void Petwindow::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		m_dragOffset = event->position().toPoint();
	}
	if (event->button() == Qt::RightButton)
	{
		close();
	}
}
void Petwindow::mouseMoveEvent(QMouseEvent* event)
{
	if (event->buttons() & Qt::LeftButton)
	{
		QPoint temp{ event->globalPosition().toPoint() - m_dragOffset };
		move(temp);
	}
}
void Petwindow::setState(PetState state)
{
	m_state = state;
	update();
}
PetState Petwindow::state() const
{
	return m_state;
}
void Petwindow::keyPressEvent(QKeyEvent* event)
{
	switch (event->key())
	{
	case Qt::Key_1:
		setState(PetState::Idle);
		break;
	case Qt::Key_2:
		setState(PetState::Working);
		break;
	case Qt::Key_3:
		setState(PetState::WaitingPermission);
		break;
	case Qt::Key_4:
		setState(PetState::Error);
		break;
	}
}