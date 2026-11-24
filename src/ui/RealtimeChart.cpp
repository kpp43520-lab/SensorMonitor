#include "ui/RealtimeChart.h"                     // 引入实时曲线头文件

#include <QPainter>                               // 引入 QPainter，用于绘图
#include <QPainterPath>                           // 引入 QPainterPath，用于绘制复杂路径（曲线）
#include <QPaintEvent>                            // 引入绘制事件类
#include <QFontMetrics>                           // 引入字体度量类，用于计算文本宽度
#include <limits>                                 // 引入数值极值库，用于初始化 Y 轴范围

namespace {                                       // 匿名命名空间，限定常量作用域
constexpr int kMarginLeft   = 62;                 // 左侧留白（用于显示 Y 轴刻度）
constexpr int kMarginRight  = 20;                 // 右侧留白
constexpr int kMarginTop    = 20;                 // 顶部留白
constexpr int kMarginBottom = 40;                 // 底部留白（用于显示 X 轴时间）
constexpr int kMaxPointsPerSeries = 3000;         // 单条曲线最大数据点数（防止内存无限增长）
}

RealtimeChart::RealtimeChart(QWidget *parent) : QWidget(parent) { // 构造函数
    setMinimumHeight(240);                        // 设置控件最小高度
    setAutoFillBackground(true);                  // 允许自动填充背景
    QPalette pal = palette();                     // 获取当前调色板
    pal.setColor(QPalette::Window, Qt::white);    // 将窗口背景色设为白色
    setPalette(pal);                              // 应用修改后的调色板
}

void RealtimeChart::setWindowSeconds(int seconds) { // 设置显示的时间窗口长度（秒）
    m_windowSeconds = qMax(5, seconds);           // 最小限制为 5 秒
    update();                                     // 触发重绘
}

void RealtimeChart::clear() {                     // 清空所有数据
    m_series.clear();                             // 清空曲线映射表
    m_lastTimestampMs = 0;                        // 重置最后时间戳
    update();                                     // 触发重绘
}

QColor RealtimeChart::colorForId(quint8 id) {     // 静态函数：根据 ID 分配颜色
    switch (id) {                                 // 根据传入的 ID 匹配
    case 1: return QColor(0xE5, 0x39, 0x35);      // 温度：红
    case 2: return QColor(0x1E, 0x88, 0xE5);      // 电压：蓝
    case 3: return QColor(0x43, 0xA0, 0x47);      // 电流：绿
    case 4: return QColor(0x8E, 0x24, 0xAA);      // 压力：紫
    default: return QColor(0x75, 0x75, 0x75);     // 默认：灰色
    }
}

QString RealtimeChart::nameForId(quint8 id) {     // 静态函数：根据 ID 分配名称
    switch (id) {                                 // 根据传入的 ID 匹配
    case 1: return QStringLiteral("温度");         // ID 1 为温度
    case 2: return QStringLiteral("电压");         // ID 2 为电压
    case 3: return QStringLiteral("电流");         // ID 3 为电流
    case 4: return QStringLiteral("压力");         // ID 4 为压力
    default: return QStringLiteral("传感器%1").arg(id); // 默认格式
    }
}

void RealtimeChart::setSeriesVisible(quint8 sensorId, bool visible) { // 设置指定曲线的可见性
    auto it = m_series.find(sensorId);            // 查找对应的曲线数据
    if (it == m_series.end()) return;             // 如果未找到，直接返回
    if (it->visible == visible) return;           // 如果状态未改变，直接返回
    it->visible = visible;                        // 更新可见状态
    update();                                     // 触发重绘
}

bool RealtimeChart::isSeriesVisible(quint8 sensorId) const { // 获取指定曲线的可见性
    auto it = m_series.constFind(sensorId);       // 常量查找曲线数据
    return it != m_series.constEnd() && it->visible; // 返回是否找到且可见
}

void RealtimeChart::appendValue(quint8 sensorId, double value, const QDateTime &ts) { // 添加新数据点
    const qint64 t = ts.isValid() ? ts.toMSecsSinceEpoch() // 如果时间有效，转为毫秒时间戳
                                  : QDateTime::currentMSecsSinceEpoch(); // 否则使用当前时间
    m_lastTimestampMs = qMax(m_lastTimestampMs, t); // 更新最新的全局时间戳

    auto it = m_series.find(sensorId);            // 查找该传感器 ID 对应的曲线
    if (it == m_series.end()) {                   // 如果是第一次遇到该传感器
        Series s;                                 // 创建新的曲线数据结构
        s.name  = nameForId(sensorId);            // 设置名称
        s.color = colorForId(sensorId);           // 设置颜色
        it = m_series.insert(sensorId, s);        // 插入到映射表
        emit seriesAdded(sensorId, s.name, s.color); // 发出信号通知界面生成复选框
    }
    it->points.append(QPointF(static_cast<qreal>(t), value)); // 将新点（时间，数值）加入队列

    const qint64 keepFrom = m_lastTimestampMs - (m_windowSeconds + 5) * 1000LL; // 计算需要保留的起始时间（多留5秒余量）
    auto &pts = it->points;                       // 获取点集引用
    int drop = 0;                                 // 记录需要丢弃的点数
    while (drop < pts.size() && pts[drop].x() < keepFrom) // 寻找窗口外的老数据
        ++drop;                                   // 增加丢弃计数
    if (drop > 0)                                 // 如果需要丢弃
        pts.remove(0, drop);                      // 从头部移除这些点
    if (pts.size() > kMaxPointsPerSeries)         // 如果点数超过硬限制
        pts.remove(0, pts.size() - kMaxPointsPerSeries); // 强制移除多余的点

    update();                                     // 触发重绘
}

void RealtimeChart::paintEvent(QPaintEvent *) {   // 绘制事件处理函数
    QPainter p(this);                             // 创建画笔对象，指定绘制目标为当前控件
    p.setRenderHint(QPainter::Antialiasing, true); // 开启抗锯齿，使曲线平滑

    const QRectF plotRect(kMarginLeft,            // 计算实际绘图区域
                          kMarginTop,
                          width()  - kMarginLeft - kMarginRight,
                          height() - kMarginTop  - kMarginBottom);
    if (plotRect.width() <= 10 || plotRect.height() <= 10) // 如果控件太小
        return;                                   // 直接返回不画

    const qint64 tEnd   = m_lastTimestampMs;      // 计算 X 轴结束时间（当前最新时间）
    const qint64 tStart = tEnd - qint64(m_windowSeconds) * 1000LL; // 计算 X 轴起始时间

    // 只统计可见曲线的 Y 范围
    bool anyVisible = false;                      // 标记是否有可见曲线
    bool hasData = false;                         // 标记在可见范围内是否有数据
    double yMin = std::numeric_limits<double>::max(); // 初始化 Y 轴最小值（极大值）
    double yMax = std::numeric_limits<double>::lowest(); // 初始化 Y 轴最大值（极小值）
    for (auto it = m_series.constBegin(); it != m_series.constEnd(); ++it) { // 遍历所有曲线
        const Series &s = it.value();             // 获取曲线引用
        if (!s.visible) continue;                 // 跳过不可见的曲线
        anyVisible = true;                        // 标记有可见曲线
        for (const QPointF &pt : s.points) {      // 遍历曲线上的点
            if (pt.x() >= tStart && pt.x() <= tEnd) { // 如果点在当前时间窗口内
                yMin = qMin(yMin, pt.y());        // 更新最小值
                yMax = qMax(yMax, pt.y());        // 更新最大值
                hasData = true;                   // 标记有数据
            }
        }
    }
    if (!hasData) { yMin = 0; yMax = 1; }         // 如果没有数据，给个默认范围 0-1
    if (yMax - yMin < 1e-9) { yMin -= 1; yMax += 1; } // 如果所有点数值一样，上下扩展范围避免除0
    const double pad = (yMax - yMin) * 0.1;       // 计算 Y 轴上下留白（10%）
    yMin -= pad; yMax += pad;                     // 应用留白，让曲线不贴边

    // 背景
    p.fillRect(rect(), Qt::white);                // 整个控件背景填充白色
    p.fillRect(plotRect, QColor(0xFA, 0xFA, 0xFA)); // 绘图区背景填充浅灰色

    // 网格与刻度
    const int xDiv = 6;                           // X 轴分 6 格
    const int yDiv = 5;                           // Y 轴分 5 格
    QFont axisFont = font();                      // 获取当前字体
    axisFont.setPointSizeF(axisFont.pointSizeF() * 0.85); // 缩小字号作为坐标轴字体
    p.setFont(axisFont);                          // 应用字体

    for (int i = 0; i <= xDiv; ++i) {             // 遍历 X 轴刻度线
        const double ratio = double(i) / xDiv;    // 计算当前比例（0.0 到 1.0）
        const double x = plotRect.left() + ratio * plotRect.width(); // 计算像素 X 坐标
        p.setPen(QPen(QColor(0xE0, 0xE0, 0xE0), 1)); // 设置网格线颜色（浅灰色）
        p.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom())); // 绘制垂直网格线

        if (hasData) {                            // 如果有数据，才绘制时间标签
            const qint64 t = tStart + qint64(ratio * (tEnd - tStart)); // 计算该刻度代表的真实时间
            const qint64 agoSec = (tEnd - t) / 1000; // 计算距离现在多少秒前
            const QString label = (i == xDiv) ? QStringLiteral("now") // 最右侧显示 "now"
                                              : QStringLiteral("-%1s").arg(agoSec); // 其他显示 "-Ns"
            p.setPen(QColor(0x80, 0x80, 0x80));   // 设置文字颜色
            p.drawText(QRectF(x - 30, plotRect.bottom() + 4, 60, 16), // 在网格线下方绘制文字
                       Qt::AlignHCenter | Qt::AlignTop, label); // 居中对齐
        }
    }

    for (int i = 0; i <= yDiv; ++i) {             // 遍历 Y 轴刻度线
        const double ratio = double(i) / yDiv;    // 计算当前比例
        const double y = plotRect.bottom() - ratio * plotRect.height(); // 计算像素 Y 坐标
        p.setPen(QPen(QColor(0xE0, 0xE0, 0xE0), 1)); // 设置网格线颜色
        p.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y)); // 绘制水平网格线

        const double v = yMin + ratio * (yMax - yMin); // 计算该刻度代表的真实数值
        p.setPen(QColor(0x80, 0x80, 0x80));       // 设置文字颜色
        p.drawText(QRectF(0, y - 8, kMarginLeft - 6, 16), // 在网格线左侧绘制文字
                   Qt::AlignRight | Qt::AlignVCenter, // 右对齐
                   QString::number(v, 'f', (yMax - yMin) < 10 ? 2 : 1)); // 格式化数值（范围小保留2位小数，否则1位）
    }

    p.setPen(QPen(QColor(0xB0, 0xB0, 0xB0), 1));  // 设置边框颜色
    p.drawRect(plotRect);                         // 绘制绘图区外边框

    // 曲线：只画可见的
    p.setClipRect(plotRect);                      // 设置裁剪区域，防止曲线画出绘图区
    for (auto it = m_series.constBegin(); it != m_series.constEnd(); ++it) { // 遍历所有曲线
        const Series &s = it.value();             // 获取曲线数据
        if (!s.visible) continue;                 // 跳过不可见曲线
        QPainterPath path;                        // 创建绘制路径
        bool started = false;                     // 标记是否已经开始绘制
        for (const QPointF &pt : s.points) {      // 遍历数据点
            if (pt.x() < tStart) continue;        // 忽略时间窗口左侧的点
            if (pt.x() > tEnd)   break;           // 遇到窗口右侧的点，结束当前曲线绘制
            const double x = plotRect.left()      // 将真实时间映射为像素 X 坐标
                             + (pt.x() - tStart) / double(tEnd - tStart) * plotRect.width();
            const double y = plotRect.bottom()    // 将真实数值映射为像素 Y 坐标
                             - (pt.y() - yMin) / (yMax - yMin) * plotRect.height();
            if (!started) { path.moveTo(x, y); started = true; } // 如果是第一个点，移动画笔
            else          { path.lineTo(x, y); }                 // 后续点直接连线
        }
        if (started) {                            // 如果路径有效
            p.setPen(QPen(s.color, 2));           // 设置画笔颜色和粗细（2像素）
            p.setBrush(Qt::NoBrush);              // 不填充内部
            p.drawPath(path);                     // 绘制曲线路径
        }
    }
    p.setClipping(false);                         // 取消裁剪，允许后续绘制在任意位置

    // 图例：只画可见的
    int lx = kMarginLeft + 8;                     // 图例起始 X 坐标
    const int ly = kMarginTop + 4;                // 图例 Y 坐标
    QFont legendFont = font();                    // 获取字体
    legendFont.setPointSizeF(legendFont.pointSizeF() * 0.9); // 缩小字号作为图例字体
    p.setFont(legendFont);                        // 应用字体
    for (auto it = m_series.constBegin(); it != m_series.constEnd(); ++it) { // 遍历曲线
        const Series &s = it.value();             // 获取曲线数据
        if (!s.visible) continue;                 // 跳过隐藏的曲线
        p.setPen(Qt::NoPen);                      // 无边框
        p.setBrush(s.color);                      // 填充颜色为曲线颜色
        p.drawRect(QRectF(lx, ly + 4, 12, 12));   // 绘制颜色小方块

        p.setPen(QColor(0x40, 0x40, 0x40));       // 设置文字颜色
        const int tw = p.fontMetrics().horizontalAdvance(s.name); // 计算文字宽度
        p.drawText(QPointF(lx + 16, ly + 14), s.name); // 绘制曲线名称
        lx += 16 + tw + 18;                       // 更新下一个图例的 X 坐标
    }

    // 提示文字
    if (!anyVisible || !hasData) {                // 如果所有曲线都隐藏了，或者根本没有数据
        QFont tipFont = font();                   // 获取字体
        tipFont.setPointSizeF(tipFont.pointSizeF() * 1.2); // 放大提示字号
        p.setFont(tipFont);                       // 应用字体
        p.setPen(QColor(0xA0, 0xA0, 0xA0));       // 设置提示文字颜色（灰色）
        const QString tip = !anyVisible           // 判断提示内容
                                ? QStringLiteral("所有曲线已隐藏，请勾选上方复选框") // 全部隐藏
                                : QStringLiteral("暂无数据，点击“启动”开始"); // 暂无数据
        p.drawText(plotRect, Qt::AlignCenter, tip); // 在绘图区居中绘制提示文字
    }
}