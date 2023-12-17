#include "mainwindow.h"
#include <QApplication>
#include <QTextCodec>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("Freeware");
    QCoreApplication::setApplicationName("m3uMan");

    MainWindow w;
    w.show();

    return a.exec();
}
