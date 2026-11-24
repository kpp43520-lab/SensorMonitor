#pragma once                                      // 防止头文件重复包含
#include <QString>                                // 引入 Qt 字符串类

struct MqttConfig {                               // 定义 MQTT 配置结构体
    enum Platform { LocalEMQX, OneNET };          // 定义目标平台枚举：本地 EMQX 或 OneNET 云

    Platform platform = LocalEMQX;                // 当前选择的平台，默认本地 EMQX

    // 连接参数
    QString host;                                 // 服务器地址（IP 或域名）
    quint16 port = 1883;                          // 服务器端口，默认 1883
    QString clientId;                             // 客户端 ID（设备唯一标识）
    QString username;                             // 用户名
    QString password;                             // 密码（OneNET 填动态 Token）

    // OneNET 专用（EMQX 可留空）
    QString productId;                            // OneNET 产品 ID
    QString deviceName;                           // OneNET 设备名称

    // 主题
    QString postTopic;                            // 发布主题（用于上报数据）
    QString replyTopic;                           // 订阅主题（用于接收云端回复）

    // 构建预置配置的静态函数
    static MqttConfig makeEMQX(const QString &host = "127.0.0.1", // 静态方法：生成 EMQX 默认配置（默认本地 IP）
                               quint16 port = 1883) {             // 默认端口 1883
        MqttConfig c;                             // 创建配置对象
        c.platform = LocalEMQX;                   // 设置平台为本地 EMQX
        c.host = host;                            // 设置服务器地址
        c.port = port;                            // 设置服务器端口
        c.clientId = "sensor_monitor_qt";         // 设置默认客户端 ID
        c.username = "";                          // 本地测试不需要用户名
        c.password = "";                          // 本地测试不需要密码
        c.postTopic = "sensor/data";              // 设置默认发布主题
        c.replyTopic = "sensor/reply";            // 设置默认订阅主题
        return c;                                 // 返回配置对象
    }

    static MqttConfig makeOneNET(const QString &productId,        // 静态方法：生成 OneNET 配置（传入产品 ID）
                                 const QString &deviceName,       // 传入设备名称
                                 const QString &accessKey) {      // 传入鉴权 Token
        MqttConfig c;                             // 创建配置对象
        c.platform = OneNET;                      // 设置平台为 OneNET
        c.host = "mqtts.heclouds.com";            // 设置 OneNET 官方 MQTT 服务器地址
        c.port = 1883;                            // 设置非加密端口 1883
        c.clientId = deviceName;                  // OneNET 规定 ClientID 填设备名称
        c.username = productId;                   // OneNET 规定 Username 填产品 ID
        c.password = accessKey;                   // 密码填动态生成的 Token
        c.productId = productId;                  // 保存产品 ID 备用
        c.deviceName = deviceName;                // 保存设备名称备用
        c.postTopic = QStringLiteral(             // 动态拼接 OneNET 物模型属性上报主题
                          "$sys/%1/%2/thing/property/post").arg(productId, deviceName);
        c.replyTopic = QStringLiteral(            // 动态拼接 OneNET 属性上报回复主题
                           "$sys/%1/%2/thing/property/post/reply").arg(productId, deviceName);
        return c;                                 // 返回配置对象
    }
};