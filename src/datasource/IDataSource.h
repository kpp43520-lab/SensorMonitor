#pragma once                             // 头文件防重复包含
#include <QObject>                       // 引入QObject，支持信号槽
#include <QByteArray>                    // 引入字节数组，用于传原始数据
#include <QString>                       // 引入字符串，用于名称和报错信息

class IDataSource : public QObject {     // 抽象基类，继承QObject
    Q_OBJECT                             // 启用Qt元对象系统（信号槽必须）

public:
    explicit IDataSource(QObject *parent = nullptr) : QObject(parent) {} // 构造函数
    virtual ~IDataSource() = default;    // 虚析构函数，安全释放派生类

    virtual bool start() = 0;            // 纯虚函数：启动数据源
    virtual void stop() = 0;             // 纯虚函数：停止数据源
    virtual QString name() const = 0;    // 纯虚函数：返回数据源名称
    virtual bool isRunning() const = 0;  // 纯虚函数：返回运行状态

signals:
    void dataArrived(const QByteArray &rawBytes);   // 信号：收到新数据
    void errorOccurred(const QString &message);     // 信号：发生错误
    void stateChanged(bool running);                // 信号：状态变化
};