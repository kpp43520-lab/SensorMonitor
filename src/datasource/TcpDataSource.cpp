#include "datasource/TcpDataSource.h"             // 引入 TCP 数据源头文件

TcpDataSource::TcpDataSource(const QString &host, quint16 port, QObject *parent) // 构造函数，传入服务器地址和端口
    : IDataSource(parent),                        // 初始化列表：调用基类构造
    m_socket(new QTcpSocket(this)),               // 初始化列表：创建网络套接字并指定父对象
    m_host(host),                                 // 初始化列表：保存目标主机地址
    m_port(port) {                                // 初始化列表：保存目标端口号
    connect(m_socket, &QTcpSocket::connected,     // 当 TCP 连接成功建立时
            this, &TcpDataSource::onConnected);   // 触发连接成功的槽函数
    connect(m_socket, &QTcpSocket::disconnected,  // 当 TCP 连接断开时
            this, &TcpDataSource::onDisconnected); // 触发连接断开的槽函数
    connect(m_socket, &QTcpSocket::readyRead,     // 当接收到新数据时
            this, &TcpDataSource::onReadyRead);   // 触发读取数据的槽函数
    connect(m_socket, &QTcpSocket::errorOccurred, // 当发生网络错误时
            this, &TcpDataSource::onErrorOccurred); // 触发处理错误的槽函数
}

// 注意：TCP 连接是异步的。start() 只负责“发起连接”，
// 真正的成功/失败通过 stateChanged / errorOccurred 信号通知上层。
bool TcpDataSource::start() {                     // 实现接口：启动数据源
    if (m_socket->state() == QAbstractSocket::ConnectedState) { // 如果已经处于连接状态
        m_running = true;                         // 标记状态为运行中
        return true;                              // 直接返回成功
    }
    if (m_host.isEmpty() || m_port == 0) {        // 如果配置的主机为空或端口为0
        emit errorOccurred(QStringLiteral("未配置服务器地址或端口")); // 发出配置错误信号
        return false;                             // 返回启动失败
    }
    m_running = false;                            // 初始化状态为未运行
    m_socket->connectToHost(m_host, m_port);      // 发起异步 TCP 连接请求
    return true;   // 连接结果异步到达
}

void TcpDataSource::stop() {                      // 实现接口：停止数据源
    if (m_socket->state() != QAbstractSocket::UnconnectedState) { // 如果当前不是未连接状态
        m_socket->disconnectFromHost();           // 请求优雅断开连接
        if (m_socket->state() != QAbstractSocket::UnconnectedState) // 如果还没断开
            m_socket->abort();                    // 强制立即中断连接
    }
    if (m_running) {                              // 如果之前状态是运行中
        m_running = false;                        // 更新状态为未运行
        emit stateChanged(false);                 // 发出状态改变信号（已停止）
    }
}

QString TcpDataSource::name() const {             // 实现接口：获取数据源名称
    return QStringLiteral("TCP %1:%2").arg(m_host).arg(m_port); // 格式化返回（如 TCP 127.0.0.1:9000）
}

void TcpDataSource::onConnected() {               // 槽函数：连接建立成功
    m_running = true;                             // 更新状态为运行中
    emit stateChanged(true);                      // 发出状态改变信号（运行中）
}

void TcpDataSource::onDisconnected() {            // 槽函数：连接断开
    if (m_running) {                              // 如果之前状态是运行中
        m_running = false;                        // 更新状态为未运行
        emit stateChanged(false);                 // 发出状态改变信号（已停止）
    }
}

void TcpDataSource::onReadyRead() {               // 槽函数：接收到网络数据
    const QByteArray data = m_socket->readAll();  // 一次性读取缓冲区中的所有数据
    if (!data.isEmpty())                          // 如果读到了数据
        emit dataArrived(data);                   // 与串口、Mock 发出的是同一种信号
}

void TcpDataSource::onErrorOccurred(QAbstractSocket::SocketError err) { // 槽函数：处理网络错误
    if (err == QAbstractSocket::RemoteHostClosedError) // 如果是对端主动关闭了连接
        return;   // 对端正常关闭，由 onDisconnected 处理，这里直接返回

    emit errorOccurred(QStringLiteral("TCP 错误：%1").arg(m_socket->errorString())); // 发出其他网络错误信号
    if (m_running) {                              // 如果之前状态是运行中
        m_running = false;                        // 更新状态为未运行
        emit stateChanged(false);                 // 发出状态改变信号（已停止）
    }
}