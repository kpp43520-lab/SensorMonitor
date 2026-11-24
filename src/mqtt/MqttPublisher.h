#pragma once                                      // 防止头文件重复包含
#include <QObject>                                // 引入 QObject，支持信号槽
#include <QMqttClient>                            // 引入 Qt 官方 MQTT 客户端模块
#include "mqtt/MqttConfig.h"                      // 引入 MQTT 配置结构体

class MqttPublisher : public QObject {            // MQTT 发布器类，继承自 QObject
    Q_OBJECT                                      // 启用 Qt 元对象系统（支持信号槽）

public:
    explicit MqttPublisher(QObject *parent = nullptr); // 构造函数，初始化父对象

    void setConfig(const MqttConfig &config);     // 设置连接配置（服务器、主题、密码等）
    void connectToBroker();                       // 发起连接（异步）
    void disconnectFromBroker();                  // 主动断开连接
    void publish(const QString &topic, const QString &payload); // 发布消息（指定主题和内容）

    bool isConnected() const;                     // 查询当前是否已成功连接到服务器

signals:
    void connectedChanged(bool connected);        // 信号：连接状态发生变化
    void logMessage(const QString &msg);          // 信号：产生日志信息（供主窗口显示）
    void replyReceived(const QString &topic, const QString &payload); // 信号：收到云端下发的消息

private slots:
    void onConnected();                           // 槽函数：连接成功时触发
    void onDisconnected();                        // 槽函数：连接断开时触发
    void onErrorChanged(QMqttClient::ClientError error); // 槽函数：发生错误时触发
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic); // 槽函数：收到订阅的消息时触发

private:
    QMqttClient *m_client = nullptr;              // 内部封装的 Qt MQTT 客户端指针
    MqttConfig   m_config;                        // 保存当前的 MQTT 配置信息
};