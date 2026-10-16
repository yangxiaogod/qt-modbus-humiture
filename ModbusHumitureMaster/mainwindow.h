#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QList>
#include <QVariantMap>
#include "modbushumtemp.h"
#include "db_manager.h"

class QChart;
class QLineSeries;
class QChartView;
class QDateTimeAxis;
class QValueAxis;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// 主窗口：UI 交互、Modbus 与数据库两工作线程的桥接，实时曲线 + 历史表格
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_openport_clicked();
    void on_closeport_clicked();
    void on_readone_clicked();
    void on_polling_read_clicked();
    void on_stop_polling_read_clicked();
    void on_query_history_clicked();
    void on_clear_history_clicked();

    void updateButtonStates(bool is_connect, bool is_polling);
    void onNewReading(float temp, float hum, qint64 tsMs);
    void onDbReady(bool ok);
    void onQueryResult(const QList<QVariantMap> &rows);

signals:
    void openportsignal(QString portname);
    void closeportsignal();
    void readonesignal();
    void timerreadsignal(qint32 time);
    void stoppollsignal();

    void dbInitSignal();
    void queryRecentsignal(int limit);
    void clearAllsignal();

private:
    Ui::MainWindow *ui;
    QThread* thread_modbus;            // Modbus 工作线程
    QThread* thread_sql;              // 数据库工作线程
    modbushumtemp* m;                 // Modbus 工作对象（运行于 thread_modbus）
    db_manager* dbWorker;             // 数据库工作对象（运行于 thread_sql）

    QLineSeries* m_tempSeries = nullptr;  // 温度曲线序列
    QLineSeries* m_humSeries  = nullptr;  // 湿度曲线序列
    QChart*      m_chart      = nullptr;  // 实时曲线图表
    QChartView*  m_chartView  = nullptr;
    QDateTimeAxis* m_axisX    = nullptr;  // 时间轴
    QValueAxis*  m_axisY      = nullptr;  // 数值轴
    static constexpr int MAX_POINTS = 120;    // 曲线最多保留点数
};

#endif
