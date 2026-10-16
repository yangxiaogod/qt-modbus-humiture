#include "mainwindow.h"
#include <QApplication>

// =====================================================================
// 程序入口
// 下位机仿真器是一个 Qt Widgets 应用（带窗口），不再是控制台程序。
// 串口通过界面自由选择，不再依赖命令行参数。
// =====================================================================
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SlaveWindow w;
    w.show();           // 弹出“从站仿真器”窗口
    return a.exec();    // 进入事件循环，等待用户点击启动/停止
}
