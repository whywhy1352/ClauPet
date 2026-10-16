#include <QApplication>
#include <QLabel>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QLabel label("你好，桌宠");
    label.resize(300, 300);
    label.show();

    return app.exec();
}