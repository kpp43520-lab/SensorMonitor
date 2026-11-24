#pragma once                                      // 防止头文件重复包含
#include <QObject>                                // 引入 QObject，支持信号槽
#include <QByteArray>                             // 引入字节数组，用于处理原始数据
#include <QVector>                                // 引入动态数组容器，用于存放解析结果
#include "core/SensorValue.h"                     // 引入传感器数据结构体

class ProtocolParser : public QObject {           // 协议解析器类，继承自 QObject
    Q_OBJECT                                      // 启用 Qt 元对象系统（支持信号槽）

public:
    explicit ProtocolParser(QObject *parent = nullptr); // 构造函数，初始化父对象

    void append(const QByteArray &bytes);         // 喂入新数据，追加到内部缓冲区
    void reset();                                 // 重置解析器，清空缓冲区与计数器

    quint64 frameCount() const { return m_frameCount; } // 获取已解析的完整帧数量
    quint64 errorCount() const { return m_errorCount; } // 获取解析失败的累计次数

signals:
    void frameParsed(const QVector<SensorValue> &values); // 信号：成功解析出一帧数据
    void rawFrame(const QByteArray &frame);               // 信号：产生完整的一帧原始报文
    void parseError(const QString &reason);               // 信号：解析发生错误

private:
    bool tryParseOne(QByteArray &outFrame);       // 内部函数：尝试从缓冲区中提取出一帧完整数据

    QByteArray m_buffer;                          // 接收缓冲区，用于累积半包数据
    quint64 m_frameCount = 0;                     // 记录解析成功的总帧数
    quint64 m_errorCount = 0;                     // 记录解析失败/校验错误的总次数
};