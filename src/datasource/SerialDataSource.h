#pragma once                                      // 防止头文件重复包含
#include "datasource/IDataSource.h"               // 引入抽象基类接口
#include <QSerialPort>                            // 引入 Qt 串口模块

class SerialDataSource : public IDataSource {     // 派生类，继承自 IDataSource
    Q_OBJECT                                      // 启用 Qt 元对象系统（支持信号槽）

public:
    explicit SerialDataSource(const QString &portName, // 构造函数：传入串口名
                              qint32 baudRate,         // 构造函数：传入波特率
                              QObject *parent = nullptr); // 构造函数：传入父对象

    bool start() override;                        // 重写接口：打开串口
    void stop() override;                         // 重写接口：关闭串口
    QString name() const override;                // 重写接口：返回数据源名称（含串口参数）
    bool isRunning() const override { return m_serial->isOpen(); } // 重写接口：判断是否运行中（检查串口是否打开）

private slots:
    void onReadyRead();                           // 槽函数：串口收到数据时触发
    void onErrorOccurred(QSerialPort::SerialPortError err); // 槽函数：串口发生异常时触发

private:
    QSerialPort *m_serial = nullptr;              // 串口通信对象指针
    QString      m_portName;                      // 目标串口名称（如 COM1）
    qint32       m_baudRate = 115200;             // 目标波特率，默认 115200
};