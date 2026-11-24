#include "core/ProtocolParser.h"                  // 引入协议解析器头文件
#include <cstring>                                // 引入 C 字符串库，用于内存拷贝
#include <QDateTime>                              // 引入 Qt 时间类，用于打时间戳

static const quint8 HEAD0 = 0xAA;                 // 定义协议帧头第一字节
static const quint8 HEAD1 = 0x55;                 // 定义协议帧头第二字节

static inline quint8 byteAt(const QByteArray &a, int i) { // 内联辅助函数：安全获取指定位置的字节
    return static_cast<quint8>(static_cast<unsigned char>(a[i])); // 强转为无符号字节，避免符号扩展
}

ProtocolParser::ProtocolParser(QObject *parent) : QObject(parent) {} // 构造函数

void ProtocolParser::reset() {                    // 重置解析器状态
    m_buffer.clear();                             // 清空接收缓冲区
    m_frameCount = 0;                             // 重置成功帧计数器
    m_errorCount = 0;                             // 重置错误帧计数器
}

void ProtocolParser::append(const QByteArray &bytes) { // 追加新收到的数据到缓冲区
    m_buffer.append(bytes);                       // 将新数据拼接到缓冲区尾部

    QByteArray frame;                             // 用于存放提取出的完整帧
    while (tryParseOne(frame)) {                  // 循环尝试从缓冲区提取完整帧
        ++m_frameCount;                           // 成功解析帧数加一
        emit rawFrame(frame);                     // 发出完整原始帧信号（供UI显示）

        const quint8 cmd = byteAt(frame, 3);      // 读取命令字（偏移3）
        const int dataLen = byteAt(frame, 2);     // 读取数据域长度（偏移2）
        const char *data = frame.constData() + 4; // 获取数据域首地址指针

        QVector<SensorValue> values;              // 创建解析结果集合
        if (cmd == 0x01) {                        // 如果是数据上报命令
            const int groupSize = 6;              // 定义每组数据长度（ID1+数值4+单位1）
            const int n = dataLen / groupSize;    // 计算数据域中包含几组数据

            for (int i = 0; i < n; ++i) {         // 遍历每组数据
                SensorValue v;                    // 创建单个传感器数据结构体
                v.id = static_cast<quint8>(static_cast<unsigned char>(data[i * groupSize])); // 解析传感器ID

                quint32 bits = static_cast<quint8>(static_cast<unsigned char>(data[i * groupSize + 1])) // 组合第1字节（低8位）
                               | (static_cast<quint8>(static_cast<unsigned char>(data[i * groupSize + 2])) << 8) // 组合第2字节（左移8位）
                               | (static_cast<quint8>(static_cast<unsigned char>(data[i * groupSize + 3])) << 16) // 组合第3字节（左移16位）
                               | (static_cast<quint8>(static_cast<unsigned char>(data[i * groupSize + 4])) << 24); // 组合第4字节（左移24位）

                std::memcpy(&v.value, &bits, sizeof(bits)); // 按位拷贝，将32位整数还原为浮点数

                v.unit = static_cast<quint8>(static_cast<unsigned char>(data[i * groupSize + 5])); // 解析单位代码
                v.timestamp = QDateTime::currentDateTime(); // 记录当前系统时间作为时间戳
                values.append(v);                 // 将解析好的数据添加到集合中
            }
        }
        if (!values.isEmpty())                    // 如果解析出了有效数据
            emit frameParsed(values);             // 发出解析结果信号（供UI和业务层使用）
    }
}

bool ProtocolParser::tryParseOne(QByteArray &outFrame) { // 尝试从缓冲区中提取完整帧
    while (true) {                                // 持续尝试
        while (m_buffer.size() >= 2) {            // 只要缓冲区还有至少2字节
            if (byteAt(m_buffer, 0) == HEAD0 && byteAt(m_buffer, 1) == HEAD1) // 如果找到帧头（AA 55）
                break;                            // 跳出寻找循环
            m_buffer.remove(0, 1);                // 没找到帧头，丢弃第一个字节继续找（重新同步）
        }

        if (m_buffer.size() < 2) return false;    // 剩余数据不够判断帧头，等待下次数据
        if (m_buffer.size() < 4) return false;    // 剩余数据不够读取长度和命令字，等待

        const int dataLen = byteAt(m_buffer, 2);  // 读取数据域长度
        const int total = 2 + 1 + 1 + dataLen + 1; // 计算整帧总长度（头2+长度1+命令1+数据N+校验1）

        if (m_buffer.size() < total) return false; // 数据不够一帧（半包），等待后续数据

        quint8 sum = 0;                           // 初始化校验和计算变量
        for (int i = 2; i < 4 + dataLen; ++i)     // 从长度字段开始到数据域结尾
            sum = static_cast<quint8>(sum + byteAt(m_buffer, i)); // 累加计算校验和

        const quint8 recv = byteAt(m_buffer, 4 + dataLen); // 读取帧尾附带的校验和

        if (sum != recv) {                        // 如果计算值与接收值不一致（校验失败）
            ++m_errorCount;                       // 错误计数器加一
            emit parseError(QStringLiteral("校验失败：计算 %1，接收 %2，丢弃 1 字节重新同步") // 发出解析错误信号
                                .arg(sum).arg(recv)); // 附带计算值和接收值
            m_buffer.remove(0, 1);                // 丢弃当前错误帧的帧头，重新寻找下一个帧头
            continue;                             // 继续外层循环，尝试解析下一个帧
        }

        outFrame = m_buffer.left(total);          // 提取完整的一帧数据
        m_buffer.remove(0, total);                // 从缓冲区中删除已提取的数据
        return true;                              // 成功提取出一帧，返回 true
    }
}