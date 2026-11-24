#pragma once                                      // 防止头文件重复包含
#include <QWidget>                                // 引入 QWidget，作为曲线控件的基类
#include <QMap>                                   // 引入 QMap，用于映射传感器ID与曲线数据
#include <QVector>                                // 引入 QVector，用于存放曲线的坐标点
#include <QPointF>                                // 引入 QPointF，用于表示浮点数坐标
#include <QColor>                                 // 引入 QColor，用于设置曲线颜色
#include <QDateTime>                              // 引入 QDateTime，用于处理时间戳
#include <QString>                                // 引入 QString，用于存储名称

class RealtimeChart : public QWidget {            // 自绘实时曲线控件，继承自 QWidget
    Q_OBJECT                                      // 启用 Qt 元对象系统（支持信号槽）

public:
    explicit RealtimeChart(QWidget *parent = nullptr); // 构造函数

    void appendValue(quint8 sensorId, double value, const QDateTime &ts); // 向指定传感器追加新的数据点
    void clear();                                 // 清空所有曲线数据

    void setWindowSeconds(int seconds);           // 设置图表显示的时间窗口长度（秒）
    int  windowSeconds() const { return m_windowSeconds; } // 获取当前时间窗口长度

    void setSeriesVisible(quint8 sensorId, bool visible); // 设置指定传感器的曲线是否可见
    bool isSeriesVisible(quint8 sensorId) const;  // 查询指定传感器的曲线是否可见

signals:
    // 新传感器第一次出现时发出，界面据此动态创建复选框
    void seriesAdded(quint8 id, const QString &name, const QColor &color); // 信号：通知界面生成新的勾选框

protected:
    void paintEvent(QPaintEvent *event) override; // 重写绘制事件，在此函数中调用 QPainter 进行画图

private:
    struct Series {                               // 内部结构体：定义单条曲线的属性
        QString name;                             // 曲线名称（如“温度”）
        QColor  color;                            // 曲线颜色
        bool    visible = true;                   // 可见状态（默认显示）
        QVector<QPointF> points;                  // 数据点集合（x = epoch 毫秒，y = 数值）
    };

    static QColor  colorForId(quint8 id);         // 静态函数：根据传感器ID分配固定颜色
    static QString nameForId(quint8 id);          // 静态函数：根据传感器ID获取传感器名称

    QMap<quint8, Series> m_series;                // 存储所有曲线的数据集合（Key为传感器ID）
    int    m_windowSeconds = 30;                  // 图表滚动窗口的时长（默认30秒）
    qint64 m_lastTimestampMs = 0;                 // 记录最后一条数据到达的时间戳（毫秒级）
};