#include <QApplication>
#include "mainwindow.h"
#include <signal.h>

// 全局变量，用于响应Ctrl+C中断
bool g_isRunning = true;

// 响应Ctrl+C中断函数
void signalHandler(int signal)
{
    (void)signal;
    g_isRunning = false;
}

int main(int argc, char *argv[])
{
    // 注册信号处理函数
    signal(SIGINT, signalHandler);

    #ifdef PNG_SETJMP_SUPPORTED
        png_set_error_fn(png_create_read_struct(...), [](...) { /* 空处理 */ });
    #endif

    // 创建Qt应用
    QApplication a(argc, argv);

    // 创建主窗口
    MainWindow w;
    w.show();

    // 运行Qt事件循环
    return a.exec();
}
