#include "mainwindow.h"

#include <QModbusDataUnitMap>   // 寄存器映射表类型（setMap 用）
#include <QSerialPortInfo>      // 枚举本机可用串口
#include <QSerialPort>          // 波特率/数据位等枚举常量
#include <QDateTime>
#include <QVBoxLayout>          // 垂直布局
#include <QHBoxLayout>          // 水平布局
#include <QGroupBox>            // 分组框
#include <QFormLayout>          // 表单布局（标签+控件成对）
#include <QCoreApplication>
#include <cmath>                // std::sin
#include <cstdlib>             // std::rand

// ---------------------------------------------------------------------
// 协议常量（集中放在这里，改协议只动这一处，须与 modbus5 保持一致）
// ---------------------------------------------------------------------
namespace {
constexpr quint8  SLAVE_ADDR   = 0x01; // 从站地址
constexpr int     REG_TEMP     = 0;    // 温度寄存器地址
constexpr int     REG_HUM      = 1;    // 湿度寄存器地址
constexpr quint16 TEMP_INIT    = 250;  // 25.0 ℃
constexpr quint16 HUM_INIT     = 600;  // 60.0 %RH
constexpr int     REG_COUNT    = 10;   // 保持寄存器表长度（多建几个备用）
constexpr int     REFRESH_MS   = 1000; // 温湿度刷新周期
}

// =====================================================================
// 构造函数
// =====================================================================
SlaveWindow::SlaveWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi();        // 先把界面控件搭出来
    refreshPorts();   // 再枚举本机串口填进下拉框

    // 刷新定时器：父对象为本窗口，随窗口析构自动清理，不泄漏
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &SlaveWindow::onTimeout);
}

// =====================================================================
// 析构：停定时器、关串口、释放 server
// =====================================================================
SlaveWindow::~SlaveWindow()
{
    if (timer)
        timer->stop();
    if (server) {
        server->disconnectDevice(); // 关闭串口（释放被占用的 COM）
        delete server;              // 删除从站对象
        server = nullptr;
    }
}

// =====================================================================
// setupUi：用代码把所有控件排布好（不用 .ui 文件）
// =====================================================================
void SlaveWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Modbus RTU 从站仿真器 (下位机)"));
    resize(420, 320);

    // ---- 串口设置分组 ----
    auto *portGroup = new QGroupBox(QStringLiteral("串口设置"), this);
    auto *formLayout = new QFormLayout(portGroup);

    comboPort = new QComboBox(portGroup);
    comboBaud = new QComboBox(portGroup);
    // 波特率选项：默认 9600，与 modbus5 主站默认一致
    comboBaud->addItems({QStringLiteral("9600"),
                         QStringLiteral("19200"),
                         QStringLiteral("38400"),
                         QStringLiteral("115200")});
    comboBaud->setCurrentText(QStringLiteral("9600"));

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), portGroup);
    connect(refreshBtn, &QPushButton::clicked, this, &SlaveWindow::refreshPorts);

    // 第一行：串口 [下拉] [刷新按钮]
    auto *rowPort = new QHBoxLayout;
    rowPort->addWidget(comboPort, 1);
    rowPort->addWidget(refreshBtn);
    formLayout->addRow(QStringLiteral("串口："), rowPort);
    // 第二行：波特率 [下拉]
    formLayout->addRow(QStringLiteral("波特率："), comboBaud);

    // ---- 启动/停止按钮 ----
    btnStart = new QPushButton(QStringLiteral("启动仿真"), this);
    connect(btnStart, &QPushButton::clicked, this, &SlaveWindow::onStartStopClicked);

    // ---- 状态与数据 ----
    labelStatus = new QLabel(QStringLiteral("未启动"), this);
    labelTemp   = new QLabel(QStringLiteral("温度：-- ℃"), this);
    labelHum    = new QLabel(QStringLiteral("湿度：-- %RH"), this);
    labelTemp->setStyleSheet(QStringLiteral("font-size:18px; color:#e67e22;"));
    labelHum->setStyleSheet(QStringLiteral("font-size:18px; color:#27ae60;"));

    // ---- 主垂直布局 ----
    auto *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(portGroup);
    mainLayout->addWidget(btnStart);
    mainLayout->addWidget(labelStatus);
    mainLayout->addWidget(labelTemp);
    mainLayout->addWidget(labelHum);
    mainLayout->addStretch(1); // 把控件顶上去，底部留白

    auto *central = new QWidget(this);
    central->setLayout(mainLayout);
    setCentralWidget(central);
}

// =====================================================================
// refreshPorts：枚举本机可用串口，填进 comboPort 下拉框
// =====================================================================
void SlaveWindow::refreshPorts()
{
    comboPort->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        comboPort->addItem(info.portName()); // 例如 "COM12"、"COM13"
    }
    if (comboPort->count() == 0) {
        // 没插任何串口（也没装 com0com）时给个占位项，避免下拉框空白
        comboPort->addItem(QStringLiteral("（无可用串口）"));
    }
}

// =====================================================================
// buildRegisterMap：建立寄存器映射，必须在 setData 之前调用
// =====================================================================
void SlaveWindow::buildRegisterMap()
{
    // QModbusServer 没有 map 时 setData 一律返回 false（之前“初始化失败”就源于此）
    QModbusDataUnitMap reg;
    reg[QModbusDataUnit::HoldingRegisters] =
        QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 0, REG_COUNT);
    if (!server->setMap(reg)) {
        labelStatus->setText(QStringLiteral("错误：建立寄存器映射失败"));
    }
}

// =====================================================================
// onStartStopClicked：启动 / 停止 仿真（同一个按钮切换）
// =====================================================================
void SlaveWindow::onStartStopClicked()
{
    if (!running) {
        // ---------- 启动 ----------
        QString port = comboPort->currentText();
        if (port.startsWith(QStringLiteral("（"))) { // 占位项，没有真串口
            labelStatus->setText(QStringLiteral("错误：请先选择串口"));
            return;
        }
        const int baud = comboBaud->currentText().toInt();

        // 运行时创建 server（与 modbus5 主站风格一致：打开时再建，关闭后删除）
        server = new QModbusRtuSerialServer(this);
        server->setConnectionParameter(QModbusDevice::SerialPortNameParameter, port);
        server->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baud);
        server->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, QSerialPort::Data8);
        server->setConnectionParameter(QModbusDevice::SerialParityParameter,   QSerialPort::NoParity);
        server->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, QSerialPort::OneStop);
        server->setServerAddress(SLAVE_ADDR); // 从站地址 0x01，须与 modbus5 一致
        // 连接状态变化 -> 更新状态标签
        connect(server, &QModbusDevice::stateChanged,
                this, &SlaveWindow::onStateChanged);

        buildRegisterMap();
        // 写入温湿度初值（map 已建立，这里必然成功）
        server->setData(QModbusDataUnit::HoldingRegisters, quint16(REG_TEMP), TEMP_INIT);
        server->setData(QModbusDataUnit::HoldingRegisters, quint16(REG_HUM),  HUM_INIT);

        if (!server->connectDevice()) {
            labelStatus->setText(QStringLiteral("错误：无法打开串口 ") + port +
                                 QStringLiteral("：") + server->errorString());
            delete server;
            server = nullptr;
            return;
        }

        // 启动成功：进入运行态
        running = true;
        btnStart->setText(QStringLiteral("停止仿真"));
        comboPort->setEnabled(false); // 运行中锁住串口选择，防误改
        comboBaud->setEnabled(false);
        timer->start(REFRESH_MS);     // 开始刷温湿度
    } else {
        // ---------- 停止 ----------
        timer->stop();                // 先停刷新
        if (server) {
            server->disconnectDevice();// 关闭串口（释放 COM，主站才能再连）
            server->deleteLater();     // 延迟删除（等事件循环处理完在途请求）
            server = nullptr;
        }
        running = false;
        btnStart->setText(QStringLiteral("启动仿真"));
        comboPort->setEnabled(true);  // 解锁串口选择
        comboBaud->setEnabled(true);
        labelStatus->setText(QStringLiteral("已停止"));
        labelTemp->setText(QStringLiteral("温度：-- ℃"));
        labelHum->setText(QStringLiteral("湿度：-- %RH"));
    }
}

// =====================================================================
// onTimeout：每秒刷新温湿度（模拟真实变送器的波动）
// =====================================================================
void SlaveWindow::onTimeout()
{
    static int t = 0;
    t++;
    // 温度：25.0℃ 附近按正弦缓慢波动（±3℃）
    qint16 temp = qint16(TEMP_INIT + 30 * std::sin(t / 20.0));
    // 湿度：在 60.0%RH 附近随机游走（每次 ±1%）
    static qint16 hum = HUM_INIT;
    hum += qint16((std::rand() % 21) - 10);
    if (hum < 300) hum = 300; // 限幅，避免越界
    if (hum > 900) hum = 900;

    // 把最新值写进保持寄存器，主站下次来读就能拿到
    if (server) {
        server->setData(QModbusDataUnit::HoldingRegisters, quint16(REG_TEMP), quint16(temp));
        server->setData(QModbusDataUnit::HoldingRegisters, quint16(REG_HUM),  quint16(hum));
    }
    // 同时更新界面显示
    labelTemp->setText(QStringLiteral("温度：") +
                       QString::number(temp / 10.0, 'f', 1) + QStringLiteral(" ℃"));
    labelHum->setText(QStringLiteral("湿度：") +
                      QString::number(hum / 10.0, 'f', 1) + QStringLiteral(" %RH"));
}

// =====================================================================
// onStateChanged：连接状态变化时更新状态标签
// =====================================================================
void SlaveWindow::onStateChanged(QModbusDevice::State state)
{
    switch (state) {
    case QModbusDevice::ConnectedState:
        labelStatus->setText(QStringLiteral("运行中：已连接 ") + comboPort->currentText());
        break;
    case QModbusDevice::UnconnectedState:
        labelStatus->setText(QStringLiteral("已断开"));
        break;
    default:
        labelStatus->setText(QStringLiteral("连接中..."));
        break;
    }
}
