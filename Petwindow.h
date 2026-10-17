#pragma once
#include<QWidget>
#include"Petstate.h"
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
};