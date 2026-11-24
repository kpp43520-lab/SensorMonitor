#pragma once                                      // 防止头文件重复包含
#include <QObject>                                // 引入 QObject，支持信号槽
#include <QSqlDatabase>                           // 引入 Qt SQL 数据库模块
#include <QVector>                                // 引入动态数组容器，用于缓存数据
#include "core/SensorValue.h"                     // 引入传感器数据结构体

class QTimer;                                     // 前置声明定时器类，减少头文件编译依赖

// 数据记录服务：把 SensorValue 批量写入 SQLite。
// 内部维护一个 pending 缓冲区，达到阈值或定时器触发时在一个事务里批量插入，
// 避免每条数据都开一次事务。
class RecordService : public QObject {            // 数据记录服务类，继承自 QObject
    Q_OBJECT                                      // 启用 Qt 元对象系统（支持信号槽）

public:
    explicit RecordService(const QString &dbPath, QObject *parent = nullptr); // 构造函数，传入数据库保存路径
    ~RecordService() override;                    // 析构函数，重写以处理关闭前的资源释放

    bool open();                                  // 打开数据库连接并初始化表结构
    void close();                                 // 关闭数据库连接
    bool isOpen() const;                          // 判断数据库当前是否已打开

    QString dbPath() const { return m_dbPath; }   // 获取当前数据库文件路径
    QString lastError() const { return m_lastError; } // 获取最近一次发生的错误信息
    QSqlDatabase database() const { return m_db; }    // 获取底层数据库对象（供历史查询模块使用）

public slots:
    void record(const SensorValue &v);            // 槽函数：接收一条数据，存入待写缓冲区
    void flush();                                 // 槽函数：强制将缓冲区中的所有数据立即写入数据库

signals:
    void errorOccurred(const QString &msg);       // 信号：发生数据库操作错误时发出通知

private:
    bool initSchema();                            // 内部函数：执行建表 SQL 语句和创建索引

    QString m_dbPath;                             // 数据库文件的保存路径
    QString m_lastError;                          // 缓存最近一次的错误信息字符串
    QSqlDatabase m_db;                            // Qt 的数据库连接对象
    QVector<SensorValue> m_pending;               // 数据缓存数组，暂存待写入的数据
    QTimer *m_flushTimer = nullptr;               // 定时器指针，定时触发强制写入（兜底）

    static constexpr int kBatchSize = 50;         // 静态常量：达到 50 条数据时自动触发写入
};