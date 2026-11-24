#pragma once                                      // 防止头文件重复包含
#include "datasource/IDataSource.h"               // 引入抽象基类接口
#include <QTimer>                                 // 引入定时器，用于定时产生数据
#include <QVector>                                // 引入动态数组容器，用于存放多路数据
#include "core/SensorValue.h"                     // 引入传感器数据结构体

class MockDataSource : public IDataSource {       // 派生类，继承自 IDataSource
    Q_OBJECT                                      // 启用 Qt 元对象系统（支持信号槽）

public:
    explicit MockDataSource(QObject *parent = nullptr); // 构造函数，初始化父对象

    bool start() override;                        // 重写接口：启动数据源
    void stop() override;                         // 重写接口：停止数据源
    QString name() const override { return QStringLiteral("Mock 模拟数据源"); } // 重写接口：返回数据源名称
    bool isRunning() const override { return m_timer->isActive(); }             // 重写接口：判断是否运行中（检查定时器状态）

private slots:
    void onTick();                                // 槽函数：定时器超时触发，生成并发送数据

private:
    QByteArray buildFrame(const QVector<SensorValue> &items); // 辅助函数：将数据组装成符合协议的字节流
    void emitSliced(const QByteArray &data);                  // 辅助函数：随机切片发送，模拟半包/粘包现象

    QTimer *m_timer = nullptr;                    // 内部定时器，控制数据产生频率
    double m_phase = 0.0;                         // 相位变量，用于生成正弦波形数据
    quint64 m_tickCount = 0;                      // 计数值，用于统计次数和定期注入错误帧
};