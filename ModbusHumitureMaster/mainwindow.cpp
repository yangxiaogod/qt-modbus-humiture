#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>
#include <QDateTime>
#include <QDebug>

#include <QtCharts>
#include <QPainter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);   // 加载 UI 与各控件默认状态

    ui->time_box->setRange(100, 2000);
    ui->time_box->setSingleStep(100);
    ui->time_box->setValue(1000);

    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        ui->cbSerialPort->addItem(info.portName());
    }

    QStringList headers;
    headers << "时间" << "温度(℃)" << "湿度(%RH)";
    ui->history_table->setColumnCount(3);
    ui->history_table->setHorizontalHeaderLabels(headers);
    ui->history_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->history_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->history_table->verticalHeader()->setVisible(false);

    m_chart = new QChart();
    m_chart->setTitle("温湿度实时曲线");
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    m_tempSeries = new QLineSeries();
    m_tempSeries->setName("温度(℃)");
    m_tempSeries->setColor(Qt::red);
    m_humSeries = new QLineSeries();
    m_humSeries->setName("湿度(%RH)");
    m_humSeries->setColor(Qt::blue);
    m_chart->addSeries(m_tempSeries);
    m_chart->addSeries(m_humSeries);

    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("hh:mm:ss");
    m_axisX->setTitleText("时间");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis();
    m_axisY->setRange(-20, 100);
    m_axisY->setTitleText("数值");
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_tempSeries->attachAxis(m_axisX);
    m_tempSeries->attachAxis(m_axisY);
    m_humSeries->attachAxis(m_axisX);
    m_humSeries->attachAxis(m_axisY);

    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setParent(ui->centralwidget);
    m_chartView->setGeometry(500, 110, 489, 470);

    updateButtonStates(false, false);
    // 创建 Modbus 工作对象并移入独立线程
    m = new modbushumtemp();
    thread_modbus = new QThread();
    m->moveToThread(thread_modbus);

    // Modbus 信号槽：UI 信号 → 工作对象槽；工作对象状态 → 刷新按钮
    connect(this, &MainWindow::openportsignal,    m, &modbus_base::openport);
    connect(this, &MainWindow::closeportsignal,   m, &modbus_base::closeport);
    connect(this, &MainWindow::readonesignal,     m, &modbushumtemp::readone);
    connect(this, &MainWindow::timerreadsignal,   m, &modbushumtemp::timerread);
    connect(this, &MainWindow::stoppollsignal,    m, &modbushumtemp::stoppoll);
    connect(m,    &modbushumtemp::StateChanged,   this, &MainWindow::updateButtonStates);

    // 创建数据库工作对象并移入独立线程
    dbWorker = new db_manager();
    thread_sql = new QThread();
    dbWorker->moveToThread(thread_sql);

    // 数据库信号槽：UI 信号 → 数据库槽；数据库结果/错误 → 刷新 UI
    connect(this, &MainWindow::dbInitSignal,     dbWorker, &db_manager::initDatabase);
    connect(this, &MainWindow::queryRecentsignal,dbWorker, &db_manager::queryRecent);
    connect(this, &MainWindow::clearAllsignal,   dbWorker, &db_manager::clearAll);
    connect(dbWorker, &db_manager::dbReady,      this, &MainWindow::onDbReady);
    connect(dbWorker, &db_manager::queryResult,  this, &MainWindow::onQueryResult);
    connect(dbWorker, &db_manager::dbError,
            this, [this](const QString& msg){ statusBar()->showMessage("数据库错误：" + msg, 0); });

    // 新读数：一面更新 UI/曲线，一面落库（两条连接均 Queued，线程安全）
    connect(m,    &modbushumtemp::newReading,     this, &MainWindow::onNewReading);
    connect(m,    &modbushumtemp::newReading,     dbWorker, &db_manager::saveReading);

    thread_modbus->start();
    thread_sql->start();
    emit dbInitSignal();
}

MainWindow::~MainWindow()
{
    thread_modbus->quit();
    thread_modbus->wait();
    thread_sql->quit();
    thread_sql->wait();
    delete m;
    delete dbWorker;
    delete ui;
}

void MainWindow::on_openport_clicked()
{
    emit openportsignal(ui->cbSerialPort->currentText());   // 打开串口
}

void MainWindow::on_closeport_clicked()
{
    emit closeportsignal();                                 // 关闭串口
}

void MainWindow::on_readone_clicked()
{
    emit readonesignal();                                   // 读一次
}

void MainWindow::on_polling_read_clicked()
{
    emit timerreadsignal(static_cast<qint32>(ui->time_box->value()));   // 启动轮询
}

void MainWindow::on_stop_polling_read_clicked()
{
    emit stoppollsignal();                                  // 停止轮询
}

void MainWindow::on_query_history_clicked()
{
    statusBar()->showMessage("正在查询历史记录…", 0);
    emit queryRecentsignal(500);                            // 查询最近 500 条
}

void MainWindow::on_clear_history_clicked()
{
    emit clearAllsignal();
    ui->history_table->setRowCount(0);
    statusBar()->showMessage("已清空历史记录", 0);
}

// 根据连接 / 轮询状态刷新按钮可用性，并更新状态栏提示
void MainWindow::updateButtonStates(bool is_connect, bool is_polling)
{
    if (!is_connect) {
        ui->openport->setEnabled(true);
        ui->readone->setEnabled(false);
        ui->polling_read->setEnabled(false);
        ui->stop_polling_read->setEnabled(false);
        ui->closeport->setEnabled(false);
        statusBar()->showMessage("串口未打开", 0);
    } else if (is_connect && !is_polling) {
        ui->openport->setEnabled(false);
        ui->readone->setEnabled(true);
        ui->polling_read->setEnabled(true);
        ui->stop_polling_read->setEnabled(false);
        ui->closeport->setEnabled(true);
        statusBar()->showMessage("串口已打开（未轮询）", 0);
    } else {
        ui->openport->setEnabled(false);
        ui->readone->setEnabled(false);
        ui->polling_read->setEnabled(false);
        ui->stop_polling_read->setEnabled(true);
        ui->closeport->setEnabled(false);
        statusBar()->showMessage("串口已打开（轮询中）", 0);
    }
}

// 收到新读数：刷新数值显示并追加到实时曲线（超出上限则丢弃最旧点）
void MainWindow::onNewReading(float temp, float hum, qint64 tsMs)
{
    ui->tem->setText(QString::number(temp, 'f', 1));
    ui->moi->setText(QString::number(hum, 'f', 1));

    double x = static_cast<double>(tsMs);
    m_tempSeries->append(x, temp);
    m_humSeries->append(x, hum);
    if (m_tempSeries->count() > MAX_POINTS) {
        m_tempSeries->remove(0);
        m_humSeries->remove(0);
    }
    if (m_tempSeries->count() > 1) {
        qint64 t0 = static_cast<qint64>(m_tempSeries->at(0).x());
        qint64 t1 = static_cast<qint64>(m_tempSeries->at(m_tempSeries->count() - 1).x());
        m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(t0),
                          QDateTime::fromMSecsSinceEpoch(t1));
    }

    statusBar()->showMessage(
        "最新读数：" + QDateTime::fromMSecsSinceEpoch(tsMs).toString("hh:mm:ss.zzz")
        + QString("  温度 %1℃  湿度 %2%").arg(temp, 0, 'f', 1).arg(hum, 0, 'f', 1), 0);
}

// 数据库初始化结果：在状态栏给出提示
void MainWindow::onDbReady(bool ok)
{
    if (ok)
        statusBar()->showMessage("数据库已就绪，记录将自动保存", 0);
    else
        statusBar()->showMessage("数据库初始化失败！", 0);
}

// 历史查询结果：填入表格并显示条数
void MainWindow::onQueryResult(const QList<QVariantMap> &rows)
{
    ui->history_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QVariantMap &r = rows.at(i);
        ui->history_table->setItem(i, 0,
            new QTableWidgetItem(r.value("record_time").toString()));
        ui->history_table->setItem(i, 1,
            new QTableWidgetItem(QString::number(r.value("temperature").toDouble(), 'f', 1)));
        ui->history_table->setItem(i, 2,
            new QTableWidgetItem(QString::number(r.value("humidity").toDouble(), 'f', 1)));
    }
    ui->history_table->resizeColumnsToContents();
    statusBar()->showMessage(QString("共 %1 条历史记录").arg(rows.size()), 0);
}
