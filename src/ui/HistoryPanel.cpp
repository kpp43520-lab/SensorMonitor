#include "ui/HistoryPanel.h"                      // 引入历史查询面板头文件
#include "core/RecordService.h"                   // 引入记录服务头文件（用于获取数据库连接）

#include <QDateTimeEdit>                          // 引入时间编辑控件
#include <QComboBox>                              // 引入下拉框控件
#include <QTableView>                             // 引入表格视图控件
#include <QSqlQueryModel>                         // 引入 SQL 查询模型类
#include <QSqlQuery>                              // 引入 SQL 查询执行类
#include <QSqlError>                              // 引入 SQL 错误类
#include <QLabel>                                 // 引入标签控件
#include <QPushButton>                            // 引入按钮控件
#include <QHBoxLayout>                            // 引入水平布局
#include <QVBoxLayout>                            // 引入垂直布局
#include <QGroupBox>                              // 引入分组框控件
#include <QHeaderView>                            // 引入表头视图类
#include <QFileDialog>                            // 引入文件保存对话框
#include <QFile>                                  // 引入文件读写类
#include <QTextStream>                            // 引入文本流类
#include <QMessageBox>                            // 引入消息提示框类
#include <QDateTime>                              // 引入时间日期类

HistoryPanel::HistoryPanel(RecordService *service, QWidget *parent) // 构造函数，注入服务指针
    : QWidget(parent), m_service(service) {       // 初始化列表：调用基类构造，保存服务指针

    auto *layout = new QVBoxLayout(this);         // 创建主垂直布局

    // ===== 查询条件 =====
    auto *filterGroup = new QGroupBox(QStringLiteral("查询条件")); // 创建查询条件分组框
    auto *filterLayout = new QHBoxLayout(filterGroup); // 在分组框内创建水平布局

    filterLayout->addWidget(new QLabel(QStringLiteral("起始:"))); // 添加“起始”标签
    m_fromEdit = new QDateTimeEdit;               // 创建起始时间编辑框
    m_fromEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss"); // 设置时间显示格式
    m_fromEdit->setCalendarPopup(true);           // 开启日历弹窗选择
    m_fromEdit->setDateTime(QDateTime::currentDateTime().addSecs(-3600)); // 默认设为1小时前
    filterLayout->addWidget(m_fromEdit);          // 添加到布局

    filterLayout->addWidget(new QLabel(QStringLiteral("结束:"))); // 添加“结束”标签
    m_toEdit = new QDateTimeEdit;                 // 创建结束时间编辑框
    m_toEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss"); // 设置时间显示格式
    m_toEdit->setCalendarPopup(true);             // 开启日历弹窗选择
    m_toEdit->setDateTime(QDateTime::currentDateTime()); // 默认设为当前时间
    filterLayout->addWidget(m_toEdit);            // 添加到布局

    filterLayout->addWidget(new QLabel(QStringLiteral("传感器:"))); // 添加“传感器”标签
    m_sensorCombo = new QComboBox;                // 创建传感器筛选下拉框
    m_sensorCombo->setMinimumWidth(140);          // 设置最小宽度
    m_sensorCombo->addItem(QStringLiteral("全部"), 0); // 添加“全部”选项（数据值为0）
    filterLayout->addWidget(m_sensorCombo);       // 添加到布局

    auto *refreshBtn = new QPushButton(QStringLiteral("刷新列表")); // 创建刷新列表按钮
    auto *queryBtn   = new QPushButton(QStringLiteral("查询"));     // 创建查询按钮
    auto *resetBtn   = new QPushButton(QStringLiteral("重置时间")); // 创建重置时间按钮
    auto *exportBtn  = new QPushButton(QStringLiteral("导出 CSV")); // 创建导出CSV按钮
    filterLayout->addWidget(refreshBtn);          // 添加刷新按钮到布局
    filterLayout->addWidget(queryBtn);            // 添加查询按钮到布局
    filterLayout->addWidget(resetBtn);            // 添加重置按钮到布局
    filterLayout->addWidget(exportBtn);           // 添加导出按钮到布局
    filterLayout->addStretch();                   // 添加弹簧，把控件推到左侧

    layout->addWidget(filterGroup);               // 将查询条件分组框添加到主布局

    // ===== 结果表格 =====
    m_tableView = new QTableView;                 // 创建表格视图控件
    m_tableView->setSortingEnabled(false);        // 禁用点击表头排序（避免与 SQL 模型冲突）
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // 设置表格不可编辑
    m_tableView->horizontalHeader()->setStretchLastSection(true); // 设置最后一列自动拉伸填满
    m_model = new QSqlQueryModel(this);           // 创建 SQL 查询模型
    m_tableView->setModel(m_model);               // 将模型绑定到视图
    layout->addWidget(m_tableView, 1);            // 添加表格到主布局，并设置拉伸因子为1

    // ===== 状态栏 =====
    auto *bottomBar = new QHBoxLayout;            // 创建底部状态栏水平布局
    m_countLabel = new QLabel(QStringLiteral("未查询")); // 创建状态标签
    bottomBar->addWidget(m_countLabel);           // 添加状态标签到布局
    bottomBar->addStretch();                      // 添加弹簧，让标签靠左
    layout->addLayout(bottomBar);                 // 将底部状态栏添加到主布局

    connect(refreshBtn, &QPushButton::clicked, this, &HistoryPanel::refreshSensorList); // 绑定刷新按钮点击事件
    connect(queryBtn,   &QPushButton::clicked, this, &HistoryPanel::runQuery);          // 绑定查询按钮点击事件
    connect(resetBtn,   &QPushButton::clicked, this, &HistoryPanel::onResetClicked);    // 绑定重置按钮点击事件
    connect(exportBtn,  &QPushButton::clicked, this, &HistoryPanel::onExportClicked);   // 绑定导出按钮点击事件

    if (m_service) {                              // 如果服务指针有效
        connect(m_service, &RecordService::errorOccurred, this, [this](const QString &msg) { // 监听数据库错误信号
            m_countLabel->setText(QStringLiteral("数据库错误：%1").arg(msg)); // 将错误信息显示在状态栏
        });
    }

    refreshSensorList();                          // 初始化时自动加载一次传感器列表
}

void HistoryPanel::refreshSensorList() {          // 刷新传感器下拉列表
    if (!m_service || !m_service->isOpen()) return; // 服务未初始化或数据库未打开则返回

    const int currentId = m_sensorCombo->currentData().toInt(); // 记录当前选中的传感器 ID

    m_sensorCombo->clear();                       // 清空下拉框
    m_sensorCombo->addItem(QStringLiteral("全部"), 0); // 重新添加“全部”选项

    QSqlQuery q(m_service->database());           // 在指定数据库连接上创建查询对象
    q.exec("SELECT DISTINCT sensor_id, sensor_name " // 执行查询：获取不重复的传感器 ID 和名称
           "FROM sensor_data ORDER BY sensor_id"); // 按 ID 排序
    while (q.next()) {                            // 遍历查询结果
        const int id = q.value(0).toInt();        // 获取传感器 ID
        const QString name = q.value(1).toString(); // 获取传感器名称
        m_sensorCombo->addItem(QStringLiteral("%1 (ID=%2)").arg(name).arg(id), id); // 将项添加到下拉框，并绑定数据为 ID
    }

    const int idx = m_sensorCombo->findData(currentId); // 查找之前选中的 ID 在新列表中的位置
    if (idx >= 0) m_sensorCombo->setCurrentIndex(idx);  // 如果找到，恢复选中状态
}

void HistoryPanel::runQuery() {                   // 执行历史数据查询
    if (!m_service || !m_service->isOpen()) {     // 如果数据库未打开
        m_countLabel->setText(QStringLiteral("数据库未打开")); // 显示错误提示
        return;                                   // 返回
    }

    // 先 flush 缓冲区，保证查询结果包含刚刚收到的数据
    m_service->flush();                           // 强制将缓存区的数据写入数据库

    const qint64 tFrom = m_fromEdit->dateTime().toMSecsSinceEpoch(); // 获取起始时间戳（毫秒）
    const qint64 tTo   = m_toEdit->dateTime().toMSecsSinceEpoch();   // 获取结束时间戳（毫秒）
    const int sensorId = m_sensorCombo->currentData().toInt();       // 获取选中的传感器 ID（0为全部）

    QString sql = QStringLiteral(                 // 准备 SQL 语句
        "SELECT "
        "  datetime(timestamp_ms/1000, 'unixepoch', 'localtime') AS 时间," // 将时间戳转换为本地时间字符串
        "  sensor_name AS 传感器,"                // 查询传感器名称
        "  sensor_id   AS ID,"                    // 查询传感器 ID
        "  printf('%.2f', value) AS 数值,"        // 将浮点数格式化为两位小数
        "  unit        AS 单位"                   // 查询单位代码
        " FROM sensor_data"                       // 从数据表查询
        " WHERE timestamp_ms BETWEEN :from AND :to"); // 时间范围过滤条件

    if (sensorId > 0)                             // 如果指定了特定传感器
        sql += QStringLiteral(" AND sensor_id = :sid"); // 增加传感器 ID 过滤条件
    sql += QStringLiteral(" ORDER BY timestamp_ms DESC LIMIT 5000"); // 按时间倒序排列，限制最多5000条

    QSqlQuery query(m_service->database());       // 创建查询对象
    query.prepare(sql);                           // 准备执行带占位符的 SQL
    query.bindValue(":from", tFrom);              // 绑定起始时间参数
    query.bindValue(":to", tTo);                  // 绑定结束时间参数
    if (sensorId > 0)                             // 如果指定了传感器
        query.bindValue(":sid", sensorId);        // 绑定传感器 ID 参数

    if (!query.exec()) {                          // 如果查询执行失败
        m_countLabel->setText(QStringLiteral("查询失败：%1").arg(query.lastError().text())); // 显示错误信息
        return;                                   // 返回
    }

    m_model->setQuery(std::move(query));          // 将执行成功的查询对象移交给模型展示
    m_countLabel->setText(QStringLiteral("共 %1 条记录（最多显示 5000 条）") // 更新状态栏
                              .arg(m_model->rowCount())); // 显示查询结果条数
}

void HistoryPanel::onResetClicked() {             // 重置时间范围
    m_toEdit->setDateTime(QDateTime::currentDateTime()); // 结束时间设为当前
    m_fromEdit->setDateTime(QDateTime::currentDateTime().addSecs(-3600)); // 起始时间设为1小时前
}

void HistoryPanel::onExportClicked() {            // 将查询结果导出为 CSV 文件
    if (m_model->rowCount() == 0) {               // 如果表格没有数据
        QMessageBox::information(this, QStringLiteral("提示"), // 弹出提示框
                                 QStringLiteral("没有可导出的数据")); // 提示无数据
        return;                                   // 返回
    }
    const QString path = QFileDialog::getSaveFileName( // 弹出保存文件对话框
        this, QStringLiteral("导出 CSV"),         // 对话框标题
        QStringLiteral("history.csv"),            // 默认文件名
        QStringLiteral("CSV 文件 (*.csv)"));      // 文件过滤器
    if (path.isEmpty()) return;                   // 如果用户取消了保存，直接返回

    QFile f(path);                                // 创建文件对象
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { // 以只写和文本模式打开
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法写入文件")); // 打开失败提示
        return;                                   // 返回
    }
    QTextStream out(&f);                          // 创建文件输出流
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)       // 如果是 Qt6 环境
    out.setEncoding(QStringConverter::Utf8);      // 设置编码为 UTF-8
#else                                             // 如果是 Qt5 环境
    out.setCodec("UTF-8");                        // 设置编码为 UTF-8
#endif

    QStringList headers;                          // 准备表头集合
    for (int c = 0; c < m_model->columnCount(); ++c) // 遍历每一列
        headers << m_model->headerData(c, Qt::Horizontal).toString(); // 获取列名
    out << headers.join(",") << "\n";             // 写入表头并用逗号分隔

    for (int r = 0; r < m_model->rowCount(); ++r) { // 遍历每一行数据
        QStringList row;                          // 准备当前行的数据集合
        for (int c = 0; c < m_model->columnCount(); ++c) { // 遍历每一列
            QString v = m_model->data(m_model->index(r, c)).toString(); // 获取单元格文本
            v.replace("\"", "\"\"");              // 将双引号替换为两个双引号（CSV标准转义）
            if (v.contains(",") || v.contains("\"")) // 如果文本中包含逗号或双引号
                v = "\"" + v + "\"";              // 用双引号将整个文本包裹起来
            row << v;                             // 将处理后的文本加入当前行集合
        }
        out << row.join(",") << "\n";             // 写入当前行数据，用逗号分隔
    }
    f.close();                                    // 关闭文件
    m_countLabel->setText(QStringLiteral("已导出到 %1").arg(path)); // 更新状态栏显示导出路径
}