#pragma once                                      // 防止头文件重复包含

#include <QMainWindow>                            // 引入 QMainWindow，作为主窗口基类
#include <QMap>                                   // 引入 QMap，用于建立各种映射关系
#include <QColor>                                 // 引入 QColor，用于处理颜色数据
#include "core/ProtocolParser.h"                  // 引入协议解析器（这里包含了 SensorValue 结构体）

// ================= 前置声明 =================
// Qt 控件类（减少头文件包含，加快编译速度）
class QComboBox;                                  // 下拉框
class QPushButton;                                // 按钮
class QLabel;                                     // 文本标签
class QTextEdit;                                  // 多行文本框
class QTableWidget;                               // 表格控件
class QWidget;                                    // 基础窗口部件
class QLineEdit;                                  // 单行输入框
class QCheckBox;                                  // 复选框
class QHBoxLayout;                                // 水平布局
class QTimer;                                     // 定时器

// 自定义业务类
class IDataSource;                                // 数据源接口
class RealtimeChart;                              // 实时曲线控件
class RecordService;                              // 数据库记录服务
class HistoryPanel;                               // 历史查询面板
class MqttPublisher;                              // MQTT 发布器
struct MqttConfig;                                // MQTT 配置结构体

// ================= 主窗口类 =================
class MainWindow : public QMainWindow             // 主窗口类，继承自 QMainWindow
{
    Q_OBJECT                                      // 启用 Qt 元对象系统（支持信号槽）

public:
    explicit MainWindow(QWidget *parent = nullptr); // 构造函数
    ~MainWindow() override = default;             // 析构函数，使用默认实现

private slots:
    // --- 数据源与解析相关槽函数 ---
    void onSourceTypeChanged(int index);          // 槽：下拉框切换数据源类型
    void onRefreshPorts();                        // 槽：刷新可用串口列表
    void onStartClicked();                        // 槽：点击启动按钮
    void onStopClicked();                         // 槽：点击停止按钮
    void onDataArrived(const QByteArray &bytes);  // 槽：收到原始字节流
    void onRawFrame(const QByteArray &frame);     // 槽：成功解析出一整帧
    void onFrameParsed(const QVector<SensorValue> &values); // 槽：解析出具体的传感器数据
    void onParseError(const QString &reason);     // 槽：解析发生错误

    // --- 界面与曲线相关槽函数 ---
    void onClearChart();                          // 槽：点击清空曲线按钮
    void onSeriesAdded(quint8 id, const QString &name, const QColor &color); // 槽：新增曲线时动态生成复选框

    // --- MQTT 相关槽函数 ---
    void onMqttConnectClicked();                  // 槽：点击连接/断开 MQTT 按钮
    void onMqttPublishTimer();                    // 槽：定时触发 MQTT 数据上报
    void onMqttConnectedChanged(bool connected);  // 槽：响应 MQTT 连接状态变化

private:
    // --- 内部辅助函数 ---
    void setupUi();                               // 初始化并搭建所有 UI 界面
    void createSource();                          // 根据当前选择创建对应的数据源实例
    void refreshPortList();                       // 扫描系统硬件并更新串口下拉框
    void updateSensorRow(const SensorValue &v);   // 更新表格中指定传感器的数据行
    void appendLog(const QString &text);          // 在日志窗口中追加一条信息
    void updateStats();                           // 更新顶部状态栏的帧数和错误数
    void clearSeriesChecks();                     // 清理曲线复选框的 UI 和内存

private:
    // ================= UI 控件 =================
    // 顶栏数据源配置
    QComboBox   *m_sourceCombo = nullptr;         // 数据源类型下拉框（Mock/Serial/TCP）
    QPushButton *m_startBtn = nullptr;            // 启动采集按钮
    QPushButton *m_stopBtn = nullptr;             // 停止采集按钮
    QLabel      *m_stateLabel = nullptr;          // 显示当前运行状态的标签
    QLabel      *m_statsLabel = nullptr;          // 显示帧数和错误数的标签

    // 串口配置区
    QWidget     *m_serialConfigWidget = nullptr;  // 串口配置容器（整体显示/隐藏）
    QComboBox   *m_portCombo = nullptr;           // 串口端口下拉框
    QComboBox   *m_baudCombo = nullptr;           // 波特率下拉框
    QPushButton *m_refreshPortBtn = nullptr;      // 刷新串口按钮

    // TCP 配置区
    QWidget     *m_tcpConfigWidget = nullptr;     // TCP 配置容器（整体显示/隐藏）
    QLineEdit   *m_hostEdit = nullptr;            // 目标服务器 IP 输入框
    QLineEdit   *m_portEdit = nullptr;            // 目标服务器端口输入框

    // MQTT 配置区
    QPushButton *m_mqttBtn = nullptr;             // 连接/断开 MQTT 按钮

    // 核心显示区
    QPushButton   *m_clearChartBtn = nullptr;     // 清空曲线按钮
    RealtimeChart *m_chart = nullptr;             // 自绘实时曲线控件指针
    QHBoxLayout   *m_legendLayout = nullptr;      // 存放曲线复选框的布局
    QMap<quint8, QCheckBox*> m_seriesChecks;      // 映射表：传感器 ID -> 对应的复选框指针

    // 数据监控区
    QTextEdit    *m_rawView = nullptr;            // 显示原始 Hex 报文的文本框
    QTableWidget *m_table = nullptr;              // 显示解析后数值的表格
    QTextEdit    *m_logView = nullptr;            // 显示运行日志的文本框

    // ================= 业务对象 =================
    IDataSource    *m_source = nullptr;           // 当前正在使用的数据源指针（多态）
    ProtocolParser *m_parser = nullptr;           // 协议解析器实例
    RecordService  *m_record = nullptr;           // SQLite 数据库记录服务实例
    HistoryPanel   *m_historyPanel = nullptr;     // 历史查询界面实例

    // MQTT 业务对象
    MqttPublisher  *m_mqtt = nullptr;             // MQTT 发布器实例
    QTimer         *m_mqttPublishTimer = nullptr; // MQTT 定时发布定时器
    QMap<quint8, SensorValue> m_latestValues;     // 缓存各传感器的最新值，供定时上报使用
    QString m_pendingTopic;                       // 记录当前配置的 MQTT 发布主题
    QMap<quint8, int> m_rowBySensorId;            // 映射表：传感器 ID -> 表格中的行号
};