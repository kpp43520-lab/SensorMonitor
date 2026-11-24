#pragma once                                      // 防止头文件重复包含
#include <QWidget>                                // 引入 QWidget，作为面板的基类

class RecordService;                              // 前置声明：数据记录服务类
class QDateTimeEdit;                              // 前置声明：时间选择控件
class QComboBox;                                  // 前置声明：下拉框控件
class QTableView;                                 // 前置声明：表格视图控件
class QSqlQueryModel;                             // 前置声明：SQL 查询模型
class QLabel;                                     // 前置声明：文本标签控件

class HistoryPanel : public QWidget {             // 历史查询面板类，继承自 QWidget
    Q_OBJECT                                      // 启用 Qt 元对象系统（支持信号槽）

public:
    explicit HistoryPanel(RecordService *service, QWidget *parent = nullptr); // 构造函数，注入数据库服务

public slots:
    void refreshSensorList();                     // 槽函数：刷新传感器下拉列表（从数据库重新读取）
    void runQuery();                              // 槽函数：执行历史数据查询并更新表格

private slots:
    void onResetClicked();                        // 槽函数：重置时间范围（恢复为最近1小时）
    void onExportClicked();                       // 槽函数：将当前查询结果导出为 CSV 文件

private:
    RecordService   *m_service = nullptr;         // 数据记录服务指针，用于获取数据库连接
    QDateTimeEdit   *m_fromEdit = nullptr;        // 起始时间选择控件
    QDateTimeEdit   *m_toEdit = nullptr;          // 结束时间选择控件
    QComboBox       *m_sensorCombo = nullptr;     // 传感器筛选下拉框（支持按 ID 过滤）
    QTableView      *m_tableView = nullptr;       // 数据显示表格视图
    QSqlQueryModel  *m_model = nullptr;           // 数据模型，绑定 SQL 查询结果到表格
    QLabel          *m_countLabel = nullptr;      // 状态标签，显示查询结果条数或错误信息
};