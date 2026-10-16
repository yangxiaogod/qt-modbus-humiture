#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include <QVariantMap>

// 温湿度数据持久化（SQLite），运行在独立线程
class db_manager : public QObject
{
    Q_OBJECT
public:
    explicit db_manager(QObject *parent = nullptr);
    ~db_manager() override;

public slots:
    void initDatabase();              // 建表并建索引
    void saveReading(float temp, float hum, qint64 tsMs);  // 写入一条记录
    void queryRecent(int limit);      // 查询最近 limit 条（按时间升序返回）
    void clearAll();                  // 清空记录表

signals:
    void dbReady(bool ok);            // 初始化结果
    void saveResult(bool ok, qint64 tsMs);
    void queryResult(const QList<QVariantMap> &rows);  // 查询结果
    void dbError(const QString &msg); // 错误信息

private:
    bool openConnection();            // 懒打开连接，已打开则直接返回
    QSqlDatabase m_db;
    bool m_initialized = false;
};

#endif
