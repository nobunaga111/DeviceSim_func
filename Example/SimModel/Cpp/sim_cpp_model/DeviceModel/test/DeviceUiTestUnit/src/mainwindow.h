// 在现有的 mainwindow.h 文件中添加声纳方程相关的声明

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QScrollBar>
#include <QDateTime>
#include <QApplication>
#include <QScreen>
#include <QMap>
#include <QDialog>           // *** 用于DI参数设置对话框 ***
#include <QDoubleSpinBox>    // *** 用于DI参数设置 ***

#include "CreateDeviceModel.h"
#include "CSimComponentBase.h"
#include "DeviceModelAgent.h"
#include "DeviceTestInOut.h"
#include "devicemodel.h"     // *** 用于访问DeviceModel类 ***

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    //========== 声纳控制功能 ==========//
    // 声纳阵列开关按钮点击事件
    void onArraySwitchClicked(int sonarID, bool isOn);
    // 被动声纳开关按钮点击事件
    void onPassiveSwitchClicked(int sonarID, bool isOn);
    // 主动声纳开关按钮点击事件
    void onActiveSwitchClicked(int sonarID, bool isOn);
    // 侦察声纳开关按钮点击事件
    void onScoutingSwitchClicked(int sonarID, bool isOn);
    // 初始化声纳按钮点击事件
    void onInitSonarClicked(int sonarID);

    //========== 消息发送功能 ==========//
    // 发送连续声数据按钮点击事件
    void onSendContinuousSoundClicked();

    //========== 日志功能 ==========//
    // 清除日志按钮点击事件
    void onClearLogClicked();

    //========== 声纳方程相关功能 ==========//
    // 显示声纳方程计算结果
    void onShowEquationResultsClicked();
    // 发送完整测试数据（包含平台噪声、环境噪声、传播声）
    void onSendCompleteTestDataClicked();
    // 设置DI参数
    void onSetDIParametersClicked();

private:
    //========== UI初始化函数 ==========//
    // 初始化UI界面
    void initializeUI();
    // 创建声纳控制面板
    void createSonarControlPanel();
    // 创建消息发送面板
    void createMessageSendPanel();
    // 创建日志面板
    void createLogPanel();

    //========== 辅助函数 ==========//
    // 添加日志信息
    void addLog(const QString& message);
    // 发送声纳控制命令
    void sendSonarControlOrder(int sonarID, bool arrayWorkingOrder, bool passiveWorkingOrder,
                              bool activeWorkingOrder, bool scoutingWorkingOrder);
    // 创建被动声纳初始化数据
    CAttr_PassiveSonarComponent createPassiveSonarConfig(int sonarID);
    // 创建连续声数据
    CMsg_PropagatedContinuousSoundListStruct createContinuousSoundData();
    // 初始化所有声纳状态
    void initializeSonarStates();

private:
    //========== 主要组件 ==========//
    QTabWidget* m_tabWidget;                  // 标签页容器
    QWidget* m_sonarControlTab;               // 声纳控制标签页
    QWidget* m_messageSendTab;                // 消息发送标签页
    QTextEdit* m_logTextEdit;                 // 日志文本编辑框
    QPushButton* m_clearLogButton;            // 清除日志按钮

    //========== 声纳控制组件 ==========//
    QMap<int, QGroupBox*> m_sonarGroupBoxes;  // 声纳分组框映射表
    struct SonarControls {
        QPushButton* arraySwitch;             // 阵列开关按钮
        QPushButton* passiveSwitch;           // 被动声纳开关按钮
        QPushButton* activeSwitch;            // 主动声纳开关按钮
        QPushButton* scoutingSwitch;          // 侦察声纳开关按钮
        QPushButton* initButton;              // 初始化按钮
    };
    QMap<int, SonarControls> m_sonarControls; // 声纳控制组件映射表

    //========== 消息发送组件 ==========//
    QPushButton* m_sendContinuousSoundButton; // 发送连续声数据按钮

    //========== 声纳方程界面组件 ==========//
    QGroupBox* m_equationGroupBox;              // 声纳方程测试组
    QPushButton* m_showEquationResultsButton;   // 显示计算结果按钮
    QPushButton* m_sendCompleteTestDataButton;  // 发送完整测试数据按钮
    QPushButton* m_setDIParametersButton;       // 设置DI参数按钮
    QTextEdit* m_equationResultsTextEdit;       // 显示计算结果的文本框

    //========== 模型组件 ==========//
    CSimComponentBase* m_component;           // 声纳组件
    DeviceModelAgent* m_agent;                // 代理

    //========== 状态信息 ==========//
    QMap<int, bool> m_arrayWorkingStates;     // 阵列工作状态
    QMap<int, bool> m_passiveWorkingStates;   // 被动声纳工作状态
    QMap<int, bool> m_activeWorkingStates;    // 主动声纳工作状态
    QMap<int, bool> m_scoutingWorkingStates;  // 侦察声纳工作状态
};

#endif // MAINWINDOW_H
