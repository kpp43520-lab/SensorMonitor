#include "datasource/SerialDataSource.h"          // 引入串口数据源头文件

SerialDataSource::SerialDataSource(const QString &portName, // 构造函数，传入串口名
                                   qint32 baudRate,         // 传入波特率
                                   QObject *parent)         // 父对象指针
    : IDataSource(parent),                        // 初始化列表：调用基类构造
    m_serial(new QSerialPort(this)),              // 初始化列表：创建串口对象并指定父对象
    m_portName(portName),                         // 初始化列表：保存端口名
    m_baudRate(baudRate) {                        // 初始化列表：保存波特率
    //设置串口参数
    m_serial->setPortName(portName);              // 设置端口号（如 COM1）
    m_serial->setBaudRate(baudRate);              // 设置波特率（如 115200）
    m_serial->setDataBits(QSerialPort::Data8);    // 设置数据位为 8 位
    m_serial->setParity(QSerialPort::NoParity);   // 设置校验位为无校验
    m_serial->setStopBits(QSerialPort::OneStop);  // 设置停止位为 1 位
    m_serial->setFlowControl(QSerialPort::NoFlowControl); // 设置无硬件/软件流控

    //关联信号和槽
    connect(m_serial, &QSerialPort::readyRead,    // 当串口收到数据时
            this, &SerialDataSource::onReadyRead); // 触发读取槽函数
    connect(m_serial, &QSerialPort::errorOccurred, // 当串口发生异常时
            this, &SerialDataSource::onErrorOccurred); // 触发错误处理槽函数
}

//启动串口
bool SerialDataSource::start() {                  // 实现接口：启动数据源
    if (m_serial->isOpen()) return true;          // 如果串口已打开，直接返回成功

    if (m_portName.isEmpty()) {                   // 如果没选择端口
        emit errorOccurred(QStringLiteral("未选择串口端口")); // 发出错误信号
        return false;                             // 返回启动失败
    }
    if (!m_serial->open(QIODevice::ReadWrite)) {  // 尝试以读写方式打开串口
        emit errorOccurred(QStringLiteral("打开串口 %1 失败：%2") // 如果打开失败
                               .arg(m_portName, m_serial->errorString())); // 发出具体错误原因
        return false;                             // 返回启动失败
    }
    emit stateChanged(true);                      // 发出状态改变信号（运行中）
    return true;                                  // 返回启动成功
}

//停止串口
void SerialDataSource::stop() {                   // 实现接口：停止数据源
    if (m_serial->isOpen()) {                     // 如果串口处于打开状态
        m_serial->close();                        // 关闭串口
        emit stateChanged(false);                 // 发出状态改变信号（已停止）
    }
}

QString SerialDataSource::name() const {          // 实现接口：获取数据源名称
    return QStringLiteral("串口 %1 @ %2").arg(m_portName).arg(m_baudRate); // 格式化返回（如 串口 COM1 @ 115200）
}

//读取串口数据，并触发信号
void SerialDataSource::onReadyRead() {            // 槽函数：串口有数据到达
    const QByteArray data = m_serial->readAll();  // 一次性读取缓冲区中的所有数据
    if (!data.isEmpty())                          // 如果读到了数据
        emit dataArrived(data);                   // 与 Mock 发出的是同一种信号，上层不感知
}

void SerialDataSource::onErrorOccurred(QSerialPort::SerialPortError err) { // 槽函数：处理串口错误
    if (err == QSerialPort::NoError) return;      // 如果没有错误，直接返回

    if (err == QSerialPort::ResourceError) {      // 如果是资源错误（如 USB 线被拔掉）
        // USB 串口被拔出等异常断开
        emit errorOccurred(QStringLiteral("串口异常断开：%1") // 发出异常断开信号
                               .arg(m_serial->errorString())); // 附带底层错误信息
        stop();                                   // 强制关闭并停止数据源
    }
    // 其他错误（如 PermissionError）在 open() 失败时已经上报，这里静默
}