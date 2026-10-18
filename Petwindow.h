#pragma once
#include<QWidget>
#include"Petstate.h"
#include<QPixmap>
#include<QTcpServer>
class Petwindow : public QWidget
{
public:
	Petwindow();
	void setState(PetState state);
	PetState state()const;
protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
private:
	QPoint m_dragOffset;
	PetState m_state{ PetState::Idle };
	QPixmap m_idle;
	QPixmap m_working;
	QPixmap m_waiting;
	QPixmap m_error;
	QTcpServer m_server;
};