#ifndef SLAVEWINDOW_H
#define SLAVEWINDOW_H

#include <QMainWindow>
#include <QModbusRtuSerialServer> // Qt6 中 QModbusRtuSerialSlave 已弃用，改用 Server
#include <QTimer>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

// =====================================================================
// SlaveWindow —— Modbus RTU 从站（下位机）仿真界面
// ---------------------------------------------------------------------
// 作用：在本机某个串口上扮演一台“温湿度变送器”。
//      你的上位机 modbus5（主站/Client）一连上来一发 0x03 读请求，
//      本程序立刻把保持寄存器里的温湿度回给它。
//
// 为什么做成窗口而不是控制台？
//      之前串口写死在 COM12，只能靠命令行参数改，不直观。
//      做成窗口后，你可以【自由选择串口 + 自由选择波特率】，点一下启动。
//
// 协议约定（必须和 modbus5 主站一致，否则读不到数）：
//      从站地址  : 0x01
//      功能码    : 0x03（读保持寄存器）
//      保持寄存器:
//          addr 0x00 = 温度原值（= 物理量℃ × 10，例如 250 -> 25.0℃）
//          addr 0x01 = 湿度原值（= 物理量%RH × 10，例如 600 -> 60.0%RH）
// =====================================================================
class SlaveWindow : public QMainWindow
{
    Q_OBJECT
public:
    // 构造函数：搭界面、枚举串口、创建刷新定时器
    explicit SlaveWindow(QWidget *parent = nullptr);
    // 析构：关掉仿真、释放 server，避免串口被残留进程占用
    ~SlaveWindow();

private slots:
    // “启动/停止仿真”按钮：第一次点=启动，运行中再点=停止
    void onStartStopClicked();
    // “刷新”按钮：重新枚举本机可用串口，填进下拉框
    void refreshPorts();
    // 定时器超时：每秒刷新一次温湿度并写进寄存器（模拟真实设备波动）
    void onTimeout();
    // 连接状态变化回调：把“已连接/断开/连接中”反映到状态标签
    void onStateChanged(QModbusDevice::State state);

private:
    // 纯代码布局（不依赖 .ui 文件，所有控件都在这里 new 出来并排好）
    void setupUi();
    // 建立寄存器映射：QModbusServer 必须先 setMap 才能 setData，否则写入无效
    void buildRegisterMap();

    QModbusRtuSerialServer *server = nullptr; // Modbus RTU 从站对象（运行时创建/删除）
    QTimer *timer = nullptr;                  // 刷新温湿度的定时器

    QComboBox *comboPort = nullptr; // 串口下拉框（列出本机所有可用 COM）
    QComboBox *comboBaud = nullptr; // 波特率下拉框（9600/19200/38400/115200）
    QPushButton *btnStart = nullptr;// 启动/停止按钮
    QLabel *labelStatus = nullptr;  // 状态文本（运行中/已停止/错误）
    QLabel *labelTemp = nullptr;    // 当前温度显示
    QLabel *labelHum = nullptr;     // 当前湿度显示

    bool running = false;           // 仿真是否正在运行（驱动按钮文字与可用性）
};

#endif // SLAVEWINDOW_H
