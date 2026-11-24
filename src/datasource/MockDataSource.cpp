#include "datasource/MockDataSource.h"            // 引入 MockDataSource 头文件
#include <QRandomGenerator>                       // 引入随机数生成器，用于产生噪声数据
#include <QtMath>                                 // 引入数学库，用于计算正弦波
#include <cstring>                                // 引入 C 字符串库，用于内存拷贝

MockDataSource::MockDataSource(QObject *parent)   // 构造函数，初始化父对象
    : IDataSource(parent), m_timer(new QTimer(this)) { // 初始化列表：调用基类构造，创建定时器
    m_timer->setInterval(200);                    // 设置定时器周期为 200 毫秒（每秒产生5帧）
    connect(m_timer, &QTimer::timeout, this, &MockDataSource::onTick); // 绑定定时器超时信号到处理槽
}

bool MockDataSource::start() {                    // 启动数据源
    if (m_timer->isActive()) return true;         // 如果已经在运行，直接返回成功
    m_phase = 0.0;                                // 重置正弦波相位
    m_tickCount = 0;                              // 重置计数器
    m_timer->start();                             // 启动定时器
    emit stateChanged(true);                      // 发出状态改变信号（运行中）
    return true;                                  // 返回启动成功
}

void MockDataSource::stop() {                     // 停止数据源
    if (!m_timer->isActive()) return;             // 如果没在运行，直接返回
    m_timer->stop();                              // 停止定时器
    emit stateChanged(false);                     // 发出状态改变信号（已停止）
}

void MockDataSource::onTick() {                   // 定时器触发函数，生成模拟数据
    ++m_tickCount;                                // 计数器加 1
    m_phase += 0.15;                              // 相位递增，产生波形移动

    auto *rng = QRandomGenerator::global();       // 获取全局随机数生成器指针

    // 生成带随机噪声的正弦波数据（模拟温度、电压、电流）
    const float temp = 25.0f + 5.0f * qSin(m_phase) // 计算温度基准值
                       + static_cast<float>((rng->generateDouble() - 0.5) * 0.6); // 叠加随机噪声
    const float volt = 12.0f + 0.8f * qSin(m_phase * 0.7 + 1.2) // 计算电压基准值
                       + static_cast<float>((rng->generateDouble() - 0.5) * 0.1); // 叠加随机噪声
    const float curr = 2.0f + 0.5f * qSin(m_phase * 0.5 + 2.0) // 计算电流基准值
                       + static_cast<float>((rng->generateDouble() - 0.5) * 0.05); // 叠加随机噪声

    QVector<SensorValue> items;                   // 创建数据集合
    items.append({1, temp, 1, {}});               // 添加温度数据（ID:1, 单位:1）
    items.append({2, volt, 2, {}});               // 添加电压数据（ID:2, 单位:2）
    items.append({3, curr, 3, {}});               // 添加电流数据（ID:3, 单位:3）

    QByteArray stream;                            // 创建待发送的字节流
    const int frames = (rng->bounded(4) == 0) ? 2 : 1; // 1/4 概率生成2帧（模拟粘包），否则1帧
    for (int i = 0; i < frames; ++i)              // 根据帧数循环
        stream.append(buildFrame(items));         // 组包并追加到字节流中

    if (m_tickCount % 30 == 0) {                  // 每触发 30 次（约6秒）
        QByteArray bad = buildFrame(items);       // 构造一个正常帧
        bad[bad.size() - 1] = static_cast<char>(  // 修改最后一个字节（校验和）
            static_cast<quint8>(bad[bad.size() - 1]) ^ 0xFF); // 通过异或取反制造校验错误
        stream.append(bad);                       // 将错误帧追加到字节流，测试解析器的容错
    }

    emitSliced(stream);                           // 将字节流切片并发送
}

QByteArray MockDataSource::buildFrame(const QVector<SensorValue> &items) { // 将数据组装为符合协议的帧
    QByteArray data;                              // 创建数据域字节流
    for (const SensorValue &it : items) {         // 遍历所有传感器数据
        data.append(static_cast<char>(it.id));    // 写入传感器 ID（1字节）

        quint32 bits = 0;                         // 准备 32 位无符号整数
        std::memcpy(&bits, &it.value, sizeof(bits)); // 将 float 的内存按位拷贝到整数中
        data.append(static_cast<char>(bits & 0xFF)); // 写入浮点数最低字节
        data.append(static_cast<char>((bits >> 8) & 0xFF)); // 写入次低字节
        data.append(static_cast<char>((bits >> 16) & 0xFF)); // 写入次高字节
        data.append(static_cast<char>((bits >> 24) & 0xFF)); // 写入最高字节（小端序）

        data.append(static_cast<char>(it.unit));  // 写入单位代码（1字节）
    }

    QByteArray frame;                             // 创建完整的帧
    frame.append(static_cast<char>(0xAA));        // 写入帧头第一字节
    frame.append(static_cast<char>(0x55));        // 写入帧头第二字节
    frame.append(static_cast<char>(data.size())); // 写入数据域长度（1字节）
    frame.append(static_cast<char>(0x01));        // 写入命令字（0x01 代表数据上报）
    frame.append(data);                           // 写入数据域内容

    quint8 sum = 0;                               // 初始化校验和变量
    for (int i = 2; i < frame.size(); ++i)        // 从长度字段开始遍历
        sum = static_cast<quint8>(sum + static_cast<quint8>(frame[i])); // 累加计算校验和
    frame.append(static_cast<char>(sum));         // 将校验和追加到帧末尾

    return frame;                                 // 返回组装好的完整帧
}

void MockDataSource::emitSliced(const QByteArray &data) { // 模拟网络传输，将数据随机切片发送
    auto *rng = QRandomGenerator::global();       // 获取全局随机数生成器
    int pos = 0;                                  // 当前发送位置索引
    const int totalSize = static_cast<int>(data.size()); // 数据总长度
    while (pos < totalSize) {                     // 只要没发完就继续循环
        int n = rng->bounded(1, 8);               // 随机生成 1 到 7 的切片长度
        n = qMin(n, totalSize - pos);             // 确保切片长度不超过剩余数据长度
        emit dataArrived(data.mid(pos, n));       // 截取指定长度的数据并发出信号
        pos += n;                                 // 更新当前发送位置
    }
}