#include <QApplication>
#include "Petwindow.h"


int main(int argc, char* argv[]) 
{
    QApplication app(argc, argv);
    
    Petwindow pet{};
    pet.show();
    
    return app.exec();
}