# IoTScope · 工业传感器采集与监控上位机

> 基于 Qt 6 的工业 HMI，三层架构实现多源传感器数据接入 → 二进制协议解析 → 实时曲线 + SQLite 存储 + MQTT 云端上报全链路。

## 功能特性

- 🔌 **多源接入**：串口（QSerialPort）、TCP（QTcpSocket）、模拟数据源统一接口，新增源零改动上层
- 📦 **自定义二进制协议**：帧头 `0xAA55` + 长度 + 命令字 + 累加和校验，TCP 半包/粘包逐字节重同步
- 🎨 **自绘实时曲线**：QPainter + 固定容量环形缓冲，滚动时间窗口，多传感器勾选显隐
- 💾 **SQLite 高性能写入**：内存攒批 + 定时 flush 双触发，单事务批量 INSERT，timestamp 复合索引
- 📡 **MQTT 云端上报**：JSON 定时发布 + 60s 心跳，兼容 EMQX 与 OneNET 物模型，离线先落库
- 📊 **历史查询**：时间范围 + 传感器组合筛选，一键导出 CSV

## 三层架构

```
┌────────────────────────────────────────────────────┐
│           展示 / 存储层（UI + SQLite）              │
│  MainWindow · RealtimeChart · HistoryPanel         │
│  RecordService (SQLite) · MqttPublisher            |
├────────────────────────────────────────────────────┤
│           协议解析层（ProtocolParser）               │
│   滚动缓冲 → 帧头匹配 → 校验 → 浮点还原               │
├────────────────────────────────────────────────────┤
│           数据接入层（IDataSource 多态）              │
│   SerialDataSource · TcpDataSource · MockDataSource │
└────────────────────────────────────────────────────┘
```

信号槽贯穿三层：接入层 `dataArrived(QByteArray)` → 解析层 `frameParsed(SensorFrame)` → UI/存储层各自 connect，每层独立可换。

## 技术栈

| 模块 | 技术 |
|------|------|
| UI 框架 | Qt 6 (C++) |
| 串口 / 网络 | QSerialPort · QTcpSocket |
| 数据库 | SQLite (QSqlDatabase) |
| 云通信 | QMqttClient |
| 构建 | qmake (SensorMonitor.pro) |

## 项目结构

```
SensorMonitor/
├── src/
│   ├── datasource/        # 接入层：串口 / TCP / 模拟（IDataSource 多态）
│   ├── core/              # 协议解析器 + SQLite 写入服务
│   ├── ui/                # 主窗口 + 自绘曲线 + 历史面板
│   ├── mqtt/              # 云端上报
│   └── main.cpp
├── tcp_server.py          # 本地 TCP 模拟器（联调用）
├── SensorMonitor.pro      # qmake 构建脚本
└── .gitignore
```

## 编译说明

**环境要求**：Windows 10+、Qt 6.5+（MSVC 或 MinGW）

```bash
# Qt Creator 直接打开 SensorMonitor.pro，配置 Kit 后构建运行
# 或命令行：
qmake SensorMonitor.pro
mingw32-make        # MinGW
# 或 nmake          # MSVC
```

## 协议帧格式

```
┌──────┬──────┬──────┬──────────────┬──────┐
│ 帧头  │ 长度  │ 命令字│    数据域     │ 校验 │
│ AA55  │ 1B   │ 1B   │   N 字节      │ 1B   │
└──────┴──────┴──────┴──────────────┴──────┘
```

- **长度字节** = 数据域长度 + 命令字 + 校验（不包含帧头自身）
- **数据域浮点数**：小端 4 字节 IEEE-754，`std::memcpy` 还原（规避 strict aliasing 和对齐 UB）
- **校验**：命令字 + 数据域的累加和（低 8 位）
- **重同步策略**：校验失败只丢 1 字节逐字节回溯（因帧头 `0xAA55` 在数据域可能出现，丢整帧会误杀真帧）

## 测试用 Mock 数据源

项目内置 `MockDataSource`，启动时可选模拟模式：
- 正弦波 + 噪声造数据，随机 1~7 字节切片模拟 TCP 半包
- 每 30 帧故意篡改校验和造坏帧，用于验证解析器重同步逻辑

## 运行截图

<img width="1952" height="1311" alt="image" src="https://github.com/user-attachments/assets/fe213c6d-0912-4ea7-bd40-596cf03c6b64" />
<img width="1911" height="1305" alt="image" src="https://github.com/user-attachments/assets/10f0b01d-e4af-43c2-8ae8-876febbdb69c" />
<img width="1917" height="1312" alt="image" src="https://github.com/user-attachments/assets/813eb9cc-95c1-4684-867b-78d260c9ebf6" />



