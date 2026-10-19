// SPDX-License-Identifier: MIT
#include <QApplication>	// QApplication：整个 Qt 程序的"总管"，负责事件循环
#include "Petwindow.h"	// 我们自己的宠物窗口类


// 程序入口
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);	// 创建 Qt 程序对象，整个程序只能有一个
    app.setQuitOnLastWindowClosed(false);	// 宠物隐藏后程序不退出，继续留在系统托盘里；只有点"退出"菜单才结束
    Petwindow pet{};	// 创建宠物窗口；它在 main 的作用域里，要等 exec() 返回后才析构
    pet.show();			// 登记"要显示这个窗口"，真正的绘制要等事件循环开始后才发生

    return app.exec();	// 进入事件循环：不断等待并处理事件，直到调用 QApplication::quit()（点"退出"菜单）才返回
}