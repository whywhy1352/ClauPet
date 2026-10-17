#include<QPainter>
#include"Petwindow.h"
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