#include "ui/MainWindow.h"                        // 引入主窗口头文件

// 核心业务类
#include "datasource/MockDataSource.h"            // 引入模拟数据源类
#include "datasource/SerialDataSource.h"          // 引入串口数据源类
#include "datasource/TcpDataSource.h"             // 引入 TCP 数据源类
#include "ui/RealtimeChart.h"                     // 引入实时曲线控件类
#include "ui/HistoryPanel.h"                      // 引入历史查询面板类
#include "core/RecordService.h"                   // 引入数据库记录服务类
#include "mqtt/MqttPublisher.h"                   // 引入 MQTT 发布器类
#include "mqtt/MqttConfig.h"                      // 引入 MQTT 配置结构体

// Qt 控件与模块
#include <QComboBox>                              // 下拉框
#include <QPushButton>                            // 按钮
#include <QLabel>                                 // 文本标签
#include <QTextEdit>                              // 文本编辑框
#include <QTableWidget>                           // 表格控件
#include <QTableWidgetItem>                       // 表格项
#include <QAbstractItemView>                      // 抽象视图（用于设置表格属性）
#include <QHeaderView>                            // 表头
#include <QSplitter>                              // 分割器
#include <QVBoxLayout>                            // 垂直布局
#include <QHBoxLayout>                            // 水平布局
#include <QGroupBox>                              // 分组框
#include <QFontDatabase>                          // 字体数据库
#include <QLineEdit>                              // 单行输入框
#include <QCheckBox>                              // 复选框
#include <QTabWidget>                             // 页签控件
#include <QTimer>                                 // 定时器
#include <QSerialPortInfo>                        // 串口信息类（用于枚举端口）
#include <QJsonObject>                            // JSON 对象
#include <QJsonDocument>                          // JSON 文档
#include <QDateTime>                              // 时间日期类
#include <QCoreApplication>                       // 核心应用类（用于获取路径）
#include <utility>                                // 标准工具库（用于 std::as_const）

// ========================================================
// 辅助函数：将字节流转为十六进制字符串（用于原始帧显示）
// ========================================================
static QString toHex(const QByteArray &data) {    // 定义静态辅助函数
    QString s;                                    // 创建结果字符串
    for (unsigned char c : data)                  // 遍历字节数组中的每个字节
        s += QStringLiteral("%1 ").arg(c, 2, 16, QChar('0')).toUpper(); // 格式化为2位大写十六进制并拼接
    return s.trimmed();                           // 去除末尾空格并返回
}

// ========================================================
// 构造与析构
// ========================================================
MainWindow::MainWindow(QWidget *parent)           // 构造函数
    : QMainWindow(parent), m_parser(new ProtocolParser(this)) { // 初始化列表：调用基类，创建解析器

    // 1. 初始化数据库记录服务（数据库文件存在 exe 同级目录）
    const QString dbPath = QCoreApplication::applicationDirPath() // 获取应用程序所在目录
                           + QStringLiteral("/sensor_data.db"); // 拼接数据库文件名
    m_record = new RecordService(dbPath, this);   // 创建记录服务对象
    if (!m_record->open()) {                      // 尝试打开数据库
        qWarning() << "数据库打开失败:" << m_record->lastError(); // 如果失败，输出警告日志
    }

    // 2. 初始化 MQTT 客户端
    m_mqtt = new MqttPublisher(this);             // 创建 MQTT 发布器对象

    // 3. 初始化 MQTT 定时发布器（每秒发布一次）
    m_mqttPublishTimer = new QTimer(this);        // 创建定时器对象
    m_mqttPublishTimer->setInterval(1000);        // 设置间隔为 1000 毫秒
    connect(m_mqttPublishTimer, &QTimer::timeout, this, &MainWindow::onMqttPublishTimer); // 绑定超时信号

    // 4. 构建界面
    setupUi();                                    // 调用 UI 初始化函数

    // 5. 连接解析器信号
    connect(m_parser, &ProtocolParser::rawFrame,    this, &MainWindow::onRawFrame);    // 完整帧解析成功信号
    connect(m_parser, &ProtocolParser::frameParsed, this, &MainWindow::onFrameParsed); // 数据域解析成功信号
    connect(m_parser, &ProtocolParser::parseError,  this, &MainWindow::onParseError);  // 解析错误信号

    // 6. 连接曲线信号（动态生成复选框）
    connect(m_chart, &RealtimeChart::seriesAdded, this, &MainWindow::onSeriesAdded);   // 新曲线增加信号

    // 7. 连接 MQTT 信号
    connect(m_mqtt, &MqttPublisher::logMessage, this, &MainWindow::appendLog);         // MQTT 日志信号
    connect(m_mqtt, &MqttPublisher::connectedChanged, this, &MainWindow::onMqttConnectedChanged); // MQTT 状态改变信号

    // 8. 创建默认数据源（Mock）
    createSource();                               // 初始化并创建第一个数据源

    // 9. 连接数据源切换和刷新信号的槽函数
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onSourceTypeChanged); // 切换数据源信号
    connect(m_refreshPortBtn, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);             // 刷新串口按钮点击信号
}

// ========================================================
// UI 搭建
// ========================================================
void MainWindow::setupUi() {                      // 构建界面的核心函数
    setWindowTitle(QStringLiteral("工业传感器监控 HMI -数据采集")); // 设置窗口标题
    resize(1280, 850);                            // 设置窗口默认尺寸

    auto *central = new QWidget(this);            // 创建中心部件
    auto *centralLayout = new QVBoxLayout(central); // 创建主垂直布局

    // 主 Tab 页签：实时监控、历史查询
    auto *tabs = new QTabWidget;                  // 创建页签控件

    // ----------------------------------------------------
    // Tab 1：实时监控
    // ----------------------------------------------------
    auto *liveTab = new QWidget;                  // 创建实时监控页面部件
    auto *mainLayout = new QVBoxLayout(liveTab);  // 创建该页面的主垂直布局

    // --- 顶栏配置区 ---
    auto *topBar = new QHBoxLayout;               // 创建顶栏水平布局

    // 数据源选择
    topBar->addWidget(new QLabel(QStringLiteral("数据源:"))); // 添加“数据源”标签
    m_sourceCombo = new QComboBox;                // 创建数据源下拉框
    m_sourceCombo->addItem(QStringLiteral("Mock 模拟")); // 添加 Mock 选项
    m_sourceCombo->addItem(QStringLiteral("Serial 串口")); // 添加串口选项
    m_sourceCombo->addItem(QStringLiteral("TCP 网络"));    // 添加 TCP 选项
    topBar->addWidget(m_sourceCombo);             // 将下拉框添加到顶栏

    // 串口配置区
    m_serialConfigWidget = new QWidget;           // 创建串口配置容器
    auto *serialLayout = new QHBoxLayout(m_serialConfigWidget); // 创建容器内的水平布局
    serialLayout->setContentsMargins(0, 0, 0, 0); // 设置边距为0
    serialLayout->addWidget(new QLabel(QStringLiteral("端口:"))); // 添加端口标签
    m_portCombo = new QComboBox;                  // 创建端口下拉框
    m_portCombo->setMinimumWidth(150);            // 设置最小宽度
    serialLayout->addWidget(m_portCombo);         // 添加到布局
    serialLayout->addWidget(new QLabel(QStringLiteral("波特率:"))); // 添加波特率标签
    m_baudCombo = new QComboBox;                  // 创建波特率下拉框
    const int bauds[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600}; // 定义常用波特率
    for (int b : bauds) m_baudCombo->addItem(QString::number(b), b); // 循环添加波特率选项
    m_baudCombo->setCurrentText(QStringLiteral("115200")); // 设置默认选中 115200
    serialLayout->addWidget(m_baudCombo);         // 添加到布局
    m_refreshPortBtn = new QPushButton(QStringLiteral("刷新")); // 创建刷新按钮
    serialLayout->addWidget(m_refreshPortBtn);    // 添加到布局
    topBar->addWidget(m_serialConfigWidget);      // 将串口配置区添加到顶栏
    m_serialConfigWidget->setVisible(false);      // 初始状态隐藏

    // TCP 配置区
    m_tcpConfigWidget = new QWidget;              // 创建 TCP 配置容器
    auto *tcpLayout = new QHBoxLayout(m_tcpConfigWidget); // 创建容器内的水平布局
    tcpLayout->setContentsMargins(0, 0, 0, 0);    // 设置边距为0
    tcpLayout->addWidget(new QLabel(QStringLiteral("IP:"))); // 添加 IP 标签
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1")); // 创建 IP 输入框
    m_hostEdit->setFixedWidth(120);               // 设置固定宽度
    tcpLayout->addWidget(m_hostEdit);             // 添加到布局
    tcpLayout->addWidget(new QLabel(QStringLiteral("端口:"))); // 添加端口标签
    m_portEdit = new QLineEdit(QStringLiteral("9000")); // 创建端口输入框
    m_portEdit->setFixedWidth(60);                // 设置固定宽度
    tcpLayout->addWidget(m_portEdit);             // 添加到布局
    topBar->addWidget(m_tcpConfigWidget);         // 将 TCP 配置区添加到顶栏
    m_tcpConfigWidget->setVisible(false);         // 初始状态隐藏

    // 控制按钮与 MQTT
    m_startBtn = new QPushButton(QStringLiteral("启动采集")); // 创建启动按钮
    m_stopBtn  = new QPushButton(QStringLiteral("停止采集")); // 创建停止按钮
    m_mqttBtn  = new QPushButton(QStringLiteral("连接 MQTT")); // 创建 MQTT 按钮
    topBar->addWidget(m_startBtn);                // 添加到顶栏
    topBar->addWidget(m_stopBtn);                 // 添加到顶栏
    topBar->addWidget(m_mqttBtn);                 // 添加到顶栏

    m_stateLabel = new QLabel(QStringLiteral("已停止")); // 创建状态显示标签
    topBar->addWidget(m_stateLabel);              // 添加到顶栏

    topBar->addStretch();                         // 添加弹簧，将后面的控件推到右侧
    m_statsLabel = new QLabel(QStringLiteral("帧: 0   错误: 0")); // 创建统计信息标签
    topBar->addWidget(m_statsLabel);              // 添加到顶栏
    mainLayout->addLayout(topBar);                // 将顶栏布局添加到主布局

    // --- 曲线区域 ---
    auto *chartGroup = new QGroupBox(QStringLiteral("实时曲线（最近 30 秒）")); // 创建曲线分组框
    auto *chartLayout = new QVBoxLayout(chartGroup); // 创建曲线区域的垂直布局

    // 曲线顶部勾选条
    auto *legendBar = new QHBoxLayout;            // 创建顶部勾选条水平布局
    legendBar->addWidget(new QLabel(QStringLiteral("显示曲线:"))); // 添加标签
    m_legendLayout = new QHBoxLayout;             // 创建存放复选框的布局
    m_legendLayout->setSpacing(15);               // 设置间距
    legendBar->addLayout(m_legendLayout);         // 添加到勾选条布局
    legendBar->addStretch();                      // 添加弹簧
    m_clearChartBtn = new QPushButton(QStringLiteral("清空曲线")); // 创建清空曲线按钮
    legendBar->addWidget(m_clearChartBtn);        // 添加到勾选条布局
    chartLayout->addLayout(legendBar);            // 将勾选条添加到曲线区域布局

    m_chart = new RealtimeChart;                  // 创建实时曲线控件
    m_chart->setWindowSeconds(30);                // 设置显示最近30秒的数据
    chartLayout->addWidget(m_chart, 1);           // 将曲线控件添加到布局，拉伸因子为1

    // --- 原始帧 + 表格 ---
    auto *hSplitter = new QSplitter(Qt::Horizontal); // 创建水平分割器

    // 原始帧
    auto *rawGroup = new QGroupBox(QStringLiteral("原始帧（Hex）")); // 创建原始帧分组框
    auto *rawLayout = new QVBoxLayout(rawGroup);  // 创建分组框内的垂直布局
    m_rawView = new QTextEdit;                    // 创建文本框
    m_rawView->setReadOnly(true);                 // 设置为只读
    m_rawView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont)); // 设置等宽字体
    m_rawView->document()->setMaximumBlockCount(500); // 限制最多显示 500 行，防止内存暴涨
    rawLayout->addWidget(m_rawView);              // 添加到布局
    hSplitter->addWidget(rawGroup);               // 将原始帧分组框添加到分割器

    // 数值表格
    auto *tableGroup = new QGroupBox(QStringLiteral("解析结果")); // 创建表格分组框
    auto *tableLayout = new QVBoxLayout(tableGroup); // 创建分组框内的垂直布局
    m_table = new QTableWidget(0, 5);             // 创建 0 行 5 列的表格
    m_table->setHorizontalHeaderLabels(           // 设置表头
        {QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("数值"),
         QStringLiteral("单位"), QStringLiteral("更新时间")});
    m_table->horizontalHeader()->setStretchLastSection(true); // 最后一列自适应拉伸
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); // 禁用编辑
    tableLayout->addWidget(m_table);              // 添加到布局
    hSplitter->addWidget(tableGroup);             // 将表格分组框添加到分割器

    hSplitter->setStretchFactor(0, 1);            // 设置分割器左侧拉伸因子
    hSplitter->setStretchFactor(1, 1);            // 设置分割器右侧拉伸因子

    // --- 垂直布局放入曲线和分割器 ---
    auto *vSplitter = new QSplitter(Qt::Vertical); // 创建垂直分割器
    vSplitter->addWidget(chartGroup);             // 添加曲线区域
    vSplitter->addWidget(hSplitter);              // 添加水平分割器（包含原始帧和表格）
    vSplitter->setStretchFactor(0, 3);            // 曲线区域占比较大
    vSplitter->setStretchFactor(1, 2);            // 原始帧和表格区域占比较小
    mainLayout->addWidget(vSplitter, 1);          // 将垂直分割器添加到主布局

    // --- 运行日志 ---
    auto *logGroup = new QGroupBox(QStringLiteral("运行日志")); // 创建日志分组框
    auto *logLayout = new QVBoxLayout(logGroup);  // 创建分组框内的垂直布局
    m_logView = new QTextEdit;                    // 创建日志文本框
    m_logView->setReadOnly(true);                 // 设置为只读
    m_logView->document()->setMaximumBlockCount(300); // 限制日志行数
    logLayout->addWidget(m_logView);              // 添加到布局
    mainLayout->addWidget(logGroup);              // 将日志分组框添加到主布局

    tabs->addTab(liveTab, QStringLiteral("实时监控")); // 将实时监控页面添加到页签

    // ----------------------------------------------------
    // Tab 2：历史查询
    // ----------------------------------------------------
    m_historyPanel = new HistoryPanel(m_record);  // 创建历史查询面板，注入数据库服务
    tabs->addTab(m_historyPanel, QStringLiteral("历史查询")); // 添加到页签

    // 切换到历史查询时刷新列表
    connect(tabs, &QTabWidget::currentChanged, this, [this](int idx) { // 监听页签切换信号
        if (idx == 1 && m_historyPanel) {         // 如果切换到索引为1的页面（历史查询）
            m_historyPanel->refreshSensorList();  // 刷新传感器下拉列表
        }
    });

    centralLayout->addWidget(tabs);               // 将页签控件添加到中心布局
    setCentralWidget(central);                    // 设置中心部件

    // 连接UI按钮信号槽
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked); // 启动按钮
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);   // 停止按钮
    connect(m_clearChartBtn, &QPushButton::clicked, this, &MainWindow::onClearChart); // 清空曲线按钮
    connect(m_mqttBtn, &QPushButton::clicked, this, &MainWindow::onMqttConnectClicked); // MQTT按钮

    // 初始化时刷新一下端口列表
    refreshPortList();                            // 扫描并显示当前系统的串口
}

// ========================================================
// 数据源管理
// ========================================================
void MainWindow::createSource() {                 // 根据当前配置创建数据源实例
    // 清空曲线和复选框
    if (m_chart) {                                // 如果曲线控件已创建
        m_chart->clear();                         // 清空曲线数据
        clearSeriesChecks();                      // 清理旧的复选框
    }

    // 停掉旧源并销毁
    if (m_source) {                               // 如果旧数据源存在
        if (m_source->isRunning()) m_source->stop(); // 如果正在运行，先停止
        m_source->disconnect(this);               // 断开与主窗口的所有信号槽连接
        delete m_source;                          // 释放内存
        m_source = nullptr;                       // 指针置空
    }

    // 创建新源
    const int idx = m_sourceCombo->currentIndex(); // 获取当前选择的数据源索引
    if (idx == 0) {                               // 索引 0：Mock 模拟
        m_source = new MockDataSource(this);      // 创建模拟数据源对象
    } else if (idx == 1) {                        // 索引 1：串口
        const QString port = m_portCombo->currentData().toString(); // 获取选中的串口名
        const qint32 baud = m_baudCombo->currentData().toInt();     // 获取选中的波特率
        m_source = new SerialDataSource(port, baud, this);          // 创建串口数据源对象
    } else {                                      // 索引 2：TCP
        const QString host = m_hostEdit->text().trimmed();          // 获取输入的 IP
        const quint16 port = static_cast<quint16>(m_portEdit->text().toUInt()); // 获取输入的端口
        m_source = new TcpDataSource(host, port, this);             // 创建 TCP 数据源对象
    }

    // 连接通用接口信号
    connect(m_source, &IDataSource::dataArrived, this, &MainWindow::onDataArrived); // 收到数据信号
    connect(m_source, &IDataSource::stateChanged, this, [this](bool running) {      // 状态改变信号
        m_stateLabel->setText(running ? QStringLiteral("运行中") : QStringLiteral("已停止")); // 更新状态标签
        m_startBtn->setEnabled(!running);         // 运行时禁用启动按钮
        m_stopBtn->setEnabled(running);           // 运行时启用停止按钮
    });
    connect(m_source, &IDataSource::errorOccurred, this, [this](const QString &msg) { // 发生错误信号
        appendLog(QStringLiteral("数据源错误：%1").arg(msg)); // 在日志中显示错误
    });

    m_startBtn->setEnabled(true);                 // 初始状态启用启动按钮
    m_stopBtn->setEnabled(false);                 // 初始状态禁用停止按钮
    m_stateLabel->setText(QStringLiteral("已停止")); // 重置状态标签
    appendLog(QStringLiteral("已切换到数据源：%1").arg(m_source->name())); // 记录切换数据源日志
}

void MainWindow::onSourceTypeChanged(int index) { // 下拉框切换数据源类型
    m_serialConfigWidget->setVisible(index == 1); // 如果是串口，显示串口配置区
    m_tcpConfigWidget->setVisible(index == 2);    // 如果是TCP，显示TCP配置区
    createSource();                               // 重新创建数据源
}

void MainWindow::onRefreshPorts() {               // 刷新按钮点击
    refreshPortList();                            // 调用刷新函数
}

void MainWindow::refreshPortList() {              // 扫描并更新可用串口列表
    m_portCombo->clear();                         // 清空下拉框
    const auto ports = QSerialPortInfo::availablePorts(); // 获取系统可用串口列表
    for (const QSerialPortInfo &info : ports) {   // 遍历每个串口
        QString text = info.portName();           // 获取串口名（如 COM1）
        if (!info.description().isEmpty()) {      // 如果有描述信息
            text += QStringLiteral(" - ") + info.description(); // 拼接描述信息
        }
        m_portCombo->addItem(text, info.portName()); // 添加到下拉框，绑定数据为纯串口名
    }
    if (ports.isEmpty()) {                        // 如果没有找到任何串口
        appendLog(QStringLiteral("未发现可用串口")); // 记录日志
    }
}

// ========================================================
// 数据采集控制
// ========================================================
void MainWindow::onStartClicked() {               // 点击启动按钮
    if (!m_source) return;                        // 如果没有数据源，直接返回
    if (m_source->start())                        // 尝试启动数据源
        appendLog(QStringLiteral("数据源已启动")); // 成功，记录日志
    else
        appendLog(QStringLiteral("数据源启动失败")); // 失败，记录日志
}

void MainWindow::onStopClicked() {                // 点击停止按钮
    if (m_source) {                               // 如果有数据源
        m_source->stop();                         // 停止数据源
        if (m_record) m_record->flush();          // 停止时强制将残留数据写入数据库
        appendLog(QStringLiteral("数据源已停止")); // 记录日志
    }
}

// ========================================================
// 数据解析与处理（核心数据流）
// ========================================================
void MainWindow::onDataArrived(const QByteArray &bytes) { // 收到原始字节流
    m_parser->append(bytes);                      // 将数据喂给解析器
}

void MainWindow::onRawFrame(const QByteArray &frame) { // 解析出完整的一帧
    m_rawView->append(toHex(frame));              // 转为十六进制并显示在文本框中
    updateStats();                                // 更新顶部的统计数据
}

void MainWindow::onFrameParsed(const QVector<SensorValue> &values) { // 解析出具体的传感器数据
    for (const SensorValue &v : values) {         // 遍历解析出的每一个数据
        // 1. 更新表格
        updateSensorRow(v);                       // 更新表格中的数据显示

        // 2. 更新曲线
        if (m_chart) m_chart->appendValue(v.id, v.value, v.timestamp); // 将数据点添加到曲线

        // 3. 写入数据库
        if (m_record) m_record->record(v);        // 将数据加入数据库写入队列

        // 4. 缓存最新值，供 MQTT 定时上传使用
        m_latestValues[v.id] = v;                 // 更新缓存的最新数据
    }
}

void MainWindow::onParseError(const QString &reason) { // 解析发生错误
    appendLog(QStringLiteral("解析错误：%1").arg(reason)); // 在日志中显示错误原因
    updateStats();                                // 更新统计数据
}

// ========================================================
// UI 辅助更新
// ========================================================
void MainWindow::updateSensorRow(const SensorValue &v) { // 更新表格中某一行数据
    int row = 0;                                  // 默认行号
    if (m_rowBySensorId.contains(v.id)) {         // 如果该传感器 ID 已经有对应的行
        row = m_rowBySensorId[v.id];              // 取出已有的行号
    } else {                                      // 如果是新出现的传感器
        row = m_table->rowCount();                // 获取当前行数作为新行号
        m_table->insertRow(row);                  // 在表格中插入新行
        m_rowBySensorId[v.id] = row;              // 记录该传感器 ID 对应的行号
    }

    m_table->setItem(row, 0, new QTableWidgetItem(QString::number(v.id))); // 设置 ID 列
    m_table->setItem(row, 1, new QTableWidgetItem(v.sensorName()));        // 设置名称列
    m_table->setItem(row, 2, new QTableWidgetItem(QString::number(v.value, 'f', 2))); // 设置数值列（保留两位小数）
    m_table->setItem(row, 3, new QTableWidgetItem(v.unitString()));        // 设置单位列
    m_table->setItem(row, 4, new QTableWidgetItem(v.timestamp.toString("hh:mm:ss.zzz"))); // 设置时间列
}

void MainWindow::appendLog(const QString &text) { // 添加运行日志
    m_logView->append(text);                      // 在日志文本框中追加一行
}

void MainWindow::updateStats() {                  // 更新顶部的帧数统计
    m_statsLabel->setText(QStringLiteral("帧: %1   错误: %2") // 格式化字符串
                              .arg(m_parser->frameCount())    // 获取成功帧数
                              .arg(m_parser->errorCount()));  // 获取错误帧数
}

// ========================================================
// 曲线控制
// ========================================================
void MainWindow::onClearChart() {                 // 点击清空曲线
    if (!m_chart) return;                         // 如果曲线控件不存在，返回
    m_chart->clear();                             // 清空曲线数据
    clearSeriesChecks();                          // 清空复选框
    appendLog(QStringLiteral("实时曲线已清空（数据库记录不受影响）")); // 记录日志
}

void MainWindow::onSeriesAdded(quint8 id, const QString &name, const QColor &color) { // 新增曲线时触发
    if (m_seriesChecks.contains(id)) return;      // 如果已经存在该传感器的复选框，返回

    auto *cb = new QCheckBox(name);               // 创建复选框
    cb->setChecked(true);                         // 默认勾选
    cb->setStyleSheet(QStringLiteral("QCheckBox { color: %1; font-weight: bold; }").arg(color.name())); // 设置文字颜色与曲线一致并加粗

    connect(cb, &QCheckBox::toggled, this, [this, id](bool on) { // 绑定勾选状态变化信号
        if (m_chart) m_chart->setSeriesVisible(id, on); // 切换对应曲线的可见性
    });

    m_legendLayout->addWidget(cb);                // 将复选框添加到布局
    m_seriesChecks.insert(id, cb);                // 记录指针以便后续清理
}

void MainWindow::clearSeriesChecks() {            // 清理所有复选框
    if (!m_legendLayout) return;                  // 如果布局不存在，返回
    for (auto *cb : std::as_const(m_seriesChecks)) { // 遍历所有复选框
        m_legendLayout->removeWidget(cb);         // 从布局中移除
        cb->deleteLater();                        // 安全释放内存
    }
    m_seriesChecks.clear();                       // 清空映射表
}

// ========================================================
// MQTT 业务逻辑
// ========================================================
void MainWindow::onMqttConnectClicked() {         // 点击 MQTT 按钮
    if (m_mqtt->isConnected()) {                  // 如果已经连接
        m_mqtt->disconnectFromBroker();           // 断开连接
        m_mqttPublishTimer->stop();               // 停止定时发布
        m_mqttBtn->setText(QStringLiteral("连接 MQTT")); // 更新按钮文字
        return;                                   // 返回
    }

    // ----------------------------------------------------
    // 教学配置：在这里切换 EMQX 或 OneNET
    // ----------------------------------------------------
    // 选项 1：本地 EMQX 服务器（方便测试）
    MqttConfig config = MqttConfig::makeEMQX("127.0.0.1", 1883); // 生成 EMQX 配置

    // 选项 2：OneNET 云平台（需要替换为真实的 Token）
    /*
    MqttConfig config = MqttConfig::makeOneNET(
        "S7693LN482",           // 你的产品 ID
        "GEC6818",              // 你的设备名称
        "version=2018-10-31&res=...sign=..." // 你的动态 Token
    );
    */

    m_mqtt->setConfig(config);                    // 设置配置

    // 记录下来，发布时要用
    m_pendingTopic = config.postTopic;            // 保存发布主题

    m_mqtt->connectToBroker();                    // 发起连接
    m_mqttBtn->setText(QStringLiteral("正在连接...")); // 更新按钮文字
}

void MainWindow::onMqttConnectedChanged(bool connected) { // MQTT 连接状态改变
    if (connected) {                              // 如果连接成功
        m_mqttPublishTimer->start();              // 启动定时发布
        m_mqttBtn->setText(QStringLiteral("断开 MQTT")); // 更新按钮文字
        appendLog(QStringLiteral("MQTT 已连接，开始定时发布数据")); // 记录日志
    } else {                                      // 如果连接断开
        m_mqttPublishTimer->stop();               // 停止定时发布
        m_mqttBtn->setText(QStringLiteral("连接 MQTT")); // 更新按钮文字
        appendLog(QStringLiteral("MQTT 已断开")); // 记录日志
    }
}

void MainWindow::onMqttPublishTimer() {           // 定时器触发，执行发布
    // 如果没有连接，或者没有任何数据，直接返回
    if (!m_mqtt->isConnected() || m_latestValues.isEmpty()) return; // 保护机制

    // 组装 JSON 数据（兼容 OneNET 物模型格式）
    QJsonObject params;                           // 创建参数对象
    auto add = [&params](const char *key, const SensorValue &v) { // 定义 Lambda 辅助函数
        QJsonObject o;                            // 创建单个传感器对象
        o["value"] = static_cast<double>(v.value); // 设置数值
        params[key] = o;                          // 添加到参数对象
    };
    if (m_latestValues.contains(1)) add("temp", m_latestValues[1]); // 如果有温度数据，添加
    if (m_latestValues.contains(2)) add("volt", m_latestValues[2]); // 如果有电压数据，添加
    if (m_latestValues.contains(3)) add("curr", m_latestValues[3]); // 如果有电流数据，添加

    QJsonObject root;                             // 创建根对象
    root["id"] = QString::number(QDateTime::currentMSecsSinceEpoch()); // 生成唯一的消息 ID（时间戳）
    root["version"] = "1.0";                      // 版本号
    root["params"] = params;                      // 加入参数对象

    const QString payload = QString::fromUtf8(    // 将 JSON 对象转为紧凑字符串
        QJsonDocument(root).toJson(QJsonDocument::Compact));

    // 发布消息
    m_mqtt->publish(m_pendingTopic, payload);     // 通过 MQTT 发送
}