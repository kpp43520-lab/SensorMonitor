#pragma once                                      // 防止头文件重复包含
#include "datasource/IDataSource.h"               // 引入抽象基类接口
#include <QTcpSocket>                             // 引入 Qt 网络套接字模块

class TcpDataSource : public IDataSource {        // 派生类，继承自 IDataSource
    Q_OBJECT                                      // 启用 Qt 元对象系统（支持信号槽）

public:
    explicit TcpDataSource(const QString &host,   // 构造函数：目标服务器IP/域名
                           quint16 port,          // 构造函数：目标服务器端口
                           QObject *parent = nullptr); // 构造函数：父对象指针

    bool start() override;                        // 重写接口：发起 TCP 连接（异步）
    void stop() override;                         // 重写接口：断开 TCP 连接
    QString name() const override;                // 重写接口：返回数据源名称（含IP和端口）
    bool isRunning() const override { return m_running; } // 重写接口：判断是否正在运行

private slots:
    void onConnected();                           // 槽函数：连接建立成功时触发
    void onDisconnected();                        // 槽函数：连接断开时触发
    void onReadyRead();                           // 槽函数：接收到网络数据时触发
    void onErrorOccurred(QAbstractSocket::SocketError err); // 槽函数：网络发生错误时触发

private:
    QTcpSocket *m_socket = nullptr;               // 网络套接字对象指针
    QString     m_host;                           // 目标服务器地址
    quint16     m_port = 0;                       // 目标服务器端口
    bool        m_running = false;                // 运行状态标志位（用于管理连接状态）
};