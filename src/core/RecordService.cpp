#include "core/RecordService.h"                   // 引入记录服务头文件
#include <QSqlQuery>                              // 引入 SQL 查询类，用于执行语句
#include <QSqlError>                              // 引入 SQL 错误类，用于获取错误信息
#include <QTimer>                                 // 引入定时器类，用于定时批量写入
#include <QVariant>                              // 引入变体类，用于绑定 SQL 参数

RecordService::RecordService(const QString &dbPath, QObject *parent) // 构造函数，传入数据库路径
    : QObject(parent), m_dbPath(dbPath) {         // 初始化列表：调用基类构造，保存路径
    m_flushTimer = new QTimer(this);              // 创建定时器对象
    m_flushTimer->setInterval(1000);   // 每秒兜底 flush 一次，防止数据滞留
    connect(m_flushTimer, &QTimer::timeout, this, &RecordService::flush); // 定时器超时触发批量写入
}

RecordService::~RecordService() {                 // 析构函数
    flush();                                      // 退出前强制将缓冲区数据写入数据库
    close();                                      // 关闭数据库连接
}

bool RecordService::open() {                      // 打开数据库
    if (m_db.isOpen()) return true;               // 如果已经打开，直接返回成功

    m_db = QSqlDatabase::addDatabase("QSQLITE", "record_conn"); // 添加 SQLite 数据库驱动并命名连接
    m_db.setDatabaseName(m_dbPath);               // 设置数据库文件路径
    if (!m_db.open()) {                           // 尝试打开数据库
        m_lastError = m_db.lastError().text();    // 记录错误信息
        emit errorOccurred(QStringLiteral("打开数据库失败：%1").arg(m_lastError)); // 发出打开失败信号
        return false;                             // 返回失败
    }

    if (!initSchema()) {                          // 如果初始化表结构失败
        m_db.close();                             // 关闭数据库
        return false;                             // 返回失败
    }

    m_flushTimer->start();                        // 启动定时器，开始定时批量写入
    return true;                                  // 返回成功
}

void RecordService::close() {                     // 关闭数据库
    m_flushTimer->stop();                         // 停止定时器
    flush();                                      // 强制写入剩余数据
    if (m_db.isOpen())                            // 如果数据库还开着
        m_db.close();                             // 关闭数据库
}

bool RecordService::isOpen() const {              // 判断数据库是否已打开
    return m_db.isOpen();                         // 返回内部数据库对象的打开状态
}

bool RecordService::initSchema() {                // 初始化数据库表结构
    QSqlQuery q(m_db);                            // 创建 SQL 查询对象，指定使用当前连接
    const QString sql = QStringLiteral(           // 准备建表 SQL 语句
        "CREATE TABLE IF NOT EXISTS sensor_data (" // 创建 sensor_data 表（如果不存在）
        "  id INTEGER PRIMARY KEY AUTOINCREMENT," // 自增主键
        "  timestamp_ms INTEGER NOT NULL,"        // 时间戳（毫秒级）
        "  sensor_id INTEGER NOT NULL,"           // 传感器 ID
        "  sensor_name TEXT,"                     // 传感器名称
        "  value REAL NOT NULL,"                  // 传感器数值（REAL 对应 float/double）
        "  unit INTEGER"                          // 单位代码
        ")");
    if (!q.exec(sql)) {                           // 执行建表语句
        m_lastError = q.lastError().text();       // 记录建表失败错误信息
        emit errorOccurred(QStringLiteral("建表失败：%1").arg(m_lastError)); // 发出建表失败信号
        return false;                             // 返回失败
    }
    // 创建时间戳索引，提高按时间范围查询的速度
    q.exec("CREATE INDEX IF NOT EXISTS idx_ts ON sensor_data(timestamp_ms)");
    // 创建复合索引，提高按传感器和时间组合查询的速度
    q.exec("CREATE INDEX IF NOT EXISTS idx_sensor_ts ON sensor_data(sensor_id, timestamp_ms)");
    return true;                                  // 初始化成功
}

void RecordService::record(const SensorValue &v) { // 接收一条需要记录的数据
    if (!m_db.isOpen()) return;                   // 如果数据库没打开，直接返回
    m_pending.append(v);                          // 将数据加入待写入缓冲区队列
    if (m_pending.size() >= kBatchSize)           // 如果缓冲区内数据达到 50 条
        flush();                                  // 立即触发批量写入
}

void RecordService::flush() {                     // 将缓冲区数据批量写入数据库
    if (m_pending.isEmpty() || !m_db.isOpen()) return; // 如果队列为空或数据库未打开，直接返回

    m_db.transaction();                           // 开启数据库事务（极大地提高批量插入速度）
    QSqlQuery q(m_db);                            // 创建 SQL 查询对象
    q.prepare("INSERT INTO sensor_data "          // 准备插入语句
              "(timestamp_ms, sensor_id, sensor_name, value, unit) "
              "VALUES (?, ?, ?, ?, ?)");          // 使用占位符防止 SQL 注入
    for (const SensorValue &v : m_pending) {      // 遍历缓冲区中的每一条数据
        q.addBindValue(v.timestamp.toMSecsSinceEpoch()); // 绑定时间戳
        q.addBindValue(v.id);                     // 绑定传感器 ID
        q.addBindValue(v.sensorName());           // 绑定传感器名称
        q.addBindValue(static_cast<double>(v.value)); // 绑定数值（强转为 double 适配 SQLite）
        q.addBindValue(v.unit);                   // 绑定单位
        if (!q.exec()) {                          // 如果单条插入失败
            m_lastError = q.lastError().text();   // 记录错误信息
            emit errorOccurred(QStringLiteral("插入失败：%1").arg(m_lastError)); // 发出插入失败信号
            break;                                // 跳出循环
        }
    }
    m_db.commit();                                // 提交事务（此时数据才真正写入磁盘）
    m_pending.clear();                            // 清空缓冲区队列
}