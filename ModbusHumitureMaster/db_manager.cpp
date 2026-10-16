#include "db_manager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>

db_manager::db_manager(QObject *parent)
    : QObject{parent}
{}

db_manager::~db_manager()
{
    if (m_db.isOpen())
        m_db.close();
    if (m_db.isValid()) {
        QString name = m_db.connectionName();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
}

// 懒打开 SQLite 连接：已打开直接返回；首次创建连接并指向程序目录下的 humiture.db
bool db_manager::openConnection()
{
    if (m_db.isOpen())
        return true;
    if (!m_db.isValid()) {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "humiture_conn");
        QString dbPath = QCoreApplication::applicationDirPath() + "/humiture.db";
        m_db.setDatabaseName(dbPath);
    }
    if (!m_db.open()) {
        emit dbError(m_db.lastError().text());
        return false;
    }
    return true;
}

// 初始化：建 humiture 表 + ts_ms 索引，成功后发 dbReady(true)
void db_manager::initDatabase()
{
    if (!openConnection()) {
        emit dbReady(false);
        return;
    }
    QSqlQuery query(m_db);
    bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS humiture ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  ts_ms       BIGINT NOT NULL,"
        "  record_time TEXT   NOT NULL,"
        "  temperature REAL   NOT NULL,"
        "  humidity    REAL   NOT NULL)"
    );
    if (!ok) {
        emit dbError(query.lastError().text());
        emit dbReady(false);
        return;
    }
    query.exec("CREATE INDEX IF NOT EXISTS idx_humiture_ts ON humiture(ts_ms)");
    m_initialized = true;
    qDebug() << "数据库初始化成功，文件："
             << QCoreApplication::applicationDirPath() + "/humiture.db";
    emit dbReady(true);
}

// 写入一条温湿度记录（含可读时间字符串），结果通过 saveResult 回传
void db_manager::saveReading(float temp, float hum, qint64 tsMs)
{
    if (!m_initialized && !openConnection())
        return;
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO humiture (ts_ms, record_time, temperature, humidity) "
                  "VALUES (:ts, :rt, :t, :h)");
    query.bindValue(":ts", tsMs);
    query.bindValue(":rt",
        QDateTime::fromMSecsSinceEpoch(tsMs).toString("yyyy-MM-dd hh:mm:ss.zzz"));
    query.bindValue(":t", temp);
    query.bindValue(":h", hum);
    if (!query.exec()) {
        emit dbError(query.lastError().text());
    } else {
        emit saveResult(true, tsMs);
    }
}

// 查询最近 limit 条（按时间升序），结果通过 queryResult 回传
void db_manager::queryRecent(int limit)
{
    if (!m_initialized && !openConnection())
        return;
    QSqlQuery query(m_db);
    query.prepare(QString(
        "SELECT ts_ms, record_time, temperature, humidity FROM humiture "
        "ORDER BY ts_ms DESC LIMIT %1").arg(limit));
    QList<QVariantMap> rows;
    if (query.exec()) {
        while (query.next()) {
            QVariantMap row;
            row["ts_ms"]       = query.value("ts_ms");
            row["record_time"] = query.value("record_time");
            row["temperature"] = query.value("temperature");
            row["humidity"]    = query.value("humidity");
            rows.append(row);
        }
    } else {
        emit dbError(query.lastError().text());
    }
    std::reverse(rows.begin(), rows.end());
    emit queryResult(rows);
}

// 清空 humiture 表所有记录
void db_manager::clearAll()
{
    if (!m_initialized && !openConnection())
        return;
    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM humiture"))
        emit dbError(query.lastError().text());
    else
        qDebug() << "已清空温湿度记录表";
}
