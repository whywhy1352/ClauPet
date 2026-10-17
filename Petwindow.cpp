#include<QPainter>
#include"Petwindow.h"
#include<QMouseEvent>
#include<QKeyEvent>
Petwindow::Petwindow()
{
	resize(200, 200);
	setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_TranslucentBackground);
}
void Petwindow::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	switch (m_state)
	{
	case PetState::Idle:
		painter.setBrush(QColor(217, 119, 87));
		break;
	case PetState::Working:
		painter.setBrush(QColor(70, 130, 230));
		break;
	case PetState::WaitingPermission:
		painter.setBrush(QColor(245, 185, 40));
		break;
	case PetState::Done:
		painter.setBrush(QColor(80, 190, 110));
		break;
	case PetState::Error:
		painter.setBrush(QColor(225, 75, 70));
		break;
	}
	painter.drawEllipse(QPoint(100, 100), 80, 80);
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
		setState(PetState::Done);
		break;
	case Qt::Key_5:
		setState(PetState::Error);
		break;
	}
}