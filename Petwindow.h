#pragma once
#include<QWidget>
class Petwindow : public QWidget
{
public:
	Petwindow();
protected:
	void paintEvent(QPaintEvent* event) override;
};