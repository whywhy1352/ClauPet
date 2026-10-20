// SPDX-License-Identifier: MIT
#include <QApplication>	// QApplication：整个 Qt 程序的"总管"，负责事件循环
#include<QMessageBox>	// 弹窗：端口被占用、无法启动时提示用户
#include "Petwindow.h"	// 我们自己的宠物窗口类


// 程序入口
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);	// 创建 Qt 程序对象，整个程序只能有一个
    app.setQuitOnLastWindowClosed(false);	// 宠物隐藏后程序不退出，继续留在系统托盘里；只有点"退出"菜单才结束
    Petwindow pet{};	// 创建宠物窗口；它在 main 的作用域里，要等 exec() 返回后才析构
    // 构造函数里已经尝试监听端口。监听失败说明端口被占用：可能已经有一个 ClauPet 在运行，
    // 也可能是别的程序占用了这个端口。这时这只宠物收不到任何事件，没有存在的意义，所以提示后退出
    if (pet.isListening())
    {
        pet.show();			// 登记"要显示这个窗口"，真正的绘制要等事件循环开始后才发生
    }
    else
    {
        // nullptr：还没有任何窗口显示出来，弹窗没有父窗口
        // 注意：这里的 52790 是写死的，修改 Petwindow.cpp 里的 kPort 时要同步修改
        QMessageBox::warning(nullptr, "Error", "Cannot start: port 52790 is already in use. ClauPet may already be running (check the tray icon), or another program is using this port.");
        // 直接从 main 返回，不进入事件循环。在构造函数里调用 quit() 是没用的，因为那时事件循环还没开始
        // 返回值是"退出码"：0 表示正常结束，非 0 表示出了问题
        // pet 离开作用域时会自动析构，托盘图标等资源也随之清理
        return 1;
    }

    return app.exec();	// 进入事件循环：不断等待并处理事件，直到调用 QApplication::quit()（点"退出"菜单）才返回
}