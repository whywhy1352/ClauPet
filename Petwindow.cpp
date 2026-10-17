#include<QPainter>
#include"Petwindow.h"
#include<QMouseEvent>
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
	painter.setBrush(QColor(255, 140, 60));
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