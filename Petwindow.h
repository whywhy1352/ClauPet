#pragma once
#include<QWidget>
class Petwindow : public QWidget
{
public:
	Petwindow();
protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
private:
	QPoint m_dragOffset;
};