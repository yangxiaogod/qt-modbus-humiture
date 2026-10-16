#include "modbushumtemp.h"
#include <QDateTime>
#include <QTimer>
#include <QObject>
#include <QModbusRtuSerialClient>
#include <QModbusDevice>
modbushumtemp::modbushumtemp(QObject *parent)
    : modbus_base{parent}
{}

void modbushumtemp::timerread(qint32 time)
{
    if (!timer) {
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &modbushumtemp::readone);
    }
    timer->start(static_cast<int>(time));
    m_polling = true;
    emit StateChanged(m_connected, m_polling);
}

void modbushumtemp::stoppoll()
{
    if (timer)
        timer->stop();
    m_polling = false;
    emit StateChanged(m_connected, m_polling);
}

void modbushumtemp::readone()
{
    if (!client || client->state() != QModbusDevice::ConnectedState) {
        qDebug() << "串口未打开，无法读取";
        return;
    }
    QModbusDataUnit data(QModbusDataUnit::HoldingRegisters, REG_TEMP, REG_COUNT);
    QModbusReply *reply = client->sendReadRequest(data, SLAVE_ADDRESS);
    if (!reply) {
        qDebug() << "发送请求失败";
        return;
    }

    // 处理一帧回复：通信错误仅打印、不发射数据；
    // 成功则把两寄存器原值 ÷10 还原为物理量，emit newReading 回主线程（Queued 连接，线程安全）
    auto onFinished = [this, reply]() {
        if (reply->error() != QModbusDevice::NoError) {
            qDebug() << "读取失败:" << reply->errorString();
            reply->deleteLater();       // 释放 reply，避免泄漏
            return;
        }
        // result() 返回读回的寄存器单元，value(i) 取第 i 个
        QModbusDataUnit r = reply->result();
        float wd = r.value(0) / TEMP_SCALE; // 温度寄存器原值 = 温度×10 → ÷10
        float sd = r.value(1) / TEMP_SCALE; // 湿度寄存器原值 = 湿度×10 → ÷10
        QDateTime now = QDateTime::currentDateTime();
        qDebug() << "[" << now.toString("yyyy-MM-dd hh:mm:ss.zzz")
                 << "] 温度:" << wd << "℃  湿度:" << sd << "%RH";
        // 抛回主线程，由 MainWindow::onNewReading 更新 UI 并落库
        emit newReading(wd, sd, now.toMSecsSinceEpoch());
        reply->deleteLater();           // 释放 reply，避免泄漏
    };

    // 若请求已被同步拒绝（reply 已 finished），需直接处理，否则 finished 不再触发、reply 泄漏
    if (!reply->isFinished())
        connect(reply, &QModbusReply::finished, this, onFinished);
    else
        onFinished();
}
