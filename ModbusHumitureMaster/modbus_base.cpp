#include "modbus_base.h"
#include <QThread>

modbus_base::modbus_base(QObject *parent)
    : QObject{parent}
{}

void modbus_base::openport(QString portname)
{
    if (!client) {
        client = new QModbusRtuSerialClient(this); // this 为父对象，随 worker 析构
        // 设备级错误（断线 / IO 失败等）打印，便于现场定位
        connect(client, &QModbusDevice::errorOccurred, this, [this](QModbusDevice::Error) {
            qDebug() << "设备错误:" << client->errorString();
        });
        // 状态变化（打开 / 关闭 / 掉线）回主线程，用于刷新按钮状态
        connect(client, &QModbusDevice::stateChanged, this, [this](QModbusDevice::State s) {
            m_connected = (s == QModbusDevice::ConnectedState); // 同步状态位
            emit StateChanged(m_connected, m_polling);
        });
    }
    // 避免重复点击「打开」反复 connectDevice
    if (client->state() == QModbusDevice::ConnectedState) {
        qDebug() << "串口已经处于打开状态";
        return;
    }

    // 配置串口参数（取值来自 Config 命名空间）
    client->setConnectionParameter(QModbusDevice::SerialPortNameParameter, portname);
    client->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, Config::BAUD_RATE);
    client->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, Config::DATA_BITS);
    client->setConnectionParameter(QModbusDevice::SerialParityParameter,   Config::PARITY);
    client->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, Config::STOP_BITS);
    client->setTimeout(Config::TIMEOUT_MS);
    client->setNumberOfRetries(Config::RETRIES);

    // connectDevice 异步：返回 true 仅表示开始打开，真正连上以 stateChanged 为准
    if (client->connectDevice())
        qDebug() << "串口打开成功！";
    else
        qDebug() << "串口打开失败！";
}

void modbus_base::closeport()
{
    if (client) {
        connect(client, &QModbusDevice::stateChanged, this, [this](QModbusDevice::State s) {
            m_connected = (s == QModbusDevice::ConnectedState); // 同步状态位，供轮询状态回传用
            emit StateChanged(m_connected, m_polling);
        });

        client->disconnectDevice();
        client->disconnect();           // 断开回调，避免 deleteLater 后访问已释放对象
        client->deleteLater();          // 删除 client，下次 openport 重建并清除错误态
        client = nullptr;
        qDebug()<<"关闭串口成功！";

    }

}
