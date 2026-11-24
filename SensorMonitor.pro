QT += core gui widgets serialport network sql mqtt

CONFIG += c++17

TARGET = SensorMonitor
TEMPLATE = app

SOURCES += \
    src/core/RecordService.cpp \
    src/datasource/TcpDataSource.cpp \
    src/main.cpp \
    src/core/ProtocolParser.cpp \
    src/datasource/MockDataSource.cpp \
    src/datasource/SerialDataSource.cpp \
    src/mqtt/MqttPublisher.cpp \
    src/ui/HistoryPanel.cpp \
    src/ui/MainWindow.cpp \
    src/ui/RealtimeChart.cpp

HEADERS += \
    src/core/RecordService.h \
    src/core/SensorValue.h \
    src/core/ProtocolParser.h \
    src/datasource/IDataSource.h \
    src/datasource/MockDataSource.h \
    src/datasource/SerialDataSource.h \
    src/datasource/TcpDataSource.h \
    src/mqtt/MqttConfig.h \
    src/mqtt/MqttPublisher.h \
    src/ui/HistoryPanel.h \
    src/ui/MainWindow.h \
    src/ui/RealtimeChart.h

INCLUDEPATH += src