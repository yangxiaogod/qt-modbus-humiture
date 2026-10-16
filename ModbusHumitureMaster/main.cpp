// 程序入口：创建应用与主窗口，进入 Qt 事件循环
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;          // 主窗口
    w.show();
    return QApplication::exec();
}
