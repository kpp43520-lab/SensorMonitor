#include "mqtt/MqttPublisher.h"                   // 引入 MQTT 发布器头文件
#include <QDebug>                                 // 引入调试输出模块

MqttPublisher::MqttPublisher(QObject *parent) : QObject(parent) { // 构造函数
    m_client = new QMqttClient(this);             // 创建 Qt 原生 MQTT 客户端对象

    connect(m_client, &QMqttClient::connected,    // 监听客户端连接成功信号
            this, &MqttPublisher::onConnected);   // 绑定到对应处理槽
    connect(m_client, &QMqttClient::disconnected, // 监听客户端断开连接信号
            this, &MqttPublisher::onDisconnected); // 绑定到对应处理槽
    connect(m_client, &QMqttClient::errorChanged, // 监听客户端错误改变信号
            this, &MqttPublisher::onErrorChanged); // 绑定到对应处理槽
    connect(m_client, &QMqttClient::messageReceived, // 监听收到服务器下发消息信号
            this, &MqttPublisher::onMessageReceived); // 绑定到对应处理槽
}

void MqttPublisher::setConfig(const MqttConfig &config) { // 设置 MQTT 连接配置
    m_config = config;                            // 保存传入的配置结构体
}

void MqttPublisher::connectToBroker() {           // 发起连接服务器请求
    if (m_client->state() != QMqttClient::Disconnected) { // 如果当前不是未连接状态
        m_client->disconnectFromHost();           // 先主动断开之前的连接
    }

    m_client->setHostname(m_config.host);         // 设置服务器地址
    m_client->setPort(m_config.port);             // 设置服务器端口
    m_client->setClientId(m_config.clientId);     // 设置客户端 ID
    m_client->setUsername(m_config.username);     // 设置用户名
    m_client->setPassword(m_config.password);     // 设置密码
    m_client->setProtocolVersion(QMqttClient::MQTT_3_1_1); // 设置协议版本为 3.1.1
    m_client->setKeepAlive(60);                   // 设置心跳保持时间为 60 秒

    emit logMessage(QStringLiteral("正在连接 MQTT Broker %1:%2 ...") // 发送日志信号
                        .arg(m_config.host).arg(m_config.port)); // 附带目标地址和端口
    m_client->connectToHost();                    // 执行异步连接
}

void MqttPublisher::disconnectFromBroker() {      // 主动断开连接
    m_client->disconnectFromHost();               // 调用底层断开接口
}

void MqttPublisher::publish(const QString &topic, const QString &payload) { // 发布消息
    if (!isConnected()) {                         // 如果尚未连接
        emit logMessage(QStringLiteral("MQTT 未连接，发布被忽略")); // 记录日志并忽略
        return;                                   // 直接返回
    }
    const QByteArray data = payload.toUtf8();     // 将字符串内容转为 UTF-8 字节流
    if (!m_client->publish(QMqttTopicName(topic), data, 1)) {  // 执行发布（指定 QoS 1）
        emit logMessage(QStringLiteral("发布失败：%1").arg(topic)); // 如果失败，记录错误日志
    }
}

bool MqttPublisher::isConnected() const {         // 获取当前连接状态
    return m_client->state() == QMqttClient::Connected; // 返回是否处于已连接状态
}

void MqttPublisher::onConnected() {               // 槽函数：连接成功
    emit connectedChanged(true);                  // 发出连接状态改变信号
    emit logMessage(QStringLiteral("MQTT 已连接，开始订阅回复主题")); // 记录连接成功日志

    // 订阅回复主题（OneNET 需要，EMQX 可省略）
    if (!m_config.replyTopic.isEmpty()) {         // 如果配置了订阅主题
        m_client->subscribe(QMqttTopicFilter(m_config.replyTopic), 1); // 订阅该主题（QoS 1）
    }
}

void MqttPublisher::onDisconnected() {            // 槽函数：连接断开
    emit connectedChanged(false);                 // 发出连接状态改变信号
    emit logMessage(QStringLiteral("MQTT 连接断开")); // 记录断开日志
}

void MqttPublisher::onErrorChanged(QMqttClient::ClientError error) { // 槽函数：错误改变
    if (error != QMqttClient::NoError) {          // 如果发生的不是“无错误”
        emit logMessage(QStringLiteral("MQTT 错误码：%1").arg(static_cast<int>(error))); // 记录错误码
    }
}

void MqttPublisher::onMessageReceived(const QByteArray &message, // 槽函数：收到订阅消息
                                      const QMqttTopicName &topic) { // 传入消息内容与主题
    const QString payload = QString::fromUtf8(message); // 将收到的字节流转为字符串
    emit logMessage(QStringLiteral("收到 [%1]：%2").arg(topic.name(), payload)); // 记录收到日志
    emit replyReceived(topic.name(), payload);    // 向上层应用发送收到回复的信号
}