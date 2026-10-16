#ifndef MODBUSHUMTEMP_H
#define MODBUSHUMTEMP_H

#include "modbus_base.h"
#include <QTimer>
#include <QSerialPort>
#include <QModbusClient>
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QDebug>
// Modbus 从站与保持寄存器映射（功能码 0x03）
// 约定：寄存器原值 = 物理量 × 10，上位机读取后 ÷10 还原
constexpr quint8  SLAVE_ADDRESS = 0x01;   // 从站地址，须与下位机一致
constexpr quint16 REG_TEMP      = 0x00;   // 温度
constexpr quint16 REG_HUM       = 0x01;   // 湿度
constexpr quint16 REG_COUNT     = 2;      // 一次读 2 个寄存器
constexpr float   TEMP_SCALE    = 10.0f;  // 原值 ÷ 此系数 = 物理量
class modbushumtemp : public modbus_base
{
    Q_OBJECT // 必须存在，否则 moc 不生成 signals/slots（链接报 undefined reference）
public:
    explicit modbushumtemp(QObject *parent = nullptr);
signals:
    void newReading(float temp, float hum, qint64 tsMs);
public slots:
    void readone();
    // 启动轮询：每隔 time 毫秒触发一次 readone()
    void timerread(qint32 time);
    // 停止轮询：停定时器并复位轮询状态（不关闭串口）
    void stoppoll();
    // 注意：不要在此重新声明 client/timer，否则会遮蔽父类同名成员，导致父子类各用各的对象
};

#endif // MODBUSHUMTEMP_H
