#include<QPainter>
#include"Petwindow.h"
#include<QMouseEvent>
#include<QKeyEvent>
constexpr int kPetSize{ 180 };
static QPixmap loadPetImage(const char* path,qreal dpr)
{
	const int realSize{ static_cast<int>(kPetSize * dpr) };
	QPixmap img{ QPixmap( path ).scaled(realSize, realSize, Qt::KeepAspectRatio, Qt::SmoothTransformation) };
	img.setDevicePixelRatio(dpr);
	return img;
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