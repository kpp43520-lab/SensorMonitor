#pragma once                                      // 防止头文件重复包含
#include <QString>                                // 引入字符串类，用于返回名称
#include <QDateTime>                              // 引入时间类，用于记录数据时间戳

struct SensorValue {                              // 定义传感器数据结构体
    quint8    id = 0;                             // 传感器ID（1温度, 2电压等）
    float     value = 0.0f;                       // 传感器测量数值
    quint8    unit = 0;                           // 单位代码（1℃, 2V等）
    QDateTime timestamp;                          // 数据接收的时间戳

    QString sensorName() const {                  // 获取传感器名称的辅助函数
        switch (id) {                             // 根据ID匹配名称
        case 1: return QStringLiteral("温度");    // ID为1：温度
        case 2: return QStringLiteral("电压");    // ID为2：电压
        case 3: return QStringLiteral("电流");    // ID为3：电流
        case 4: return QStringLiteral("压力");    // ID为4：压力
        default: return QStringLiteral("传感器%1").arg(id); // 未知ID：默认格式化显示
        }
    }

    QString unitString() const {                  // 获取单位字符串的辅助函数
        switch (unit) {                           // 根据单位代码匹配符号
        case 1: return QStringLiteral("℃");       // 代码1：摄氏度
        case 2: return QStringLiteral("V");       // 代码2：伏特
        case 3: return QStringLiteral("A");       // 代码3：安培
        case 4: return QStringLiteral("kPa");     // 代码4：千帕
        default: return QStringLiteral("?");      // 未知单位：显示问号
        }
    }
};