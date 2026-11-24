#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);  //创建应用对象，事件循环的载体
    MainWindow w;       //构造主窗口（栈上）
    w.show();           //显示窗口
    return app.exec();  //进入事件循环，程序“停”在这里接收消息
}

/*
 * 关键点 ：`app.exec()` 之后代码就卡住了，所有后续动作（点击、定时器、网络数据）都是 事件循环回调 驱动的。这就是 Qt 程序的运行模型。
 * 事件循环回调：事件循环是“有事件才醒、处理完就回去睡”的无限循环；回调是“你提前把函数地址交给框架，框架在对应事件到来时替你调用它”
 */