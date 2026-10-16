# Modbus RTU 从站（下位机）仿真器工程
# 现在是一个带窗口的 Qt Widgets 程序，串口/波特率由界面自由选择。
QT += widgets serialbus serialport

CONFIG += c++17
CONFIG -= app_bundle
# 去掉 console：这是 GUI 程序（之前是控制台程序才需要 console）

TARGET = slave_sim

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h
