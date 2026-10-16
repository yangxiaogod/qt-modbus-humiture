#ifndef MODBUS_BASE_H
#define MODBUS_BASE_H
// 通用 Modbus RTU 通信基类：串口连接 / 打开 / 关闭
#include <QObject>
#include <QTimer>
#include <QVariant>
#include <QSerialPort>
#include <QModbusClient>
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QDebug>

namespace Config {
// 串口参数（集中配置，便于修改协议）
constexpr qint32        BAUD_RATE  = 9600;
constexpr QSerialPort::DataBits DATA_BITS = QSerialPort::Data8;
constexpr QSerialPort::Parity   PARITY    = QSerialPort::NoParity;
constexpr QSerialPort::StopBits STOP_BITS = QSerialPort::OneStop;
constexpr int           TIMEOUT_MS = 1000; // 单次读超时（毫秒）
constexpr int           RETRIES    = 3;    // 读失败自动重试次数
}

class modbus_base : public QObject
{
    Q_OBJECT
public:
    explicit modbus_base(QObject *parent = nullptr);
public slots:
    void openport(QString portname);
    void closeport();

signals:
    void StateChanged(bool m_connected,bool m_polling);
protected: // 供子类直接继承使用，避免重复声明遮蔽父类成员
    QModbusClient* client = nullptr;
    QTimer* timer = nullptr; // 轮询定时器
    bool m_connected = false; // 串口是否已打开
    bool m_polling = false; // 是否正在轮询
};

#endif // MODBUS_BASE_H
