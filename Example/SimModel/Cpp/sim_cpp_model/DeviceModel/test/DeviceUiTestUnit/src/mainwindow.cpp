#include "mainwindow.h"
#include <random>
#include <chrono>
#include <cstring>

// 构造函数
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
    m_component(nullptr),
    m_agent(nullptr)
{
    // 设置窗口标题和大小
    setWindowTitle("声纳系统测试平台");
    resize(1000, 600);

    // 居中显示窗口
    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    // 初始化界面
    initializeUI();

    // 创建声纳模型实例
    m_component = createComponent("sonar");
    m_agent = new DeviceModelAgent();

    // 初始化声纳模型
    if (m_component && m_agent) {
        m_component->init(m_agent, nullptr);
        m_component->start();
        addLog("声纳模型初始化完成");
    } else {
        addLog("错误：声纳模型初始化失败");
    }

    // 初始化声纳状态映射 - 默认全部开启（4个声纳：0,1,2,3）
    for (int id = 0; id < 4; id++) {
        m_arrayWorkingStates[id] = true;        // 默认开启
        m_passiveWorkingStates[id] = true;      // 默认开启
        m_activeWorkingStates[id] = true;       // 默认开启
        m_scoutingWorkingStates[id] = true;     // 默认开启
    }

    // 初始化所有声纳状态
    initializeSonarStates();
}

// 析构函数
MainWindow::~MainWindow()
{
    // 释放资源
    if (m_component) {
        delete m_component;
        m_component = nullptr;
    }

    if (m_agent) {
        delete m_agent;
        m_agent = nullptr;
    }
}

// 初始化UI界面
void MainWindow::initializeUI()
{
    // 创建中央部件
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);

    // 创建各个标签页
    m_sonarControlTab = new QWidget(m_tabWidget);
    m_messageSendTab = new QWidget(m_tabWidget);

    // 创建各个面板
    createSonarControlPanel();
    createMessageSendPanel();
    createLogPanel();

    // 添加标签页
    m_tabWidget->addTab(m_sonarControlTab, "声纳控制");
    m_tabWidget->addTab(m_messageSendTab, "消息发送");

    // 创建底部日志容器
    QWidget* logContainer = new QWidget(this);
    QVBoxLayout* logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(0, 0, 0, 0);

    // 添加日志控件和按钮到底部容器
    logLayout->addWidget(m_logTextEdit);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_clearLogButton);
    logLayout->addLayout(buttonLayout);

    // 添加标签页和日志容器到主布局
    mainLayout->addWidget(m_tabWidget, 3);      // 标签页占3/4
    mainLayout->addWidget(logContainer, 1);     // 日志区占1/4
}

// 创建声纳控制面板
void MainWindow::createSonarControlPanel()
{
    // 创建声纳控制面板布局
    QGridLayout* layout = new QGridLayout(m_sonarControlTab);

    // 声纳类型和位置定义
    struct SonarInfo {
        int id;
        QString name;
        QString description;
    };

    // 声纳类型列表（精简为4个，ID为0,1,2,3）
    QList<SonarInfo> sonars = {
        {0, "艏端声纳", "前向声纳，用于探测前方目标"},
        {1, "舷侧声纳", "侧向声纳，扩展探测角度"},
        {2, "粗拖声纳", "拖曳式粗孔径声纳阵列"},
        {3, "细拖声纳", "拖曳式细孔径声纳阵列"}
    };

    // 创建各声纳控制组（2x2布局）
    int row = 0;
    int col = 0;
    for (const SonarInfo& sonar : sonars) {
        // 创建声纳分组框
        QGroupBox* groupBox = new QGroupBox(sonar.name);
        QVBoxLayout* groupLayout = new QVBoxLayout(groupBox);

        // 创建声纳状态标签
        QLabel* descLabel = new QLabel(sonar.description);
        descLabel->setWordWrap(true);
        groupLayout->addWidget(descLabel);

        // 创建控制按钮
        QGridLayout* buttonGrid = new QGridLayout();

        // 阵列开关按钮 - 默认开启状态
        QPushButton* arraySwitch = new QPushButton("阵列: 开启");
        arraySwitch->setCheckable(true);
        arraySwitch->setChecked(true);  // 默认选中
        connect(arraySwitch, &QPushButton::clicked, [this, sonar](bool checked) {
            onArraySwitchClicked(sonar.id, checked);
        });
        buttonGrid->addWidget(arraySwitch, 0, 0);

        // 被动声纳开关按钮 - 默认开启状态
        QPushButton* passiveSwitch = new QPushButton("被动: 开启");
        passiveSwitch->setCheckable(true);
        passiveSwitch->setChecked(true);  // 默认选中
        passiveSwitch->setEnabled(true);   // 默认启用
        connect(passiveSwitch, &QPushButton::clicked, [this, sonar](bool checked) {
            onPassiveSwitchClicked(sonar.id, checked);
        });
        buttonGrid->addWidget(passiveSwitch, 0, 1);

        // 主动声纳开关按钮 - 默认开启状态
        QPushButton* activeSwitch = new QPushButton("主动: 开启");
        activeSwitch->setCheckable(true);
        activeSwitch->setChecked(true);  // 默认选中
        activeSwitch->setEnabled(true);   // 默认启用
        connect(activeSwitch, &QPushButton::clicked, [this, sonar](bool checked) {
            onActiveSwitchClicked(sonar.id, checked);
        });
        buttonGrid->addWidget(activeSwitch, 1, 0);

        // 侦察声纳开关按钮 - 默认开启状态
        QPushButton* scoutingSwitch = new QPushButton("侦察: 开启");
        scoutingSwitch->setCheckable(true);
        scoutingSwitch->setChecked(true);  // 默认选中
        scoutingSwitch->setEnabled(true);   // 默认启用
        connect(scoutingSwitch, &QPushButton::clicked, [this, sonar](bool checked) {
            onScoutingSwitchClicked(sonar.id, checked);
        });
        buttonGrid->addWidget(scoutingSwitch, 1, 1);

        // 声纳初始化按钮
        QPushButton* initButton = new QPushButton("初始化声纳");
        connect(initButton, &QPushButton::clicked, [this, sonar]() {
            onInitSonarClicked(sonar.id);
        });
        buttonGrid->addWidget(initButton, 2, 0, 1, 2);

        // 添加按钮网格到分组布局
        groupLayout->addLayout(buttonGrid);

        // 保存控件引用
        SonarControls controls;
        controls.arraySwitch = arraySwitch;
        controls.passiveSwitch = passiveSwitch;
        controls.activeSwitch = activeSwitch;
        controls.scoutingSwitch = scoutingSwitch;
        controls.initButton = initButton;
        m_sonarControls[sonar.id] = controls;
        m_sonarGroupBoxes[sonar.id] = groupBox;

        // 添加分组框到布局（2x2排列）
        layout->addWidget(groupBox, row, col);

        // 更新行列位置
        col++;
        if (col > 1) {  // 每行2个
            col = 0;
            row++;
        }
    }

    // 设置布局
    m_sonarControlTab->setLayout(layout);
}

void MainWindow::createMessageSendPanel()
{
    // 创建消息发送面板布局
    QVBoxLayout* layout = new QVBoxLayout(m_messageSendTab);

    // 创建消息发送组
    QGroupBox* messageGroupBox = new QGroupBox("声音数据发送");
    QVBoxLayout* messageLayout = new QVBoxLayout(messageGroupBox);

    // 创建发送连续声数据按钮
    m_sendContinuousSoundButton = new QPushButton("发送连续声数据 (MSG_PropagatedContinuousSound)");
    m_sendContinuousSoundButton->setMinimumHeight(40);
    connect(m_sendContinuousSoundButton, &QPushButton::clicked, this, &MainWindow::onSendContinuousSoundClicked);

    // 添加说明标签
    QLabel* descLabel = new QLabel("点击按钮发送一次MSG_PropagatedContinuousSound消息到声纳模型");
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: gray; font-size: 12px;");

    // 添加控件到消息发送布局
    messageLayout->addWidget(descLabel);
    messageLayout->addWidget(m_sendContinuousSoundButton);

    // *** 新增：声纳方程测试组 ***
    m_equationGroupBox = new QGroupBox("声纳方程计算测试");
    QVBoxLayout* equationLayout = new QVBoxLayout(m_equationGroupBox);

    // 发送完整测试数据按钮
    m_sendCompleteTestDataButton = new QPushButton("发送完整测试数据");
    m_sendCompleteTestDataButton->setMinimumHeight(40);
    m_sendCompleteTestDataButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    connect(m_sendCompleteTestDataButton, &QPushButton::clicked, this, &MainWindow::onSendCompleteTestDataClicked);

    // 显示计算结果按钮
    m_showEquationResultsButton = new QPushButton("显示声纳方程计算结果");
    m_showEquationResultsButton->setMinimumHeight(40);
    m_showEquationResultsButton->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold;");
    connect(m_showEquationResultsButton, &QPushButton::clicked, this, &MainWindow::onShowEquationResultsClicked);

    // 设置DI参数按钮
    m_setDIParametersButton = new QPushButton("设置DI参数");
    m_setDIParametersButton->setMinimumHeight(40);
    m_setDIParametersButton->setStyleSheet("background-color: #FF9800; color: white; font-weight: bold;");
    connect(m_setDIParametersButton, &QPushButton::clicked, this, &MainWindow::onSetDIParametersClicked);

    // 创建按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_sendCompleteTestDataButton);
    buttonLayout->addWidget(m_showEquationResultsButton);

    // 计算结果显示区域
    m_equationResultsTextEdit = new QTextEdit();
    m_equationResultsTextEdit->setReadOnly(true);
    m_equationResultsTextEdit->setMaximumHeight(200);
    m_equationResultsTextEdit->setPlainText("点击\"发送完整测试数据\"开始测试声纳方程计算功能...");

    // 添加说明标签
    QLabel* equationDescLabel = new QLabel(
        "声纳方程计算测试流程：\n"
        "1. 点击\"发送完整测试数据\"发送所需的三种数据\n"
        "2. 等待2-3秒让系统处理数据\n"
        "3. 点击\"显示计算结果\"查看计算结果\n"
        "4. 可选：点击\"设置DI参数\"调整计算参数"
    );
    equationDescLabel->setWordWrap(true);
    equationDescLabel->setStyleSheet("color: #666; font-size: 11px; padding: 5px; background-color: #f0f0f0; border-radius: 3px;");

    // 添加控件到声纳方程布局
    equationLayout->addWidget(equationDescLabel);
    equationLayout->addLayout(buttonLayout);
    equationLayout->addWidget(m_setDIParametersButton);
    equationLayout->addWidget(new QLabel("计算结果："));
    equationLayout->addWidget(m_equationResultsTextEdit);

    // 添加分组框到主布局
    layout->addWidget(messageGroupBox);
    layout->addWidget(m_equationGroupBox);  // 添加新的声纳方程测试组
    layout->addStretch(1);

    // 设置消息发送标签页布局
    m_messageSendTab->setLayout(layout);
}

// 创建日志面板
void MainWindow::createLogPanel()
{
    // 创建日志文本编辑框
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);

    // 创建清除日志按钮
    m_clearLogButton = new QPushButton("清除日志", this);
    connect(m_clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
}

// 初始化所有声纳状态
void MainWindow::initializeSonarStates()
{
    // 等待界面完全创建后再发送控制命令
    QTimer::singleShot(100, this, [this]() {
        for (int id = 0; id < 4; id++) {  // 只有4个声纳：0,1,2,3
            // 发送声纳控制命令
            sendSonarControlOrder(id,
                                  m_arrayWorkingStates[id],
                                  m_passiveWorkingStates[id],
                                  m_activeWorkingStates[id],
                                  m_scoutingWorkingStates[id]);

            // 初始化声纳配置
            onInitSonarClicked(id);
        }
        addLog("所有声纳已初始化为开启状态");
    });
}

// 添加日志信息
void MainWindow::addLog(const QString& message)
{
    if (!m_logTextEdit) return;

    // 获取当前时间
    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss.zzz] ");

    // 添加带时间戳的日志
    m_logTextEdit->append(timestamp + message);

    // 滚动到底部
    QScrollBar* scrollBar = m_logTextEdit->verticalScrollBar();
    if (scrollBar) {
        scrollBar->setValue(scrollBar->maximum());
    }
}

// 声纳阵列开关按钮点击事件
void MainWindow::onArraySwitchClicked(int sonarID, bool isOn)
{
    if (!m_component) return;

    // 更新声纳状态
    m_arrayWorkingStates[sonarID] = isOn;

    // 更新按钮文本
    m_sonarControls[sonarID].arraySwitch->setText(isOn ? "阵列: 开启" : "阵列: 关闭");

    // 启用或禁用功能按钮
    m_sonarControls[sonarID].passiveSwitch->setEnabled(isOn);
    m_sonarControls[sonarID].activeSwitch->setEnabled(isOn);
    m_sonarControls[sonarID].scoutingSwitch->setEnabled(isOn);

    // 如果关闭阵列，同时关闭所有功能
    if (!isOn) {
        m_passiveWorkingStates[sonarID] = false;
        m_activeWorkingStates[sonarID] = false;
        m_scoutingWorkingStates[sonarID] = false;

        m_sonarControls[sonarID].passiveSwitch->setChecked(false);
        m_sonarControls[sonarID].activeSwitch->setChecked(false);
        m_sonarControls[sonarID].scoutingSwitch->setChecked(false);

        m_sonarControls[sonarID].passiveSwitch->setText("被动: 关闭");
        m_sonarControls[sonarID].activeSwitch->setText("主动: 关闭");
        m_sonarControls[sonarID].scoutingSwitch->setText("侦察: 关闭");
    }

    // 发送声纳控制命令
    sendSonarControlOrder(sonarID,
                          m_arrayWorkingStates[sonarID],
                          m_passiveWorkingStates[sonarID],
                          m_activeWorkingStates[sonarID],
                          m_scoutingWorkingStates[sonarID]);

    // 添加日志
    addLog(QString("声纳 %1 阵列已%2").arg(sonarID).arg(isOn ? "开启" : "关闭"));
}

// 被动声纳开关按钮点击事件
void MainWindow::onPassiveSwitchClicked(int sonarID, bool isOn)
{
    if (!m_component) return;

    // 更新声纳状态
    m_passiveWorkingStates[sonarID] = isOn;

    // 更新按钮文本
    m_sonarControls[sonarID].passiveSwitch->setText(isOn ? "被动: 开启" : "被动: 关闭");

    // 发送声纳控制命令
    sendSonarControlOrder(sonarID,
                          m_arrayWorkingStates[sonarID],
                          m_passiveWorkingStates[sonarID],
                          m_activeWorkingStates[sonarID],
                          m_scoutingWorkingStates[sonarID]);

    // 添加日志
    addLog(QString("声纳 %1 被动功能已%2").arg(sonarID).arg(isOn ? "开启" : "关闭"));
}

// 主动声纳开关按钮点击事件
void MainWindow::onActiveSwitchClicked(int sonarID, bool isOn)
{
    if (!m_component) return;

    // 更新声纳状态
    m_activeWorkingStates[sonarID] = isOn;

    // 更新按钮文本
    m_sonarControls[sonarID].activeSwitch->setText(isOn ? "主动: 开启" : "主动: 关闭");

    // 发送声纳控制命令
    sendSonarControlOrder(sonarID,
                          m_arrayWorkingStates[sonarID],
                          m_passiveWorkingStates[sonarID],
                          m_activeWorkingStates[sonarID],
                          m_scoutingWorkingStates[sonarID]);

    // 添加日志
    addLog(QString("声纳 %1 主动功能已%2").arg(sonarID).arg(isOn ? "开启" : "关闭"));
}

// 侦察声纳开关按钮点击事件
void MainWindow::onScoutingSwitchClicked(int sonarID, bool isOn)
{
    if (!m_component) return;

    // 更新声纳状态
    m_scoutingWorkingStates[sonarID] = isOn;

    // 更新按钮文本
    m_sonarControls[sonarID].scoutingSwitch->setText(isOn ? "侦察: 开启" : "侦察: 关闭");

    // 发送声纳控制命令
    sendSonarControlOrder(sonarID,
                          m_arrayWorkingStates[sonarID],
                          m_passiveWorkingStates[sonarID],
                          m_activeWorkingStates[sonarID],
                          m_scoutingWorkingStates[sonarID]);

    // 添加日志
    addLog(QString("声纳 %1 侦察功能已%2").arg(sonarID).arg(isOn ? "开启" : "关闭"));
}

// 初始化声纳按钮点击事件
void MainWindow::onInitSonarClicked(int sonarID)
{
    if (!m_component) return;

    // 创建被动声纳初始化配置
    CAttr_PassiveSonarComponent config = createPassiveSonarConfig(sonarID);

    // 创建初始化消息
    CSimMessage msg;
    msg.dataFormat = STRUCT;
    msg.data = &config;
    msg.length = sizeof(config);
    memcpy(msg.topic, ATTR_PassiveSonarComponent, sizeof(ATTR_PassiveSonarComponent));

    // 发送初始化消息
    m_component->onMessage(&msg);

    // 添加日志
    addLog(QString("声纳 %1 已初始化").arg(sonarID));
}

// 发送连续声数据按钮点击事件
void MainWindow::onSendContinuousSoundClicked()
{
    if (!m_component) {
        addLog("错误：声纳组件未初始化");
        return;
    }

    try {
        // 创建连续声数据
        CMsg_PropagatedContinuousSoundListStruct continuousSound = createContinuousSoundData();

        // 创建消息
        CSimMessage continuousMsg;
        continuousMsg.dataFormat = STRUCT;
        continuousMsg.time = QDateTime::currentMSecsSinceEpoch();
        continuousMsg.sender = 1;
        continuousMsg.senderComponentId = 1;
        continuousMsg.receiver = 1885;
        continuousMsg.data = &continuousSound;
        continuousMsg.length = sizeof(continuousSound);
        memcpy(continuousMsg.topic, MSG_PropagatedContinuousSound, strlen(MSG_PropagatedContinuousSound) + 1);

        // 发送消息
        m_component->onMessage(&continuousMsg);

        // 添加日志
        addLog(QString("已发送连续声数据，目标数量: %1").arg(continuousSound.propagatedContinuousList.size()));

    } catch(const std::exception& e) {
        addLog(QString("发送连续声数据时出错: %1").arg(e.what()));
    } catch(...) {
        addLog("发送连续声数据时发生未知错误");
    }
}

// 清除日志按钮点击事件
void MainWindow::onClearLogClicked()
{
    if (m_logTextEdit) {
        m_logTextEdit->clear();
        addLog("日志已清除");
    }
}

// 发送声纳控制命令
void MainWindow::sendSonarControlOrder(int sonarID, bool arrayWorkingOrder, bool passiveWorkingOrder,
                                      bool activeWorkingOrder, bool scoutingWorkingOrder)
{
    if (!m_component) return;

    try {
        // 创建声纳控制命令
        CMsg_SonarCommandControlOrder order;
        order.sonarID = sonarID;
        order.arrayWorkingOrder = arrayWorkingOrder ? 1 : 0;
        order.passiveWorkingOrder = passiveWorkingOrder ? 1 : 0;
        order.activeWorkingOrder = activeWorkingOrder ? 1 : 0;
        order.scoutingWorkingOrder = scoutingWorkingOrder ? 1 : 0;
        order.multiSendWorkingOrder = 0;       // 默认关闭
        order.multiReceiveWorkingOrder = 0;    // 默认关闭
        order.activeTransmitWorkingOrder = 0;  // 默认关闭

        // 创建控制消息
        CSimMessage controlMsg;
        controlMsg.dataFormat = STRUCT;
        controlMsg.data = &order;
        controlMsg.length = sizeof(order);
        memcpy(controlMsg.topic, MSG_SonarCommandControlOrder, strlen(MSG_SonarCommandControlOrder) + 1);

        // 发送控制命令
        m_component->onMessage(&controlMsg);

    } catch (const std::exception& e) {
        addLog(QString("发送声纳控制命令时发生异常: %1").arg(e.what()));
    } catch (...) {
        addLog("发送声纳控制命令时发生未知异常");
    }
}

// 创建被动声纳初始化数据
CAttr_PassiveSonarComponent MainWindow::createPassiveSonarConfig(int sonarID)
{
    CAttr_PassiveSonarComponent config;
    config.sonarID = sonarID;
    config.sonarMod = 2;  // 被动模式
    config.sfb = 5000;
    config.arrayNumber = 64;
    config.fs = 8000;
    config.SSpeed_range = 30;
    config.Depth_range = 500.0f;
    config.RPhi = 360.0f;
    config.RTheta = 180.0f;
    config.BandRange = 8000;
    config.BandFormingAnum = 72;
    config.BandFormingAdeg = 5.0f;
    config.BandFormingPnum = 18;
    config.BandFormingPdeg = 10.0f;
    config.array_xyz = 0.0f;
    config.array_work = true;
    config.TrackingTime = 30;
    config.Gnf = 20.0f;
    config.DOA_mse_surf = 2.5f;
    config.RelativeBandwidth = 2000;
    config.RelativeTimewidth = 5.0f;
    config.DetectionAlgorithm = 1;
    config.BFAlgorithm = 2;
    config.TrackingNum = 10;

    return config;
}

// 创建连续声数据
CMsg_PropagatedContinuousSoundListStruct MainWindow::createContinuousSoundData()
{
    CMsg_PropagatedContinuousSoundListStruct soundData;

    // 创建基于paste.txt数据的目标
    C_PropagatedContinuousSoundStruct target;
    target.arrivalSideAngle = 19.5438f;
    target.arrivalPitchAngle = 3.2185f;
    target.targetDistance = 914.0f;
    target.platType = 1;

    // 设置频谱数据（基于paste.txt中的真实数据）
    float spectrumData[] = {
        52.50841, 37.919254, 36.897068, 29.2859, 45.48483, 43.50526, 42.062664, 38.1274, 51.17799, 37.512203, 28.545574, 44.764828, 43.689716, 31.169971,
                40.08112, 42.22299, 51.90115, 36.36925, 42.48184, 33.06689, 40.75137, 49.749073, 40.278767, 39.04391, 41.190075, 37.740154, 37.56088, 42.822758,
                54.543293, 57.352, 36.672993, 49.63798, 40.08109, 43.38162, 40.230297, 41.879963, 54.36601, 45.3821, 46.733883, 47.473034, 48.605663, 35.35348,
                40.78937, 46.032253, 35.108463, 57.20457, 34.52171, 36.53748, 50.29858, 37.040867, 34.99332, 40.002224, 48.740795, 35.359943, 39.2963, 34.063595,
                35.84026, 33.834133, 35.0488, 29.045033, 50.389263, 45.96497, 30.242062, 39.57781, 32.755535, 36.385967, 36.669613, 47.028904, 38.012127, 43.83655,
                41.966637, 44.46791, 32.3841, 46.90075, 32.190178, 43.289856, 35.24288, 25.028534, 31.727028, 34.531044, 34.217453, 27.29924, 36.293274, 41.763588,
                44.40615, 40.108345, 43.824905, 46.684692, 32.155296, 35.642784, 38.452362, 43.876884, 47.07904, 60.926872, 49.25039, 41.760452, 46.55358, 29.217308,
                31.517715, 38.077835, 40.07325, 54.258667, 38.911865, 52.213272, 29.998451, 32.1175, 37.01436, 40.361824, 37.79666, 51.095055, 37.389862, 29.983643,
                52.669456, 40.62001, 49.019356, 41.331116, 43.920418, 35.181694, 36.185616, 31.166603, 42.35265, 37.98174, 30.781063, 42.05627, 35.08256, 35.75388,
                40.101208, 30.683697, 37.652782, 32.996952, 35.45978, 49.829052, 26.966854, 39.59663, 39.185535, 24.419033, 27.54063, 25.71011, 24.64365, 30.322315,
                26.904247, 21.736546, 19.790012, 26.803173, 24.386677, 33.062534, 34.788525, 32.042164, 31.88924, 28.333744, 27.403652, 25.345982, 25.488453, 22.94672,
                28.204037, 24.471828, 26.496334, 22.54919, 26.094921, 25.495586, 24.115849, 21.748753, 27.63007, 24.938984, 26.453953, 39.685986, 33.326115, 34.61424,
                35.696644, 29.530857, 43.43556, 25.747189, 25.260288, 24.82188, 30.373463, 23.441113, 24.318127, 21.841503, 26.711544, 42.285908, 37.86388, 32.685688,
                34.0042, 40.13653, 26.180248, 42.026897, 26.976551, 23.78432, 23.21648, 24.16277, 22.193417, 20.726223, 24.935337, 21.53172, 27.42641, 35.375023,
                25.605782, 33.78984, 30.854774, 27.16249, 31.82331, 28.578758, 32.06147, 24.579353, 29.482597, 28.720665, 26.375114, 31.825348, 34.719727, 23.33857,
                22.53991, 26.483856, 37.116104, 24.20617, 45.4747, 25.317833, 33.634514, 31.035347, 37.864555, 25.544106, 33.519714, 33.488335, 25.491165, 26.212593,
                30.315002, 28.632103, 29.371666, 23.530037, 25.516273, 28.34925, 23.718071, 30.473648, 23.258484, 27.862228, 24.035408, 31.302551, 31.129135, 37.79451,
                37.745514, 31.497719, 31.181969, 25.36036, 26.375084, 27.81047, 19.807945, 43.631165, 28.47654, 30.488838, 33.87336, 29.84626, 28.642662, 30.771797,
                24.717598, 33.268364, 25.184097, 24.051178, 25.312958, 27.515526, 23.44606, 23.972893, 22.366676, 25.508278, 29.555023, 24.653648, 23.942802, 21.344604,
                24.038681, 24.87751, 23.043182, 29.75595, 22.843605, 32.63833, 21.202026, 23.030952, 24.228378, 23.458733, 29.319374, 21.95327, 27.232498, 32.706665,
                29.556808, 33.529922, 25.38002, 25.46083, 24.295853, 25.007423, 24.858543, 18.518822, 18.446327, 27.826248, 27.239494, 23.84694, 30.56424, 22.801247,
                21.016243, 26.838516, 23.337234, 22.92881, 20.474861, 20.146774, 31.364517, 31.46148, 22.404457, 25.101135, 26.040306, 22.73735, 25.094429, 21.370865,
                25.48243, 28.318344, 22.260262, 24.65474, 28.517914, 25.052338, 25.827019, 25.363724, 24.036133, 28.746628, 26.581772, 33.694344, 25.8256, 36.141975,
                28.108368, 24.395462, 32.2378, 27.329872, 24.597008, 25.996445, 25.765999, 26.487434, 32.083267, 28.035011, 27.096275, 32.45543, 25.966461, 28.2304,
                23.405006, 29.817818, 27.44777, 30.235512, 27.551231, 27.231743, 24.761436, 24.323036, 28.791756, 28.506119, 26.629868, 30.254875, 26.521164, 26.194603,
                25.55468, 31.001968, 23.41935, 23.5335, 23.53949, 26.712334, 32.058205, 29.928085, 31.120872, 31.051628, 40.440826, 27.37371, 25.635857, 22.556923,
                26.951378, 31.808395, 36.203705, 30.940544, 24.70398, 28.678345, 25.095299, 24.856697, 23.077362, 23.960663, 24.157547, 29.836792, 24.636795,
                23.106827, 25.389809, 23.369835, 16.03608, 28.774895, 25.268997, 31.13562, 27.88787, 20.183815, 16.758926, 29.159332, 17.340042, 18.35025, 16.127617,
                25.91655, 29.76799, 25.49849, 27.315536, 28.445007, 24.994972, 27.13552, 30.048256, 26.489319, 30.989967, 27.432693, 25.877663, 27.746613, 29.213905,
                28.22348, 29.631111, 30.437096, 27.274788, 30.01841, 30.545937, 28.630997, 28.00856, 26.593956, 29.426575, 25.85305, 33.517586, 24.336403, 20.15857,
                23.819977, 27.45507, 26.485588, 26.82891, 30.023346, 26.39112, 21.436165, 28.311424, 31.53357, 26.224533, 26.05925, 34.0643, 25.833405, 28.230438,
                25.13955, 25.60434, 27.804451, 29.04927, 25.072151, 30.885834, 28.296875, 30.575462, 35.289375, 31.11216, 25.994148, 34.282272, 22.44117, 28.86457,
                26.832916, 30.590141, 32.747765, 29.827751, 28.764275, 35.345047, 22.504524, 25.310524, 27.674934, 31.443909, 30.045036, 32.70134, 30.726479,
                30.211922, 21.339188, 33.28724, 30.423561, 33.069153, 32.55153, 28.277351, 17.82454, 30.483261, 33.444237, 31.511642, 21.793388, 32.406754, 32.25676,
                29.12004, 35.492874, 32.844208, 30.56868, 28.060272, 28.734985, 30.416527, 24.924362, 25.07695, 32.832237, 30.281265, 31.81002, 33.681175, 32.86492,
                41.38497, 32.15648, 31.41275, 29.623047, 27.526207, 29.051643, 27.13279, 28.662704, 28.49012, 33.52851, 25.748459, 33.818542, 30.18637, 36.604713,
                27.085197, 33.5599, 38.416687, 35.223534, 31.595612, 34.377533, 34.126465, 30.233551, 30.623413, 29.467705, 37.75141, 29.995697, 32.870125, 33.431946,
                38.212997, 33.660553, 31.595497, 32.90702, 30.207008, 35.336166, 38.05915, 37.302383, 35.434547, 35.044876, 37.506577, 30.907463, 31.114761, 26.98603,
                40.062286, 34.782715, 29.992134, 31.686531, 33.316925, 34.669502, 28.12442, 29.037537, 32.81868, 28.587135, 37.78675, 35.13788, 34.181274, 28.736664,
                31.584297, 33.546677, 32.77655, 37.427895, 31.17118, 36.860855, 35.19803, 28.32196, 34.49372, 32.298576, 30.40393, 24.601982, 32.81218, 32.327232,
                30.699432, 31.740616, 31.666893, 29.780136, 29.858345, 28.880714, 27.639618, 29.157776, 35.11992, 33.585815, 28.817421, 27.671478, 31.797256, 35.163513,
                28.49424, 28.421013, 33.312874, 30.784515, 34.884773, 27.106209, 32.144753, 30.63372, 35.83716, 32.337868, 30.302803, 26.822403, 30.14196, 33.01243,
                35.000168, 31.45794, 33.142654, 28.14289, 30.403374, 34.614616, 31.075058, 31.42479, 26.364212, 35.02925, 28.333359, 38.491592, 33.43203, 24.282448,
                30.14804, 30.842537, 29.589157, 31.722221, 26.032616, 24.158401, 30.33052, 29.437141, 31.11686, 30.850563, 37.232872, 34.30175, 34.926277, 36.28997,
                31.35743, 27.149452, 31.810265, 36.43815, 33.948303, 30.09298, 30.692894, 33.517754, 36.492622, 35.59957, 27.352974, 32.304184, 34.057014, 23.52024,
                37.3244, 30.205917, 30.421173, 30.126945, 30.99556, 24.935387, 34.699326, 37.590485, 38.90281, 32.240517, 30.339104, 29.577965, 32.9459, 31.79004,
                33.040512, 32.409233, 30.723274, 34.21945, 33.033722, 29.794838, 30.598244, 37.01967, 31.410995, 30.01184, 36.461983, 32.82258, 29.770897,
                28.121513, 35.09285, 29.310585, 30.077133, 28.9581, 34.665756, 30.395798, 29.57373, 31.10859, 38.210564, 30.52864, 36.16091, 25.9105,
                30.840164, 31.436142, 30.681633, 24.33554, 31.208122, 30.500069, 28.584229, 24.832512, 33.127228, 30.429497, 33.03267, 30.029945, 28.714584,
                27.7659, 32.094208, 33.536125, 23.914436, 25.778763, 28.082085, 25.47126, 36.202423, 27.826591, 34.4629, 30.906181, 35.130028, 31.781784,
                31.007622, 31.590172, 30.711922,35.543816, 36.79522, 35.57959, 33.589386, 34.59268, 27.972694, 33.09192, 27.858017, 38.505257, 30.69207,
                26.659172, 25.468735, 30.69133, 34.875015, 31.566025, 29.903168, 32.131973, 37.122215, 31.488808, 36.908966, 28.373611, 30.589714, 32.15291,
                31.854889, 27.44638, 32.112686, 30.074783, 29.350632, 26.838783, 30.723167, 24.228539, 33.695526, 34.0226, 29.469604, 29.992325, 33.605217,
                32.928635, 25.212158, 35.76889, 22.91668, 34.186424, 23.79969, 27.635185, 29.260902, 26.790016, 30.085907, 35.14676, 27.46505, 37.039967,
                27.031162, 24.783878, 23.848919, 23.19646, 24.741512, 21.67603, 27.134617, 31.37576, 36.85996, 36.27605, 22.224667, 26.844181, 28.94077,
                23.302029, 24.707561, 28.689503, 30.89674, 37.788, 34.035007, 29.530544, 27.036732, 25.499653, 28.370655, 29.872524, 24.474888, 23.481716,
                34.564007, 25.806904, 28.69357, 36.00764, 26.679867, 27.021107, 27.126293, 30.263073, 29.479809, 24.874142, 26.90755, 30.40717, 28.79948,
                27.018253, 28.52666, 27.51315, 33.848675, 29.311031, 30.108318, 22.092297, 29.685154, 24.567204, 27.877491, 27.873638, 38.02283, 23.064968,
                28.941364, 21.413937, 26.209576, 34.998005, 33.7728, 30.76083, 25.782215, 30.463627, 29.236996, 25.687656, 30.012936, 24.07063, 33.57662,
                36.054104, 33.77963, 29.881763, 25.017284, 21.1796, 32.488636, 20.954144, 30.535198, 32.094784, 34.11973, 29.761196, 26.370312, 23.454456,
                29.059391, 29.976566, 33.70492, 29.999294, 25.51192, 27.199108, 28.037266, 22.270557, 21.910305, 32.369587, 23.656887, 25.058231, 29.264332,
                30.242321, 32.960102, 23.797466, 27.628284, 28.137379, 29.143711, 30.573689, 25.434086, 26.668476, 27.342625, 31.519115, 26.569767, 21.22681,
                24.699848, 27.912838, 27.903461, 22.921978, 23.74052, 34.630642, 35.721905, 35.099545, 28.116726, 22.88319, 24.642086, 23.614063, 33.53541,
                32.468052, 31.197323, 34.6767, 23.072544, 24.18021, 33.661297, 25.764317, 33.319294, 24.151844, 28.2934, 35.839, 30.632435, 20.04816,
                27.085201, 28.075909, 28.244411, 33.33209, 29.825375, 32.296963, 27.412754, 32.6702, 33.154728, 30.04871, 30.727047, 23.147732, 35.08208,
                30.250034, 33.499714, 21.495098, 28.065807, 23.554913, 25.372608, 34.67097, 27.44617, 26.200146, 30.244167, 21.964474, 27.831623, 27.24805,
                36.53867, 24.52406, 25.296833, 26.82336, 22.69006, 29.112133, 27.494709, 28.939335, 30.253017, 36.61085, 24.168293, 29.589657, 24.744411,
                26.32771, 29.55766, 22.82986, 26.143505, 28.482838, 32.890537, 26.82874, 32.842815, 24.118221, 25.341824, 33.364437, 23.11961, 35.338284,
                31.58308, 31.624409, 32.52181, 30.1888, 37.261707, 29.884281, 27.133419, 35.124454, 22.287579, 26.33939, 32.749737, 34.93503, 30.133259,
                22.27155, 23.996181, 27.05038, 20.965, 30.32143, 25.085995, 21.174297, 31.41911, 33.22401, 29.410557, 34.037373, 26.28999, 32.135014,
                28.85905, 26.599545, 26.443691, 24.807812, 32.567562, 27.299175, 17.883305, 30.234005, 29.496456, 32.663967, 34.503498, 25.766102,
                33.192356, 23.709766, 22.016582, 29.904072, 32.195347, 24.288326, 25.875515, 27.070988, 31.858105, 29.524906, 21.513836, 28.44418,
                25.675701, 31.848515, 27.87473, 27.746265, 28.576382, 24.413418, 19.540318, 28.720615, 20.867939, 29.685406, 19.554516, 29.335903,
                31.152859,  22.854435, 23.015003, 31.411747, 33.790585, 25.25121, 34.491573, 30.440174, 25.22395, 26.20374, 32.114033, 31.41624,
                20.13013, 27.350918, 34.437, 26.029469, 27.8457, 33.505566, 25.87611, 31.810375, 28.915638, 32.870487, 23.372318, 31.378445, 31.477268,
                26.58559, 30.710934, 22.379025, 20.271885, 26.2205, 30.748745, 21.994656, 27.936481, 26.032795, 28.643703, 23.64606, 24.209988, 29.235935, 22.164776, 26.473667, 29.36562, 18.717785, 24.57938,
                31.015453, 26.466076, 28.570713, 36.64104, 33.98077, 26.463474, 22.767422, 27.284473, 17.658688, 30.16167, 25.870098, 30.925533, 27.583118,
                28.326572, 22.497425, 24.560078, 27.898716, 29.769024, 28.329182, 28.418636, 29.444195, 27.743359, 27.012531, 29.359013, 24.990208, 28.07204,
                28.571392, 33.651203, 21.377605, 29.042233, 27.074116, 29.442196, 25.044216, 28.673702, 22.977146, 27.45065, 26.27422, 26.859615, 22.808071,
                28.571743, 30.07214, 31.612598, 23.716839, 26.525814, 22.330479, 29.480465, 20.810177, 27.584873, 32.625736, 26.20927, 33.2971, 27.024014,
                23.290714, 27.759533, 24.080578, 30.556011, 20.700687, 23.906109, 20.273327, 24.956013, 32.880146, 29.614391, 24.880688, 27.813633, 22.687984,
                31.037746, 29.138096, 26.487225, 29.374538, 20.714916, 23.951794, 31.388424, 21.276257, 24.37881, 29.000584, 24.497005, 26.926075, 22.70766,
                30.828846, 33.313046, 21.727627, 23.553951, 25.075886, 28.059467, 29.615948, 20.639118, 22.641674, 31.330807, 17.70338, 29.321255, 26.147892,
                26.398853, 22.130589, 26.246235, 22.06741, 25.277874, 24.386364, 20.04797, 23.46136, 26.035213, 26.752186, 28.266544, 25.990124, 26.135654,
                28.32407, 21.548534, 34.000484, 27.518742, 15.859402, 26.174816, 16.337719, 26.271175, 32.176792, 24.129688, 25.191296, 23.896435, 28.111599,
                30.058697, 25.958065, 32.55109, 33.165257, 33.245792, 25.911594, 22.111568, 34.244785, 22.746784, 23.517735, 23.885197, 30.939442, 31.909527,
                27.312199, 29.025906, 21.2625, 19.631138, 24.43996, 22.947292, 34.027294, 17.435635, 20.32259, 27.19379, 30.718212, 25.63068, 31.423656,
                24.593838, 27.778522, 26.411732, 28.091076, 23.805058, 19.982075, 28.081959, 32.111988, 24.721317, 25.522121, 19.575924, 22.382687, 28.915997, 15.824177, 25.116276, 23.444347, 28.365208, 22.394344, 25.455769, 26.34444, 22.541012, 25.280224, 20.452328, 27.609531, 32.271656, 26.893124, 23.731815, 24.89072, 24.833675, 21.95023, 27.51096, 18.009762, 18.482418, 25.91949, 23.130085, 25.741642, 29.989979, 22.330883, 28.588673, 34.946217, 20.468365, 19.364796, 26.209316, 17.46088, 26.139835, 24.50941, 24.723652, 27.661892, 34.389133, 28.67844, 18.530216, 23.79964, 33.29617, 31.03049, 32.39533, 18.30862, 16.428753, 27.410236, 21.144642, 30.820225, 28.524418, 30.651623, 33.73492, 32.839146, 27.924107,
                27.505573, 24.86694, 29.79034, 34.458782, 21.849297, 28.552147, 23.164513, 23.974777, 28.46862, 21.147202, 25.348236, 34.32833, 26.197502, 32.201096, 24.462883, 35.221413, 20.958557, 31.836777, 19.765053, 22.556152, 23.514091, 24.269958,
                24.330147, 26.821175, 26.04168, 22.044449, 31.594475, 16.983177, 23.074951, 25.99923, 23.782494, 29.741379, 33.059746, 19.191803, 20.360512, 30.141006, 24.95272, 18.840515, 32.630722, 19.703445, 30.33268, 27.275764, 30.798027, 22.730415, 30.213242, 29.829117, 20.882362, 25.521591, 28.738152, 29.349571, 24.363861, 16.522568, 31.044395, 30.505005, 28.226555, 29.537582, 34.494568, 23.972206, 32.48346, 20.818138, 28.586006, 21.954758, 17.136826, 27.680641, 18.361557, 26.370026, 27.13491, 14.734215, 28.457794, 23.39067, 19.050758, 23.154762, 24.447083, 34.466904, 21.072838, 23.408333, 28.008392, 22.481827, 28.684113, 25.227173, 16.164139, 17.188408, 19.831604, 27.41581, 31.735435, 34.26986, 30.1139, 15.211861,
                30.286163, 23.355225, 18.090195, 21.844208, 19.498215, 21.663261, 25.12313, 29.756851, 31.551674, 15.318672, 26.246155, 28.942154, 22.533104, 23.860062, 33.568306, 22.658531, 26.928246, 24.516327, 18.8378, 24.45237, 27.382805, 17.684402, 31.397278, 30.757187, 19.814735, 18.8097, 24.63736, 27.586884, 20.205368, 36.19169, 20.59243, 18.534554, 26.858345, 17.772278, 32.3832, 27.953705, 16.58323, 29.46759, 17.828026, 27.179466, 27.353073, 28.014435, 26.365356, 17.79055, 32.509186, 30.67118, 24.231873, 21.647629, 28.88575, 18.538849, 22.319344, 27.239853,
                27.767296, 24.53595, 23.237251, 26.231323, 18.52716, 25.217476, 24.295082, 13.948486, 23.468826, 24.582954, 19.879997, 25.879166, 27.168343, 23.065178, 25.314842, 13.767601, 19.022697, 25.47474, 25.08419, 22.556015, 14.949013, 17.542679, 18.701385, 15.51638, 29.343452, 15.237724, 22.254768, 22.376778, 15.244675, 13.472755, 12.146866, 19.604118, 18.581093, 17.774063, 23.023178, 29.2678, 30.252594, 21.287651, 29.12455, 14.178741, 19.614891, 32.982307, 15.494263, 20.654099, 23.43303, 26.830055, 26.722168, 19.241646, 16.019066, 23.760208, 27.824982, 21.461357, 19.881012, 26.215347, 21.041405, 30.78698, 23.49849, 33.4626, 19.195496, 19.935654, 14.904701, 28.124908, 30.651207, 24.777939, 22.416107, 20.869377, 14.996529, 18.322525, 17.165985, 21.341156, 21.543633, 26.716263, 19.077156, 23.013329, 31.396294, 18.570084, 26.586067, 28.290367, 29.95591, 29.17965, 24.941978, 23.38713, 23.417252, 29.565025, 25.584877, 25.18692, 23.047966, 19.402573, 23.776863, 29.394432, 31.380188, 24.670448, 22.956436, 24.049469, 19.655289, 17.817879, 24.95298, 30.106468, 23.647377, 20.209412, 23.637016, 27.91066, 15.45472, 15.069168, 18.625526, 19.06665, 17.405968, 21.542915, 18.495148, 29.738571, 24.607445, 21.481422, 32.35527, 26.961288, 22.858414, 29.258324, 19.187485, 25.581299, 16.101555, 20.619728, 22.461021, 23.168304, 28.04702, 22.42241, 31.24118, 26.779709, 31.919044, 27.125679, 24.611275, 31.495155, 21.660973, 25.807388, 22.229881, 27.947845, 28.282188, 28.616081, 26.192688, 18.558266, 21.82891, 17.149399, 21.410774, 25.466377, 16.845505, 23.771278, 18.435722, 28.075539, 27.778702, 26.866539,
                16.650993, 30.60846, 24.14647, 29.08664, 28.867294, 25.498116, 25.36229, 18.154442, 32.683838, 22.00312, 28.752068, 19.681717, 22.957123, 24.735481, 19.539856, 21.784622, 25.117294, 26.588058, 23.226593, 18.927612, 22.133781, 23.116402,
                20.944489, 28.362442, 21.85521, 23.850273, 17.55474, 19.9048, 27.40136, 16.100159, 16.926064, 25.526314, 22.559631, 22.68325, 27.73391, 16.478424, 19.033035, 31.048615, 25.62162, 23.001976, 17.762772, 27.920113, 25.164017, 16.056702, 27.655075, 29.492699, 22.2779, 20.287697, 21.12436, 28.857605, 18.52391, 29.535194, 24.433243, 30.114357, 27.561333, 22.308723, 17.391022, 28.130981, 30.96933, 31.663239, 18.19574, 31.307465, 15.900742, 15.833694, 21.941055, 27.53756, 26.655655, 24.86383, 28.484383, 19.392738, 23.620842, 18.648361, 30.850601, 20.202354, 17.143852, 25.929268, 21.681213, 29.825577, 28.444954, 30.183678, 25.735153, 22.716019, 17.752037, 18.132706, 30.809166, 17.68444, 19.162659, 23.819557, 31.988716, 20.240608, 28.322388, 22.412987, 27.55265, 29.66101, 29.857521, 20.042107, 22.370445, 25.06491, 27.47499, 19.846626,
                19.476578, 31.104988, 24.593575, 25.848244, 15.944534, 28.119469, 23.9114, 25.131126, 28.301666, 31.213966, 21.133308, 25.3815, 17.394058, 29.468956,
                16.208153, 17.465515, 27.735054, 22.198235, 30.285782, 17.547058, 20.271317, 30.129372, 26.095123, 28.518799, 24.958649, 21.292137, 23.894005, 23.214172, 24.10437, 26.288078, 30.90635, 15.955826, 22.144897, 27.801178, 28.825264, 22.357086, 27.924835, 26.330856, 30.856552, 29.196045, 24.194244, 20.16526, 19.464989, 15.634201, 28.799538, 26.884193, 20.781723, 23.608528, 29.443222, 23.294533, 21.127998, 24.21402, 21.98014, 18.679016, 27.22715, 21.662819, 25.600403, 23.246971, 21.62983, 23.52938, 21.385384, 25.263184, 18.160255, 25.131683, 28.288628, 27.363388, 21.083847, 29.95665, 25.866287, 22.467308, 18.699394, 29.031784, 27.951561, 14.930672, 30.862518, 17.366844, 29.478157, 30.004318, 30.378319, 30.255531, 19.187508, 30.75734, 30.991753, 31.445465, 31.61686, 28.794495, 18.844238, 21.241402, 30.306412, 15.434357, 18.432953, 21.271591, 28.34336, 20.470718, 26.555603, 22.571, 31.01265, 28.265526, 18.675194, 29.322624, 29.217033, 28.91497, 24.71212, 23.786057, 16.323631, 29.298752, 31.402893, 15.841476, 17.673073, 26.88807, 24.529587, 27.174957, 15.4045105, 30.16304, 22.659859, 16.547867, 30.932018, 23.672718, 22.554768, 17.468372, 22.41293, 29.340992, 26.913723, 27.609913, 29.827457, 18.049664, 24.225552, 29.535473, 20.542248, 20.586544, 24.629292, 30.124195, 15.529003, 29.140644, 26.464626, 16.971584, 29.991245, 23.328365, 19.89368, 29.306637, 27.385227, 24.561626, 27.276424, 20.907185, 25.500912, 18.693287, 24.255573, 20.92316, 26.652355, 19.952122, 17.487133, 21.388508, 19.876385, 21.62611, 19.875057, 23.023663, 30.636097, 16.161686, 15.501003, 19.937077, 20.001759, 22.425373, 21.568623, 28.975674, 19.76662, 18.920078, 27.492886, 28.061123,
                17.024563, 17.269932, 21.753147, 29.100513, 19.196903, 30.882671, 16.650524, 18.859959, 29.784344, 22.901188, 16.195515, 20.161572, 18.536121, 27.727299,
                23.63786, 21.077251, 27.272747, 27.517086, 26.632732, 19.3344, 20.624874, 20.348156, 21.003155, 14.276394, 26.373806, 23.741276, 28.95956, 19.472805, 24.651188, 23.04697, 26.499134,
                19.41682, 28.456532, 22.169132, 30.257084, 20.538692, 17.666737, 24.871296, 21.514935, 17.765453, 16.485264, 24.71867, 21.263325, 24.668934, 23.757572, 15.377491, 26.884022,
                23.490452, 16.901592, 17.605122, 14.256496, 24.15939, 25.649296, 21.085514, 30.396397, 27.536427, 19.842503, 14.576275, 28.664783, 24.161327, 14.867313, 17.601238, 24.64915, 20.115803, 23.105885, 26.346325, 23.362362, 29.423779, 21.015339, 15.95573, 15.962566, 24.151516, 22.001614, 23.48946, 23.41988, 16.079906, 20.636616, 21.145199, 15.936901, 14.642872, 24.389309, 27.064548, 15.4450035, 20.736675, 17.073048, 30.475712, 19.12675, 21.116962, 27.378994, 20.62524, 21.41042, 20.046307, 26.770893, 18.629185, 25.44772, 21.49133, 27.837925, 23.265759, 17.638203, 15.015217, 28.852024, 15.628277, 22.745869, 14.650295, 23.526073, 24.744114, 16.678371, 22.046787,
                13.926456, 24.887417, 23.107632, 30.020435, 15.608669, 14.890797, 24.431973, 15.228428, 22.600483, 22.470806, 21.391712, 22.796589, 23.486012, 30.366833, 28.419155, 16.123226, 20.492031, 24.565723, 22.4827, 13.947346, 28.542255, 23.778568, 20.121609, 21.036648, 24.590282, 19.094082, 18.802525, 16.632946, 18.84357, 18.846447, 23.206982, 17.695965, 13.712803, 15.6518135, 21.854763, 25.410374, 25.73524, 26.545048, 28.059269, 27.150349, 17.04662, 18.432308, 21.708408, 17.160809, 17.700932, 18.92944, 14.942654, 15.655766, 28.804272, 16.094296, 20.605785, 29.872097, 18.535297, 17.518803, 27.855846, 29.985706, 13.586277, 15.893024, 19.158596, 17.257908, 24.82671,
                16.490269, 17.417744, 23.84203, 24.001163, 16.45831, 26.148869, 23.394718, 15.031101, 15.699932, 29.681324, 18.75977, 23.112041, 26.583324, 27.068409, 17.881527, 25.173977, 22.477238, 16.48631, 17.197903, 17.977192, 20.348064, 20.115353, 18.680637, 20.964733, 26.21426, 27.177738, 19.518475, 15.4426155, 13.975811, 26.183582, 20.115917, 20.28783, 19.216007, 28.356098, 22.35033, 20.485859, 20.11021, 19.744884, 26.663296, 13.932018, 15.33485, 18.289394, 22.688908, 19.877232, 18.39835, 25.028095, 17.150219, 26.413342, 23.882153, 25.005337, 23.409817, 15.832394, 17.49718, 19.72694, 26.202808, 13.140537, 20.985172, 22.325237, 23.048092, 21.212818, 18.123936, 19.914326, 27.602734, 20.833424, 25.667614, 23.168743, 19.219067, 19.390759, 29.698917, 26.457455, 20.454609, 27.255268, 19.088718, 14.406269, 24.521229, 15.031307, 23.872395, 19.077488, 18.763584, 20.212246, 17.07307, 27.039791, 25.679852, 17.367062, 21.838024, 24.671917, 17.717762, 27.641987, 26.559643, 24.69395, 14.640362, 20.249538, 23.876614, 17.37421, 26.807056, 21.587063, 29.788235, 15.668125, 15.492786, 13.189312, 22.469791, 26.352383, 13.665348, 18.317081, 18.76907, 24.22665, 27.993984, 18.833454, 29.045696, 29.712177, 21.089, 21.662663, 24.565556, 24.090878, 23.002552, 19.34106, 19.329441, 24.666042, 19.28107, 27.510052, 13.396534, 13.024982, 18.757122, 24.263828, 29.134998, 26.259968, 22.597042, 24.903591, 28.9651, 17.182682, 16.266819, 15.66489, 19.028965,
                27.372433, 17.246029, 22.408894, 15.7458, 18.375866, 14.823467, 23.948498, 22.323269, 20.926777, 16.314098, 22.040714, 13.088604, 28.409191, 14.928043, 25.66994, 29.278645, 25.42992, 18.266285, 25.10152, 19.459698, 27.105602, 25.317142, 19.680042, 13.645992, 16.134594, 26.894665, 27.605656, 27.90849, 18.845753, 18.353642, 23.987957, 13.909298, 19.760044, 26.770695, 15.314968, 16.108051, 27.185352, 26.626957, 22.262821, 13.682583, 19.022175, 13.261326, 19.416187, 17.088963, 21.524708, 29.074146, 22.480724, 13.751934, 15.307865, 15.2915535,
                15.274792, 27.640583, 14.991695, 16.185474, 21.521236, 25.358402, 19.104488, 22.96162, 14.384441, 19.820042, 16.870426, 21.079937, 29.231548, 16.769611, 25.885357, 19.280521, 24.862568, 18.166576, 12.533123, 16.318333, 24.438465, 27.332745, 28.65347, 13.742092, 26.00565, 15.595776, 18.809254, 25.050053, 14.824184, 20.935238, 16.052174, 24.647976, 24.072819, 20.670315, 20.607563, 21.586956, 13.211155, 26.575413, 18.204723, 17.464962, 28.892056, 19.496456, 22.40501, 20.243664, 24.014416, 21.67316, 22.679592, 25.713818, 24.453152, 13.565464, 22.123997, 20.782894, 20.667599, 26.95271, 13.099514,
                28.971226, 17.620525, 27.495396, 20.307491, 21.85146, 26.439404, 21.001026, 19.50562, 17.254162, 17.220257, 21.166248, 18.64278, 15.112736, 20.484516, 27.712933, 18.179928, 22.006908, 14.302433, 14.269787, 20.398273, 25.435047, 20.434788, 22.930233, 17.608463, 28.755192, 28.65755, 15.164852, 15.327847, 17.208256, 24.361027, 27.649914, 28.54932, 20.058285, 17.763226, 13.926731, 21.368954, 21.698513, 20.092861, 17.301235, 15.043674, 20.260006, 25.954315, 23.169113, 22.024109, 23.662476, 21.4758, 19.287582, 20.53344, 23.505707, 14.744774, 23.1531, 25.071365, 11.7660675, 19.639145, 28.286186, 19.130852, 22.411118, 25.037338, 22.863876, 18.139198, 21.289139, 13.933716, 18.025558, 24.964302, 15.811363, 26.506714, 17.122253, 11.729179, 18.10624, 20.49511, 14.616623, 20.307014, 23.132843, 23.330017, 22.717285, 21.323158, 27.65715, 15.884972, 11.599121, 22.817879, 19.091377, 14.269623, 19.384384, 25.221016, 15.471855, 13.2561035, 16.661201, 24.911446, 15.152336, 25.426964, 15.596687, 23.976074, 12.140785, 14.933807, 17.042892, 13.995071, 23.804855, 20.131516, 27.066292, 20.199669, 25.874504, 22.967339, 16.04609, 23.184494, 20.404045, 23.404457, 14.555893, 21.066788, 19.789597, 25.892654, 26.930626, 14.725914, 26.093414, 24.31031, 14.915749, 13.900551, 27.311905, 24.144386, 26.550964, 12.251175, 20.1446, 25.432045, 22.344345, 26.993332, 18.296455, 16.194565, 24.96901, 26.752792, 24.66378, 22.498116, 19.951408, 23.852898,
                14.488518, 12.692627, 15.728378, 25.807983, 21.39164, 26.535637, 17.637482, 17.032486, 18.080238, 19.319878, 27.261024, 27.04567, 15.905441, 24.896622, 17.944756, 26.764954, 20.832626, 27.309952, 14.407303, 16.15567, 24.180359, 14.235725, 12.893288, 26.63749, 11.228592, 19.131142, 18.177856, 14.374611, 15.100067, 21.242027, 21.037903, 17.421776, 20.710281, 12.253319, 11.385963, 12.842186, 13.27356, 21.38118, 15.006355, 24.196754, 21.393478, 15.035683, 27.799294, 25.10418, 13.654854, 17.043846, 11.919296, 21.324097, 25.462837, 17.25869, 27.20578, 17.391449, 14.07341, 18.548645, 13.63678, 13.960587, 21.126686, 27.723427, 17.875916, 23.016632, 16.026527, 20.234344, 16.40258, 17.731651,
                19.966423, 21.123444, 13.006004, 27.659737, 21.360954, 23.358055, 24.659866, 12.976364, 22.04055, 14.857529, 19.532928, 12.362541, 20.443817, 15.45816, 16.729721, 25.583397, 14.509232, 24.21196, 23.33908, 18.45987, 27.309105, 11.502975, 20.92604, 17.783379, 26.961098, 14.808502, 18.592667, 26.464905, 24.044258, 18.5149, 26.770363, 15.854698, 14.883354, 18.897331, 22.454956, 27.279648, 22.37201, 25.992737, 20.601135, 18.829483, 15.063332, 15.405182, 20.203972, 11.37278, 14.48951, 26.293152, 26.160606, 18.870926, 12.895042, 12.2491, 19.066689, 13.402733, 22.064331, 23.63836, 13.159935, 14.095413, 24.084198, 13.684204, 17.33944, 15.0670395, 27.067764, 17.577446, 23.96019, 18.3815,
                27.10852, 18.538086, 15.4081955, 15.049767, 21.964119, 15.926147, 17.254517, 15.141006, 27.369972, 11.751312, 15.599037, 26.773262, 21.18509, 17.464737, 14.111046, 12.784317, 22.661888, 22.980621, 27.195328, 18.433876, 26.49656, 19.444344, 20.987335, 21.729065, 13.454033, 21.342728, 25.637505, 26.537804, 14.698456, 21.113838, 27.14431, 24.656471, 23.82045, 16.94757, 10.858795, 21.699509, 15.883316, 14.662216, 26.681961, 25.581093, 22.426064, 12.035225, 18.837418, 17.208817, 16.276695, 12.261444, 26.418709, 19.125427, 16.084213, 22.354897, 18.396011, 25.962578, 15.34893, 16.14415, 15.674568, 25.436195, 17.417725, 13.3179245, 13.806595, 11.938408, 12.117233, 14.738701, 26.034378, 14.387604, 23.39457, 18.932854, 25.749779, 16.780273, 25.390022, 12.593178, 24.547256, 17.457191, 26.265015, 15.733002, 15.842865, 27.009056, 13.605362, 13.239067, 13.839653, 12.622269, 19.125572, 13.324341, 13.335373, 15.93251, 16.657158, 25.974861, 24.600113, 12.406837, 24.70079, 16.663528, 12.289497, 15.602463, 20.859352, 12.6754, 25.175224, 18.0411, 23.241112, 21.437988, 13.997231, 15.656738, 17.97902,
                22.86718, 25.296883, 12.343506, 23.891144, 11.963158, 16.903915, 15.836266, 22.795273, 12.840477, 26.99337, 24.517502, 23.705925, 18.464493, 11.024345, 13.867889, 15.457428, 21.706787, 23.386276, 17.316353, 14.037041, 11.309074, 16.574387, 25.004341, 20.69004, 22.700562, 14.876198, 13.6276245, 26.701797, 16.880028, 14.230789, 27.150452, 17.257797, 27.237808, 25.238716, 21.227089, 20.12664, 12.662582, 25.980965, 15.538506, 26.116386, 17.74585, 21.848015, 22.71428, 24.801224, 14.066315, 25.95108, 26.787064, 13.609688, 13.386162, 24.912537, 23.450272, 20.09729, 24.67952, 23.104828, 11.973961, 11.584091, 17.571709, 23.859535, 13.994026, 21.080399, 15.390068, 19.662666, 25.217674, 16.872963, 23.467056, 25.958359, 16.110329, 19.487755, 19.73349, 14.516754, 18.51104, 18.821556, 20.43357, 15.628105, 17.785324, 19.517525, 19.3275, 14.364487, 24.52427, 18.389572, 23.842888, 17.173302, 13.759567, 18.963287, 13.964325, 11.349571, 26.739975, 14.213638, 14.321136, 24.179901, 24.543541, 10.9366455, 17.81459, 11.989609, 26.928665, 20.436867, 18.104889, 13.196342, 12.181641, 19.287506,
                25.194649, 18.46933, 17.007164, 16.840332, 26.811783, 11.089531, 17.895248, 13.903275, 21.367073, 24.492401, 19.591934, 22.919167, 21.644104, 19.87165, 16.2249, 13.476234, 14.91169, 17.168709, 13.09156, 18.535767, 19.212288, 15.493698, 25.68451, 14.1701355, 15.127075, 23.36898,
                12.018158, 19.716537, 25.098808, 24.553078, 13.314484, 21.815338, 12.417076, 19.792053, 18.642937, 11.831512, 17.97113, 20.87796, 15.956116, 15.788185, 12.690201, 23.64006, 16.508553, 20.086754, 12.336464, 23.297188, 21.293968, 22.694145, 23.79087, 23.218498, 19.509193,
                24.46566, 19.204903, 24.718803, 22.57772, 23.668747, 17.908173, 26.767792, 10.6638565, 15.502182, 21.767258, 23.7696, 19.740501, 26.542206, 23.801498, 16.830551, 22.730133, 18.37548, 17.558846, 13.308029, 11.45417, 15.253212, 13.635628, 25.554451, 23.126526, 16.311852, 14.05983, 15.229927, 25.063217, 24.321167, 16.640976, 20.169685, 17.931427, 24.188446, 23.13321, 16.238655, 15.337532, 23.573288, 16.388847, 23.183914, 19.219238, 18.957085, 18.820595, 16.47799, 23.391106, 13.462296, 13.541473, 21.99965, 11.403442, 20.8871, 16.743706, 14.756485, 18.729286, 24.95375, 22.162537, 24.867905, 20.98645, 16.902832, 15.105362, 16.371468, 23.75357, 16.800735, 17.424263, 20.480316, 21.303345, 13.248795, 13.944199, 21.233925, 20.948433, 19.831917, 16.12014, 18.99208, 15.045853, 14.512764, 23.26429, 24.75547, 20.92321, 20.143356, 10.8628235, 12.876732, 21.485695, 25.26072, 18.69793, 24.78064, 10.738495, 17.071022, 15.034256, 18.725266, 21.640038, 15.616753, 9.956718, 11.705322, 17.640816, 16.221275, 13.015724, 23.602287, 23.555573, 12.207809, 21.065475, 11.14299, 10.013115, 22.614136, 25.815582, 17.061974, 15.6427765, 12.337997, 20.113289, 16.026848, 12.959511, 18.049965, 22.84864, 15.326569, 14.734932, 9.863388, 11.653366, 20.578407, 26.266144, 15.811295, 26.064201, 12.110672, 22.423126, 22.416061, 17.506165, 14.485115, 19.922432, 14.717041, 20.127342, 11.913658, 26.359627, 16.008232, 19.583954, 25.858337, 11.958763, 19.127396, 25.9059, 21.896667, 21.487503, 25.758614, 21.362396, 12.185806, 17.415237, 19.403305, 14.163002, 12.083351, 21.371597, 11.692421, 14.1132965, 26.263397, 11.628685, 15.238655, 20.722153, 20.62558, 24.459686, 14.559692, 22.567139, 24.19963, 17.126442, 23.392982, 12.771362, 23.38958, 9.6021805, 20.903069, 23.432533, 21.73066, 16.846748, 9.6163025, 13.1707, 11.875244, 19.662231, 24.939316, 20.504982, 14.189186,
                24.276947, 20.770424, 16.29284, 19.713692, 14.700775, 18.653984, 25.457176, 16.729507, 19.129562, 20.289993, 16.367813, 24.071518, 21.632019, 9.501617, 17.165268, 19.770096, 11.34716, 9.916306, 11.099495, 19.159477, 23.064445, 24.099556, 23.597946, 18.957054, 25.075188, 17.47554, 24.065918, 21.209892, 9.623299, 14.524406, 25.703468, 12.13446, 24.20028, 15.540169, 14.3487625, 20.757774, 24.478668, 9.472115, 16.961647, 12.199539, 14.674347, 15.376587, 22.12001, 17.247787, 17.020714, 14.128578, 12.343254, 22.805077, 22.749794, 13.349045, 16.41188, 25.119759, 21.343552, 12.602165, 25.10572, 15.63427, 12.488014, 19.029251, 19.981232, 16.817436, 22.243034, 11.288719, 25.220673, 11.220436, 12.479172, 15.799194, 20.345345, 18.685135, 16.671638, 10.571808, 21.357918, 24.31118, 22.234306, 19.104485, 13.277908, 19.993073, 12.845261, 14.015015, 19.904503, 13.669067, 25.466309, 21.357338, 15.192619, 13.881935, 9.394043, 22.472939, 25.75087, 11.588966, 17.018051, 10.2714615, 17.810081, 20.945763, 22.507149, 21.430496, 20.171616, 25.68113, 13.4333725, 16.87513, 11.370918, 24.686768, 25.978561, 15.760536, 9.488724, 23.891777, 14.4144745, 23.678192, 23.49208, 20.736298, 24.396378, 23.853432, 23.305695, 22.660637, 18.473701, 21.813812, 24.558525, 16.76294, 10.664146, 25.019608, 19.362389, 14.1390915, 14.849037, 17.696762, 22.405785, 12.117355, 18.6856, 15.872795, 11.752136, 24.664383, 11.744606, 15.713165, 12.009277, 21.557167, 25.761162, 9.580688,
                24.307472, 14.766159, 23.660477, 25.247215, 18.511757, 11.911819, 18.710945, 17.17234, 9.395142, 16.813614, 18.090752, 17.9104, 24.184029, 12.431938, 23.412048, 12.30127, 14.902306, 10.934998, 11.660088, 12.808998, 22.669266, 17.14177, 17.409592, 20.92791, 11.248253, 17.992004, 17.737793,
                25.211761, 12.29953, 17.97821, 23.11718, 20.334381, 17.996727, 12.353432, 11.093712, 11.015755, 19.743935, 12.309074, 19.86502, 12.021988, 24.941422, 15.959106, 19.444687, 12.034935, 19.028046, 18.981575, 9.386871, 25.477287, 18.725258, 19.503128, 14.2808, 14.323204, 17.172623, 21.951202, 23.174385, 24.550072, 21.513489, 23.341316, 17.53035, 20.18238, 12.594978, 13.2417145, 19.663208, 18.896027, 24.141602, 24.35656, 16.65989, 16.772987, 11.678246, 11.7487335, 22.86248, 12.4027405, 22.687523, 20.24736, 13.624306, 21.712563, 19.961082, 12.121422, 24.910995, 25.197754, 13.172882, 13.426064, 19.391716, 12.708435, 21.786537, 11.126976, 25.378174, 16.839836, 16.228882, 12.856987, 21.634323, 20.48452, 17.409897, 15.932732, 10.367943, 24.368538, 14.279465, 25.607574, 19.40258, 12.288322, 10.107704, 20.511078, 21.755455, 16.790627, 10.373482, 10.815697, 16.059715, 21.870872,
                17.079674, 9.720314, 14.521416, 20.914833, 20.916122, 18.376, 21.298187, 17.367897, 13.017357, 12.182365, 22.05185, 19.005875, 10.294205, 10.294685, 9.63076, 13.552879, 10.195663, 18.908806, 17.63462, 19.739113, 10.66478, 11.898537, 9.623589, 16.091537, 15.704498, 11.589096, 15.453072,
                20.778015, 21.240677, 22.460007, 21.253242, 13.410637, 9.320114, 22.670738, 23.073257, 11.355133, 22.84159, 22.918747, 19.44542, 19.926369, 16.448479, 7.5423355, 14.783043, 17.484207, 20.654259, 10.048225, 13.606758, 22.923492, 8.808853, 13.644791, 19.854843, 15.9848175, 18.396011, 15.935989, 21.255608, 12.008606, 10.232338, 13.421486, 20.413933, 17.437485, 13.214455, 12.566528, 12.192238, 19.2818, 7.3072357, 8.917816, 11.975273, 16.965866, 9.881943, 7.8177567, 14.555725, 23.478432, 14.180084, 10.11335, 9.089027, 12.268066, 6.666855, 10.818985, 8.187218, 19.844536, 22.782387, 18.2546, 8.785881, 21.681786, 22.888748, 13.254089, 21.007172, 16.110214, 8.485901, 15.751701, 20.740807, 15.825195, 14.095497, 22.207924, 23.297531, 14.160324, 13.651108, 16.593353, 12.353691, 14.037201, 7.6722183, 9.933693, 22.966057, 15.876175, 14.335724, 13.160469, 16.78437, 14.573715, 18.27156,
                17.984093, 17.816086, 19.02082, 9.773544, 11.497658, 11.829613, 21.045753, 21.103996, 14.010162, 22.99324, 19.465729, 12.668861, 12.876045, 15.2697525, 18.385727, 19.59626, 22.968094, 6.6874924, 20.494873, 18.776833, 21.857933, 14.490509, 17.357018, 12.875282, 13.564949, 20.204437,
                8.939117, 10.524391, 22.811043, 11.318367, 9.427734, 7.1323166, 19.216423, 10.9254, 15.099571, 22.641373, 15.453575, 8.33918, 18.153214, 12.699181, 12.734734, 10.183701, 13.046516, 21.594925, 22.054344, 16.113472, 8.774765, 10.198692, 16.454094, 18.723656, 21.848274, 14.148872,
                9.120941, 12.25779, 20.17765, 8.843285, 19.090683, 19.447357, 13.744698, 7.88266, 12.877983, 6.9700165, 22.16101, 19.190605, 16.217194, 17.205315, 9.926308, 9.546074, 15.19474, 16.081917, 17.869919, 12.475861, 11.696747, 21.948082, 12.364113, 10.589134, 18.328087, 16.800095, 11.813194, 19.04264, 18.484375, 9.017418, 13.688919, 20.794853, 18.399376, 9.553413, 23.069473, 17.180588, 20.959152, 8.8463745, 18.014832, 16.215431, 9.086098, 15.298119, 9.509834, 18.82492, 11.150223, 22.482903, 20.14943, 9.977501, 9.932205, 22.137856, 7.133995, 20.09771, 6.413994, 7.628807, 18.340012, 22.86203, 16.04316, 7.075653, 17.274223, 11.788727, 9.582359, 10.04097, 15.2481, 6.243614, 7.3623276, 10.850403, 22.8581, 12.886414, 16.164894, 16.131355, 12.014786, 17.058197, 8.7730255, 17.461243, 18.326942, 11.036789, 14.208282, 20.273018, 14.962822, 20.359901, 17.950058, 9.470352, 6.9013596, 20.258537, 7.9479904, 14.825432, 8.878014, 11.872322, 7.759529, 22.900063, 21.343445, 12.95224, 6.5746994, 11.698326, 12.842674,
                20.862068, 14.771889, 11.5720825, 18.065033, 13.658508, 23.0281, 9.91481, 12.274193, 22.078812, 16.473358, 17.542976, 14.083206, 13.512444, 13.983406, 7.845665, 15.383247, 20.83294, 19.908653, 13.925064,
                15.175049, 13.90226, 9.30703, 20.626717, 6.5847626, 12.176605, 10.578583, 8.688797, 22.219276, 11.425758, 20.307503, 17.182877, 12.235176, 6.966934, 14.573334, 16.34008, 17.33496, 7.4636765, 8.877876, 22.862404, 15.67585, 22.692123, 10.628464, 16.172958, 21.628021, 7.968704, 12.647446, 19.392365, 21.458092, 9.253899, 9.364807, 6.7075653, 12.599289, 20.529488, 11.605278, 14.006615, 21.975105, 6.5478363, 16.716293, 19.822723, 14.713097, 22.401558, 20.672707, 10.029709, 11.7656555, 7.290642, 15.545967, 15.852325, 8.283707, 20.10595, 17.07508, 21.564766, 21.807892, 6.4625244, 7.8598022, 9.177597, 21.220093, 20.476624, 15.309906, 9.986565, 20.845764, 17.117256, 11.043083, 13.902046, 11.570396, 12.113731, 18.866539, 6.8825684, 21.118057, 9.270027, 22.428093, 7.271164, 9.599472, 8.240059, 12.441811, 17.92096, 6.8056335, 16.854156, 14.164131, 22.324173, 21.679924, 11.47419, 9.840378, 9.520943, 22.45839, 7.053108, 12.487183, 10.024483, 6.8769226, 10.633255, 7.6559753, 6.908058, 21.54586, 8.156494,
                18.39399, 17.246262, 21.51625, 9.334366, 14.756088, 9.480614, 7.0102997, 19.666801, 13.424904, 19.5597, 13.3322525, 11.397026, 21.961967, 22.254135, 12.90316, 22.63868, 12.252106, 20.446983, 8.552498, 11.7127075, 13.085823, 18.026215, 15.324821, 11.681046, 10.664864, 13.942825, 20.340996, 20.848701, 14.770668, 22.056564, 21.128555, 12.051132, 9.782646, 21.95765, 15.841583, 11.895279, 7.714409, 21.942421, 6.031067, 16.725075, 9.34317, 18.078064, 12.749222, 11.368286, 8.669785, 11.970261, 7.384262, 8.229057, 19.674553, 13.614983, 21.1932, 8.74379, 19.020432, 6.5836105, 10.0442505, 17.928352, 10.656929, 12.704758, 10.52063, 8.752541, 21.781136, 12.910744, 20.948875, 18.896378, 21.290222, 15.249725, 18.845238, 6.887451, 18.36792, 11.172569, 6.1618958, 5.9899063, 10.366013, 14.663528, 18.3611, 9.241562, 21.874443, 8.777817, 12.35965, 14.812119, 9.003464, 21.32167, 9.923714, 14.18531, 8.656639, 13.088112, 17.121284, 9.072311, 22.480927, 10.629562, 17.247993, 20.092896, 9.06926, 20.142754, 17.58538, 6.4804153, 22.476303, 10.163406, 19.511337, 17.633842, 19.31839, 10.302277, 7.2550964, 6.290413, 8.937897, 11.2388, 12.862289, 15.66433, 6.7219467, 19.714691, 12.261543, 18.832588, 6.325142, 6.851822, 8.823143, 8.915077, 10.94577, 17.826767, 15.026039, 12.935249, 11.344093, 5.93322, 21.113777, 9.170631, 16.747238, 8.107872, 10.181984, 8.912201, 18.76075, 11.717621, 16.587975, 8.9275055, 8.052498, 6.439888, 15.475098, 16.789886, 21.94094, 8.958443, 11.329582, 14.809135, 17.097015, 20.237495, 8.175034, 7.8699875, 6.8520203, 14.87941, 8.945465, 9.495491, 15.0035095, 18.59356, 9.156105, 9.252136, 19.564537, 11.257675, 10.035187, 20.570526, 20.639938, 20.128395, 14.424843, 13.471275, 14.703201, 19.078705, 10.113205, 7.6266327, 18.32457, 7.7641525, 18.444649, 20.916481, 21.92894, 15.089973, 10.321777, 15.904663, 13.43412, 9.678352, 12.500153, 17.138672, 20.518814, 8.236099, 22.215874, 21.781197, 17.390434, 16.592499, 15.32515, 22.138992, 19.389389, 10.224579, 10.898453, 20.031944, 13.7767105, 12.057602, 15.413155, 16.205757, 13.106995, 13.182793, 20.806915, 14.722984, 14.986977, 10.291328, 7.7623444, 10.99939, 15.689629, 17.526657, 20.47734, 10.517128, 20.744728, 14.6688, 7.6909714, 6.1305237, 5.995682, 5.938774, 9.979759, 21.444397, 14.175049, 19.219421, 18.68724, 16.243782, 7.9310684, 13.0197525, 17.250061, 12.844086, 10.617538, 7.0492554, 21.543701, 17.62056, 18.881546, 13.282417, 18.836632, 8.457474, 12.328949, 6.6714706, 18.312729, 11.07106, 16.089462, 7.7079926, 9.485016, 15.066071, 7.2077103, 8.521782, 20.909576, 17.277092, 12.204575, 18.836258, 18.768333, 19.750496, 17.949661, 19.866753, 10.752899, 11.956596, 9.385719, 13.304047, 18.735, 7.5005493, 18.70095, 20.162216, 7.8467407, 6.4789124, 13.064842, 13.890678, 20.100876, 9.996483, 17.817184, 21.615997, 19.705605, 17.142677, 9.235336, 5.947975, 11.064117, 20.574913, 17.302223, 21.596283, 5.9209366, 10.740753, 14.270088, 13.442024, 14.7934265, 21.378792, 21.091454, 6.1498413, 14.374809, 8.35405,
                13.384094, 6.583893, 16.647408, 6.3619614, 8.228844, 12.103264, 19.998466, 9.556107, 20.343887, 20.070618, 17.13684, 7.818878, 21.895882, 8.709427, 9.760017, 21.93547, 15.383087, 10.065735, 6.7371674, 5.509041, 15.127991, 7.24395, 21.694252, 17.113503, 10.4951935, 13.640472, 18.534279, 19.583069, 9.748299, 20.190224, 13.902046, 17.839546, 14.363495, 7.401558, 20.404709, 6.3097305, 17.387672, 18.357246, 17.6036, 17.111877, 22.015297, 16.93, 17.55819, 6.9430313, 16.106346, 20.658318, 16.704254, 7.86512, 18.50148, 18.087051, 10.98037, 11.321793, 11.748863, 19.017952, 15.010567, 7.489395, 6.3878403, 8.078125, 21.32383, 10.504265, 14.399994, 19.755882, 12.242973, 15.424751, 16.496986, 5.9005356, 7.2962723, 17.926163, 16.015099, 7.7396164, 21.243393, 10.814125, 21.435532, 6.02861, 8.708809, 5.546692, 16.601898, 13.733673, 18.669815, 21.67051, 18.16256, 17.404213, 5.7010727, 21.365456, 14.991913, 21.48397, 8.247177, 20.712852, 14.036598, 20.12716, 17.97699, 6.702667, 11.708046, 6.913765, 8.103897, 5.7360077, 14.600914, 7.754509, 8.631454, 11.756165, 12.944931, 8.55825, 7.988228, 18.066246, 15.609077, 20.36454, 15.294701, 12.817184, 11.621849, 20.904861, 21.837769, 8.265556, 21.696022, 20.979492, 9.404533, 6.6828156, 11.59639, 16.225594, 11.589241, 17.905258, 12.284477, 14.61454, 10.362961, 14.361374, 18.819382, 7.0631714, 16.024612, 17.725792, 13.764038, 7.9561844, 6.2466965, 12.132149, 10.002983, 11.033035, 12.508934, 9.794746, 16.654327, 21.065674, 17.095383, 8.49128, 12.865631, 14.706985, 14.049522, 6.6355057, 14.860115, 14.5160675, 17.60492, 6.57621, 11.196495, 6.9511795, 19.763649, 7.433426, 18.963608, 10.120705, 12.459068, 16.40287, 19.641457, 9.608498, 9.399475, 13.909042, 11.206047, 7.7297363, 19.478737, 12.141403, 7.506111, 11.802528, 12.210083, 8.197418, 6.170189, 21.407837, 17.474098, 14.327904, 20.302513, 7.830414, 11.538055, 9.70314, 18.436691, 10.844742, 10.634987, 17.385765, 16.81897, 14.810265, 10.004364, 12.044586, 8.673347, 9.797401, 12.374229, 20.652946, 15.39872,
                20.115486, 18.700623, 9.040848, 11.242821, 7.0021515, 18.460358, 20.966248, 13.072601, 5.174797, 19.79573, 21.075478, 15.363289, 12.39518, 13.94722, 11.177139, 19.834564, 20.915901, 19.144287, 18.575272, 14.818573, 9.172241, 8.906006, 10.433731, 17.166122, 12.694527, 10.220139, 11.664246, 9.3604355, 9.013687, 5.654228, 15.515762, 19.732307, 17.430122, 7.584076, 12.438049, 12.618706, 15.080803, 16.179192, 18.466927, 18.773117, 14.094543, 9.1729355, 13.156479, 1.3433113, 7.5522766, 17.480202, 9.686012, 17.495583, 8.754562, 9.991699, 13.454773, 15.725174, 10.980843, 16.890198, 10.451584, 11.591942, 16.400024, 14.167999, 15.628098, 13.352173, 16.05738, 10.956413, 4.3922195, 13.768639, 17.701622, 10.685844, 8.89769, 17.133797, 9.374451, 7.4138184, 12.4596405, 6.20417, 12.138908, 13.830963, 13.969727, 17.421532, 10.220299, 13.663803, 6.7731094, 11.523926, 12.903824, 13.835686, 15.474831, 11.997475, 10.906311, 14.34935, 13.274963, 15.378181, 11.190956, 10.872955, 18.090454, 13.474335, 12.054955, 11.246796, 12.41777, 14.262764, 9.795143, 10.535942, 15.51815, 17.233315, 11.89576, 13.913933, 9.351387, 14.105385, 13.506157, 7.924492, 11.238922, 9.714554, 18.748741, 10.946671, 13.192574, 7.770088, 15.747208, 13.83699, 15.696693, 13.171494, 13.339561, 17.747978, 15.046112, 12.466293, 13.412994, 14.38974, 10.318283, 9.56237, 4.5222473, 12.451851, 11.566551, 8.974297, 8.469154, 9.32515, 12.190269, 16.10617, 15.214943, 14.790802, 12.6405945, 13.5477295, 15.083908, 9.738098, 8.890495, 13.187019, 11.360466, 13.075462, 15.930695, 15.719292, 14.43428, 8.828346, 14.423767, 11.667503, 7.3174286, 15.197235, 10.181885, 15.715576, 17.11673, 9.453796, 8.957825, 6.9590454, 6.9885025, 18.300728, 12.084785, 14.816406, 15.691078, 14.346077, 11.279343, 12.34536, 6.3682175, 18.182457, 6.192024, 10.828056, 7.1436768, 6.9508133, 10.560287, 13.208595, 15.526749, 11.482941, 14.354698, 17.67762, 10.895569, 14.82058, 7.9027863, 11.772804, 9.193207, 14.944649, 20.465744, 13.194908, 10.618134, 12.281746, 12.617081, 7.0784683, 13.928139, 13.634277, 7.237686, 10.728928, 16.755905, 11.759926, 9.545654, 9.585609, 9.772217, 7.3342514, 14.003532, 8.460182, 16.194077, 7.207718, 10.249542, 17.615929, 13.564461, 13.489212, 15.3001175, 13.741196, 13.462341, 11.8741455, 7.4826584, 10.052483, 9.054199, 8.485054, 11.863235, 14.54248, 19.528282, 16.144844, 12.637619, 12.259407, 12.324165, 11.016945, 12.650322, 9.115814, 11.883339, 9.113197, 7.256119, 12.930511, 10.264488, 15.374527, 13.731804, -0.6259918, 13.924469, 14.492241, 13.325394, 7.593178, 12.249908, 14.815956, 10.171814, 16.418251, 8.855263, 17.591309, 12.805229, 8.182381, 14.581085, 9.280228, 9.971542, 9.706696, 11.970619, 11.876221, 12.937302, 15.171135, 15.301392, 8.658798, 11.440887, 18.76001, 12.974686, 17.044815, 7.7605057, 13.106598, 7.525299, 12.794151, 19.174934, 4.511009, 17.565033, 10.4254, 15.791565, 13.989822, 10.909004, 13.910156, 14.356575, 10.230621, 14.504868, 16.15635, 12.413025, 8.670532, 16.534363, 9.262863, 17.720894, 15.596069, 17.428146, 7.9337463, 13.345581, 12.949585, 14.856773, 11.704895, 9.298454, 15.493416, 14.206879, 12.351456, 11.341385, 7.025017, 12.686302, 16.944984, 11.044121, 17.94423, 9.229095, 14.586952, 13.365448, 9.988716, 10.416809, 8.741379, 8.02198, 12.3231125, 10.264633, 13.353561, 13.2181015, 10.713257, 12.390083, 9.28978, 6.89402, 14.216133, 7.6969376, 10.006668, 13.385872, 6.8230133, 13.305969, 14.632042, 12.242363, 12.35968, 11.212486, 15.373917, 6.1252594, 8.644035, 18.585205, 15.983124, 12.4489975, 8.983299, 17.821503, 13.934555, 15.046333, 11.923622, 8.269691, 14.844177, 10.0228195, 16.876175, 10.675644, 12.112007, 21.854103, 11.413109, 18.745514, 16.174995, 15.00663, 11.843025, 16.08503, 7.6013947, 12.537102, 12.618439, 10.583611, 12.596001, 13.405411, 10.636009, 13.251114, 12.340668, 13.564957, 0.38695526, 10.171112, 12.7986145, 8.764458, 3.2323647, 16.78682, 12.640884, 12.470665, 13.283424, 10.894241, 12.589355, 10.481384, 11.282562, 3.4585419, 10.809349, 15.444702, 10.522079, 11.205704, 10.370148, 11.670349, 13.182846, 14.703247, 9.310852, 12.07354, 16.765839, 7.8081436,
                13.741318, 11.407394, 8.673271, 10.432396, 11.086014, 10.368538, 19.069336, 6.8377457, 9.635803, 12.470039, 10.426468, 18.91175, 6.322319, 5.957382, 6.5651855, 13.976952, 9.332916, 11.85656, 14.643921, 12.935242, 7.4443817, 15.208191, 4.2500687, 13.414833, 10.512657, 9.159058, 11.619194, 10.589783, 13.102455,
                2.62529, 8.464493, 5.5814667, 4.421631, 9.084396, 14.528069, 14.375954, 3.462555, 11.507034, 14.138649, 12.505112, 9.778198, 5.356079, 8.386955, 6.1207504, 9.589249, 12.730949, 15.315903, 5.8063126, 13.626144, 11.811745, 8.196442, 8.736626, 4.9243927, -1.020565, 9.692268, 11.731461, 15.339523, 3.5561066, 12.303833, 4.804947, 14.555634, 12.98658, 7.198784, 7.538315, 14.862038, 12.651817, -0.8139534, 3.1998215, 11.46463, 8.710815, 5.610306, 4.273567, 10.825218, 9.363213, 5.161438, 10.892548, 7.7080994, 13.29702, 15.287201, 8.180679, 9.0624695, 12.555267, 9.773399, 10.72393, 7.8859863, 8.139954, 9.376747, 10.082329, 12.801735, 6.7571945, 8.994225, 16.798767, 8.196808, 2.3343544, 7.5486603, 9.154671, 1.3284073, 6.48999, 6.435837, 18.002228, 16.389648, 10.490387, 10.626129, 13.019997, 7.964691, 9.431961, 5.4287033, 7.635521, 9.528175, 6.4591904, 8.901276, 10.864784, 10.563957, 15.05011, 7.998123, 8.525444, 4.455101, 1.453682, 11.092865, 10.691109, 10.550583, 10.197006, 12.09552, 6.421417, 10.8703, 7.923271, 9.987831, 11.752426, 3.5594559, 12.631798, 11.716789, 6.7620087, 4.341728, 12.247345, 11.351456, 7.62117, 10.092575, 10.679199, 8.157242, 6.691559, 8.734085, 5.615326, 11.810814, 8.05455, 13.220314, 5.40506, 0.38625336, 12.239876, 12.193596, 6.77742, 11.875671, 12.37561, 8.150658, 11.238564, 4.703827, 7.694031, 9.013016, 9.493919, 9.34343, 2.7594376, 9.554153, 8.282112, 2.9465637, 11.95636, 10.557678, 6.545433, 4.225914, 4.0008316, 11.681419,
                8.838478, 5.6994934, 13.099022, 1.7636833, 20.109856, 15.055183, 6.905159, 11.397408, 7.874817, 8.478691, 6.260124, 5.398361, 6.540695, 4.563072, 4.815422, 10.843887, 15.065178, 12.62413, 9.833122, 10.498894, 6.4252167, 4.3710327, 6.336937, 11.971062, 5.8398743, 4.059059, 10.8267975, 12.147034, 5.2556534, 4.7089844, 8.817184, 3.2708817, 8.746208, 11.65757, 12.67971, 8.690582, 12.281181, 13.262764, 2.8903885, 0.4717636, 9.271072, 5.855362, 4.2088547, 10.890785, 12.070374, 5.7779694, 5.0358353, 6.558159, 5.7009964, 7.8795853, 9.0063095, 5.8971863, 6.773819, 9.905121, 2.488678, 16.291023, 8.456078, 4.1512375, 2.8161926, 1.7917023, 8.78701, 11.044571, 9.873253, 7.090164, 9.091682, 13.824654, 0.69283295, 10.809868, 12.771393, 10.0264435, 3.063446, 5.7978134, 3.6923828, 2.9400482, 17.288086, 5.9595184, 11.60791, 4.8033752, 9.699463, 11.628876, 5.970009, 5.4806137, 6.5139008, 10.881424, 11.071625, 6.5625076, 13.259422, 10.993706, 8.636635, 7.9518127, 9.046074, 7.5130386, 9.5466385, 7.281212, 9.244263, 6.9986954, 8.00145, 6.530098, 11.631035, 7.46846, 8.309181, 9.368149, 5.434021, 11.692291, 7.7217484, 8.070892, 10.737343, 5.007225, 0.9728508, 10.828857, 7.496521, 10.270226, 7.8363495, 11.422249, 5.2468796, 10.713013, 7.732979, 6.9751053, 5.2620316, 12.545769, 1.7219238, 12.080757, 13.701462, 8.3966675, 8.77784, 9.37735, 10.062302, 7.79924, 7.6329193, 14.059311, 10.581879, 6.9904404, 4.4473877, 11.890175, 9.3498535, 9.020645, 12.20192, 9.142426, 5.988434, 1.2271538, 8.884514, 5.6873093, 8.259895, 7.2191925, 8.208504, 6.7677307, 3.3581924, 10.882446, 8.080147, 8.298767, 7.902008, 9.760544, 7.0842285, 6.4117584, 7.3588333, 6.063202, 8.054321, 5.401146, 7.821739, 8.992706, 5.4064865, 10.538536, 7.9063873, 8.407799, 10.033569, 10.246262, 5.954918, 12.278641, 9.63829, 3.4365234, 9.127907, 11.445694, 10.830666, 4.5859375, 8.685242, 8.419197, 6.626793, 8.489822, 11.864655, 6.0545197, 5.4940796, 7.499634, 6.8434906, 10.27272, 6.390831, 13.024765, 10.478867, 4.3631516, 3.966629, 7.6196976, 11.016945, 7.914711, 9.490356, 8.956329, 7.7327576, 4.5056305, 11.125511, 10.234268, 10.721672, 7.839081, 10.882629, 4.1763535, 14.877441, 7.9287415, 13.425743, 7.4035645, 13.130867, 5.4546204, 15.25164, 11.290291, 6.6230392, 5.153023, 7.244774, 4.93869, 6.9927216, 4.21743, 11.407097, 8.841782, 4.6123962, 12.073158, 5.24572, 8.5592575, 7.9452972, 6.124962, 4.4715347, 10.074875, 11.388512, 6.997658, 3.927002, 7.5746155, 7.9861145, 4.8407288, 9.839661, 6.111511, 8.361748, 8.257668, -0.47646332, 11.522377, 15.6489105, 11.245674, 7.455803, 5.5983276, 12.7532425, 5.290413, 7.809494, 8.2335205, 12.859329, 6.332733, 8.173599, 5.5710297, 10.199425, 10.052063, 9.271355, 7.9247284, 13.394585, 1.871891, 17.26699, 5.0669937, 9.818176, 10.54496, 9.220009, 8.415489, 7.4871063, 9.211662, 10.444298, 4.503975, 8.681351, 4.6464615, 6.431801, 9.923386, 13.796646, 13.90065, 6.581459, 7.359764, 8.344025, 1.4591446, 10.455063, 10.026909, 9.338165, 11.306221, 10.475632, 13.831345, 11.379021, 10.621582, -0.12689209, 9.535767, 12.933159, 4.570648, 4.639923, 8.388466, 10.42646, 6.3969955, 5.8983, 7.7566833, 11.626404, 9.044601, 8.7464905, 12.141327, 2.8749619, 2.4973526, 7.825577, 9.246223, 3.7963562, 3.9213562, 15.219719, 12.097153, 11.223091, 2.34618, 9.322411, 13.991928, 7.9767075, 7.1600723, 9.959892, 4.7432785, 10.100708, 7.550026, 7.8062897, 15.325211, 9.171326, 14.590614, 4.4052353, 3.8433456, 13.943268, 13.450645, 10.471619, 8.816711, 6.6667404, 6.573555, 3.1553802, 6.5295258, 7.174713, 10.9850235, 10.550583, 10.659027, 5.4809113, 12.719185, 8.468002, 10.746681, 2.5665588, 12.752747, 3.9914246, 8.391701, 10.34668, 11.101715, 1.396389, 11.029274, 10.088448, 4.962822, 6.2530975, 11.940491, 4.676384, 7.695114, 2.5302582, 2.8398056, 11.287193, 10.065407, 6.605873, 12.056778, 15.564148, 9.102455, 10.520058, 9.873497, 19.080177, 9.027748, 5.4381256, 11.829002, 2.206894, 10.2730255, 4.3419647, 5.5344315, 9.611534, 7.054657, 13.363609, 5.7571106, 6.1685257, 9.327934, 8.136147, 3.7443619, 10.495796, 10.240288, 9.620132, 6.1760635, 12.111992, 7.777069, 9.071091, 15.40181, 11.490875, 1.2383919, 8.732666, 6.28598, 6.9781647, 5.8513336, 2.1257172, 6.238838, 6.072838, 14.4698715, 7.250778, 14.687233, 10.691353, 7.4973297, 5.698036, 13.830002, 7.3318024, 8.284195, 9.174469, 9.153122, 8.343262, 4.939354, 6.7281494, 6.5127716, 8.787102, 4.621895, 2.7378845, 5.776581, 3.7946472, 13.218033, 10.241684, 5.148308, 9.492401, 4.3686676, 11.6017, 12.862595, 11.554756, 9.481522, 6.466751, 7.9301605, 4.266838, 6.1601334, 7.6510544, 8.348343, 10.067398, 4.4602127, 3.316597, 9.41082, 10.801033, 4.3121414,
                6.9548798, 8.841324, 6.227585, 5.4870834, 10.611641, 8.776756, 6.2407, 9.262993, 12.894318, 1.6085968, 8.881996, 4.625206, 1.9411278, 9.870689, 7.210945, 9.109016, 6.0928726, 11.096596, 12.627548, 5.1498184, 14.356247, 6.1668854, 7.092369, 7.701172, 10.325684, 11.601761, 7.2524414, 6.181999, 9.434578, 9.081985, 4.7412643, 7.4645233, 10.196129, 10.36335, 6.6664047, 3.054573, 9.253296, 10.21534, 9.721161, 18.417572, 3.1223679, 9.360603, 5.180916, 9.571991, 9.336212, 7.217476, 9.895149, 0.39609528, 1.0594254, -1.2757225, 4.599945, -0.012989044, 7.4304123, 10.350731, 7.000778, 7.7773514, 1.956295, 12.494621, 6.684067, 2.79953, 2.9192657, 0.6268654, 4.0511475, 8.591118, -1.8108177, 11.016632, 4.0881577, 11.342758, 1.6770134, 13.420021, 6.159645, -0.27661133, 4.830414, 3.5044556, 0.7789345, 10.304428, 4.410843, 5.4039536, 14.409935, 14.344032, 10.91909, 3.9320526, -1.9615059, 1.4297142, 10.74733, 7.9588547, 5.391632, 1.7876892, 7.838333, 8.018349, -1.4479561, 5.5027924, -10.443291, -17.05555, -17.507828, -14.255936, -16.903446, -10.89241, -16.782864, -12.642052, -19.479229, -9.365715, -18.666527, -15.57497, -12.637321, -14.855141, -15.460571, -16.99028, -15.08329, -13.763039, -20.5083, -17.528488, -11.560066, -13.40358, -16.243088, -12.552307, -19.988163, -15.759914, -27.589302, -17.48418, -13.12912, -18.357555, -24.095192, -15.492165, -9.639885, -12.703865, -20.438145, -12.539108, -20.787327, -13.735008, -12.436302, -16.404179, -18.851822, -15.959942, -13.56237, -21.66227, -17.98397, -21.202068, -21.699795, -17.955784, -17.19326, -27.315601, -18.196873, -13.00489, -18.402958, -27.096794, -19.129204, -17.864288, -20.214737, -14.253723, -12.734756, -16.493484, -15.34803, -14.188576, -22.388245, -20.953983, -22.010593, -16.708538, -17.003796, -18.580242, -13.905846, -21.003563, -29.498444, -14.856728, -12.864113, -13.32608, -9.26001, -24.00138, -19.4533, -21.388332, -13.706963, -16.920723, -22.384045, -25.991924, -17.117985, -16.98426, -24.853317, -22.635475, -21.410992, -19.304878, -13.645348, -20.865105, -22.679981, -23.840557, -18.314884, -15.379036, -22.417652, -17.02884, -21.506786, -12.578163, -23.054726, -15.570728, -51.645664, -47.375225, -54.005157, -51.416016, -47.538883, -45.75016, -55.50598, -58.405354, -47.193615, -44.706596, -43.493324, -49.207436, -48.98085, -49.759113, -45.47449, -46.347507, -54.37645, -51.899384, -54.586365, -44.382843, -48.268967, -48.694534, -54.261425, -51.30262, -52.350536, -52.315437, -53.655125, -58.126083, -40.13359, -50.957375, -48.669117, -43.863403, -43.440132, -52.615654, -54.73618, -41.953995, -46.911484, -46.50359, -52.658, -60.319492, -48.728767, -51.424866, -45.134586, -52.066265, -48.659874, -55.57541, -45.204327, -53.94902, -54.758724, -49.030117, -53.39594, -58.879604, -51.927204, -49.855343, -53.80038, -44.495705, -41.361214, -52.617912, -49.783524, -62.735386, -49.31346, -48.70229, -47.814205, -44.533997, -47.137722, -47.091873, -46.927597, -44.87278, -44.340187, -42.930252, -40.597305, -54.25224, -54.689278, -57.595947, -41.823875, -44.72958, -58.954575, -49.57727, -51.366024, -53.717094, -51.628826, -39.841812, -41.708633, -55.90271, -52.470707, -55.461826, -50.394833, -61.462383, -47.81471, -53.401768, -33.53253, -55.31763, -59.276928, -59.339462, -45.006542, -53.30818, -47.34733, -46.563084, -51.975986, -60.547817, -86.703835, -105.04149, -92.36385, -92.39051, -89.22275, -87.24386, -89.71219, -87.3764, -87.40732, -94.52154, -94.23016, -87.444756, -85.82067, -93.71182, -90.39978, -88.31973, -91.78732, -90.39362, -97.0619, -102.618935, -98.1076, -90.33147, -91.00008, -97.67384, -88.75184, -95.92693, -91.60867, -86.92531, -85.8194, -91.40172, -101.12471, -97.63818, -91.21962, -92.60516, -85.77664, -104.93595, -96.577255, -91.55618, -89.4299, -96.247635, -94.184586, -93.114914, -91.16815, -97.12602,
                -91.51756, -93.912994, -94.96173, -97.13478, -95.42807, -93.55741, -88.85095
    };

    // 复制频谱数据到target
    int dataSize = sizeof(spectrumData) / sizeof(float);
    for (int i = 0; i < 5296; i++) {
        if (i < dataSize) {
            target.spectrumData[i] = spectrumData[i];
        } else {
            // 用基础值加随机变化填充剩余部分
            target.spectrumData[i] = 40.0f + static_cast<float>(rand() % 20);
        }
    }

    // 添加目标到列表
    soundData.propagatedContinuousList.push_back(target);

    return soundData;
}

void MainWindow::onShowEquationResultsClicked()
{
    if (!m_component) {
        addLog("错误：声纳组件未初始化");
        return;
    }

    // 将组件转换为 DeviceModel* 以访问声纳方程功能
    DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
    if (!deviceModel) {
        addLog("错误：无法转换为 DeviceModel 类型");
        return;
    }





    // *** 详细的调试信息 ***
        addLog("=== 开始调试声纳方程计算状态 ===");

        // 检查每个声纳的数据有效性
        for (int sonarID = 0; sonarID < 4; sonarID++) {
            bool dataValid = deviceModel->isEquationDataValid(sonarID);
            addLog(QString("声纳%1数据有效性: %2").arg(sonarID).arg(dataValid ? "有效" : "无效"));

            // 检查声纳状态
            // 这里我们需要添加一个方法来获取声纳状态，或者直接检查
        }



    // 获取所有声纳的计算结果
    auto results = deviceModel->getAllSonarEquationResults();

    QString resultText;
    resultText += "============ 声纳方程计算结果 ============\n";
    resultText += QString("计算时间：%1\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    resultText += "公式：SL-TL-NL+DI=X\n\n";

    if (results.empty()) {
        resultText += "❌ 暂无有效的计算结果\n";
        resultText += "原因可能：\n";
        resultText += "1. 数据未准备完整（需要平台噪声、环境噪声、传播声三种数据）\n";
        resultText += "2. 数据已过期（超过5秒未更新）\n";
        resultText += "3. 声纳未启用\n\n";
        resultText += "建议：点击\"发送完整测试数据\"按钮，等待几秒后再查看结果。";
    } else {
        // 声纳名称映射
        QStringList sonarNames = {"艏端声纳", "舷侧声纳", "粗拖声纳", "细拖声纳"};

        for (int sonarID = 0; sonarID < 4; sonarID++) {
            resultText += QString("📡 %1 (ID=%2):\n").arg(sonarNames[sonarID]).arg(sonarID);

            auto it = results.find(sonarID);
            if (it != results.end()) {
                double result = it->second;
                resultText += QString("   ✅ X = %.3f\n").arg(result);

                // 检查数据有效性
                bool dataValid = deviceModel->isEquationDataValid(sonarID);
                if (!dataValid) {
                    resultText += "   ⚠️  数据已过期，结果可能不准确\n";
                }
            } else {
                resultText += "   ❌ 无计算结果（声纳可能未启用或数据不足）\n";
            }
            resultText += "\n";
        }

        // 添加说明
        resultText += "💡 说明：\n";
        resultText += "• X值越大表示目标信号相对噪声越强\n";
        resultText += "• 数据每5秒更新一次，过期数据会标注警告\n";
        resultText += "• 只有启用的声纳才会进行计算\n";
    }

    // 更新显示
    m_equationResultsTextEdit->setPlainText(resultText);

    addLog("已更新声纳方程计算结果显示");
}

void MainWindow::onSendCompleteTestDataClicked()
{
    if (!m_component || !m_agent) {
        addLog("错误：声纳组件或代理未初始化");
        return;
    }

    addLog("开始发送完整测试数据...");

    try {
        // 1. 发送环境噪声数据
        CMsg_EnvironmentNoiseToSonarStruct envNoise;

        // 生成模拟的环境噪声频谱数据
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> envNoiseDist(25.0f, 40.0f);  // 25-40 dB

        for (int i = 0; i < 5296; i++) {
            envNoise.spectrumData[i] = envNoiseDist(gen);
        }
        envNoise.acousticVel = 1500.0f;



        // *** 打印环境噪声频谱数据 ***
        addLog("=== 环境噪声频谱数据 ===");
        QString envSpectrumInfo = "前100个值: ";
        for (int i = 0; i < 100; i++) {
            envSpectrumInfo += QString::number(envNoise.spectrumData[i], 'f', 2) + " ";
        }
        addLog(envSpectrumInfo);

        float envSum = 0.0f, envMin = envNoise.spectrumData[0], envMax = envNoise.spectrumData[0];
        for (int i = 0; i < 5296; i++) {
            envSum += envNoise.spectrumData[i];
            if (envNoise.spectrumData[i] < envMin) envMin = envNoise.spectrumData[i];
            if (envNoise.spectrumData[i] > envMax) envMax = envNoise.spectrumData[i];
        }
        addLog(QString("环境噪声统计: 总和=%1, 平均=%2, 最小=%3, 最大=%4")
               .arg(QString::number(envSum, 'f', 2))
               .arg(QString::number(envSum/5296, 'f', 2))
               .arg(QString::number(envMin, 'f', 2))
               .arg(QString::number(envMax, 'f', 2)));





        CSimMessage envMsg;
        envMsg.dataFormat = STRUCT;
        envMsg.time = QDateTime::currentMSecsSinceEpoch();
        envMsg.sender = 1;
        envMsg.senderComponentId = 1;
        envMsg.receiver = 1885;
        envMsg.data = &envNoise;
        envMsg.length = sizeof(envNoise);
        memcpy(envMsg.topic, MSG_EnvironmentNoiseToSonar, strlen(MSG_EnvironmentNoiseToSonar) + 1);

        m_component->onMessage(&envMsg);
        addLog("✅ 已发送环境噪声数据");

        // 2. *** 准备平台自噪声数据 ***
        CData_PlatformSelfSound* platformSelfSound = new CData_PlatformSelfSound();

        // 为4个声纳位置创建自噪声数据
        std::uniform_real_distribution<float> selfNoiseDist(30.0f, 50.0f);  // 30-50 dB

        for (int sonarID = 0; sonarID < 4; sonarID++) {
            C_SelfSoundSpectrumStruct spectrumStruct;
            spectrumStruct.sonarID = sonarID;

            // 生成频谱数据
            for (int i = 0; i < 5296; i++) {
                spectrumStruct.spectumData[i] = selfNoiseDist(gen);
            }

            platformSelfSound->selfSoundSpectrumList.push_back(spectrumStruct);







            // *** 打印每个声纳的平台自噪声频谱数据 ***
           addLog(QString("=== 平台自噪声频谱数据 (声纳%1) ===").arg(sonarID));
           QString selfSpectrumInfo = "前100个值: ";
           for (int i = 0; i < 100; i++) {
               selfSpectrumInfo += QString::number(spectrumStruct.spectumData[i], 'f', 2) + " ";
           }
           addLog(selfSpectrumInfo);

           float selfSum = 0.0f, selfMin = spectrumStruct.spectumData[0], selfMax = spectrumStruct.spectumData[0];
           for (int i = 0; i < 5296; i++) {
               selfSum += spectrumStruct.spectumData[i];
               if (spectrumStruct.spectumData[i] < selfMin) selfMin = spectrumStruct.spectumData[i];
               if (spectrumStruct.spectumData[i] > selfMax) selfMax = spectrumStruct.spectumData[i];
           }
           addLog(QString("声纳%1自噪声统计: 总和=%2, 平均=%3, 最小=%4, 最大=%5")
                  .arg(sonarID)
                  .arg(QString::number(selfSum, 'f', 2))
                  .arg(QString::number(selfSum/5296, 'f', 2))
                  .arg(QString::number(selfMin, 'f', 2))
                  .arg(QString::number(selfMax, 'f', 2)));






        }

        // 创建 CSimData 包装器
        CSimData* selfSoundData = new CSimData();
        selfSoundData->dataFormat = STRUCT;
        int64 currentTime = QDateTime::currentMSecsSinceEpoch();
        selfSoundData->time = currentTime;  // 确保时间戳被设置
        selfSoundData->sender = m_agent->getPlatformEntity()->id;
        selfSoundData->receiver = m_agent->getPlatformEntity()->id;
        selfSoundData->componentId = 1;
        selfSoundData->data = platformSelfSound;
        selfSoundData->length = sizeof(*platformSelfSound);
        memcpy(selfSoundData->topic, Data_PlatformSelfSound, strlen(Data_PlatformSelfSound) + 1);

        // *** 确认时间戳被正确设置 ***
        addLog(QString("平台自噪声数据创建时时间戳: %1").arg(selfSoundData->time));

        m_agent->addSubscribedData(Data_PlatformSelfSound,
                                   m_agent->getPlatformEntity()->id,
                                   selfSoundData);

        addLog("✅ 已准备平台自噪声数据并添加到订阅数据中");

        // 3. 发送传播后连续声数据
        CMsg_PropagatedContinuousSoundListStruct continuousSound;
    // 为4个声纳位置各创建一个传播声数据
        std::uniform_real_distribution<float> signalDist(60.0f, 80.0f); // 60-80 dB（信号强度）

        for (int sonarID = 0; sonarID < 4; sonarID++) {
            C_PropagatedContinuousSoundStruct soundData;
        // 设置目标参数
            soundData.arrivalSideAngle = 30.0f + sonarID * 10.0f;  // 不同方向
            soundData.arrivalPitchAngle = 5.0f;
            soundData.targetDistance = 1000.0f + sonarID * 200.0f;
            soundData.platType = 1; // 潜艇类型

            // 生成频谱数据
            for (int i = 0; i < 5296; i++) {
                soundData.spectrumData[i] = signalDist(gen);
            }

            continuousSound.propagatedContinuousList.push_back(soundData);





            // *** 打印每个传播声的频谱数据 ***
            addLog(QString("=== 传播声频谱数据 (目标%1) ===").arg(sonarID));
            QString propSpectrumInfo = "前100个值: ";
            for (int i = 0; i < 100; i++) {
                propSpectrumInfo += QString::number(soundData.spectrumData[i], 'f', 2) + " ";
            }
            addLog(propSpectrumInfo);

            float propSum = 0.0f, propMin = soundData.spectrumData[0], propMax = soundData.spectrumData[0];
            for (int i = 0; i < 5296; i++) {
                propSum += soundData.spectrumData[i];
                if (soundData.spectrumData[i] < propMin) propMin = soundData.spectrumData[i];
                if (soundData.spectrumData[i] > propMax) propMax = soundData.spectrumData[i];
            }
            addLog(QString("目标%1传播声统计: 总和=%2, 平均=%3, 最小=%4, 最大=%5, 距离=%6m, 方位=%7°")
                   .arg(sonarID)
                   .arg(QString::number(propSum, 'f', 2))
                   .arg(QString::number(propSum/5296, 'f', 2))
                   .arg(QString::number(propMin, 'f', 2))
                   .arg(QString::number(propMax, 'f', 2))
                   .arg(QString::number(soundData.targetDistance, 'f', 1))
                   .arg(QString::number(soundData.arrivalSideAngle, 'f', 1)));





        }

        CSimMessage continuousMsg;
        continuousMsg.dataFormat = STRUCT;
        continuousMsg.time = QDateTime::currentMSecsSinceEpoch();
        continuousMsg.sender = 1;
        continuousMsg.senderComponentId = 1;
        continuousMsg.receiver = 1885;
        continuousMsg.data = &continuousSound;
        continuousMsg.length = sizeof(continuousSound);
        memcpy(continuousMsg.topic, MSG_PropagatedContinuousSound, strlen(MSG_PropagatedContinuousSound) + 1);

        m_component->onMessage(&continuousMsg);
        addLog("✅ 已发送传播后连续声数据");

        // *** 关键步骤：手动触发 step() 方法让 DeviceModel 获取订阅数据 ***
        currentTime = QDateTime::currentMSecsSinceEpoch();
        int32 stepInterval = 1000;  // 1秒步长

        m_component->step(currentTime, stepInterval);
        addLog("✅ 已触发 step() 方法，DeviceModel 现在可以获取平台自噪声数据了");

        addLog("🎉 完整测试数据发送完成！");
        addLog("所有三种数据（环境噪声、平台自噪声、传播声）都已准备就绪");
        addLog("点击\"显示计算结果\"查看声纳方程计算结果");

    } catch(const std::exception& e) {
        addLog(QString("❌ 发送测试数据时出错: %1").arg(e.what()));
    } catch(...) {
        addLog("❌ 发送测试数据时发生未知错误");
    }




    QTimer::singleShot(100, this, [this]() {
        if (m_component) {
            int64 currentTime = QDateTime::currentMSecsSinceEpoch();
            m_component->step(currentTime, 1000);
            addLog("🔄 额外触发了一次 step() 方法");
        }
    });
}


void MainWindow::onSetDIParametersClicked()
{
    if (!m_component) {
        addLog("错误：声纳组件未初始化");
        return;
    }

    // 将组件转换为 DeviceModel* 以访问DI参数设置功能
    DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
    if (!deviceModel) {
        addLog("错误：无法转换为 DeviceModel 类型");
        return;
    }

    // 创建DI参数设置对话框
    QDialog dialog(this);
    dialog.setWindowTitle("设置声纳DI参数");
    dialog.setModal(true);
    dialog.resize(400, 300);

    QVBoxLayout* dialogLayout = new QVBoxLayout(&dialog);

    // 说明标签
    QLabel* infoLabel = new QLabel("设置各声纳位置的DI计算参数\nDI = 20lg(f) + offset\n(频率上限5kHz)");
    infoLabel->setStyleSheet("color: blue; font-weight: bold;");
    dialogLayout->addWidget(infoLabel);

    // 声纳参数设置
    QStringList sonarNames = {"艏端声纳", "舷侧声纳", "粗拖声纳", "细拖声纳"};
    QList<QDoubleSpinBox*> freqSpinBoxes;
    QList<QDoubleSpinBox*> offsetSpinBoxes;

    for (int sonarID = 0; sonarID < 4; sonarID++) {
        QGroupBox* sonarGroup = new QGroupBox(sonarNames[sonarID]);
        QGridLayout* gridLayout = new QGridLayout(sonarGroup);

        // 频率设置
        QLabel* freqLabel = new QLabel("频率 (kHz):");
        QDoubleSpinBox* freqSpinBox = new QDoubleSpinBox();
        freqSpinBox->setRange(0.1, 5.0);
        freqSpinBox->setDecimals(1);
        freqSpinBox->setSingleStep(0.1);
        freqSpinBox->setValue(3.0 + sonarID * 0.2);  // 默认值

        // 偏移量设置
        QLabel* offsetLabel = new QLabel("偏移量:");
        QDoubleSpinBox* offsetSpinBox = new QDoubleSpinBox();
        offsetSpinBox->setRange(0.0, 20.0);
        offsetSpinBox->setDecimals(1);
        offsetSpinBox->setSingleStep(0.1);
        offsetSpinBox->setValue(9.5 + sonarID * 0.1);  // 默认值

        gridLayout->addWidget(freqLabel, 0, 0);
        gridLayout->addWidget(freqSpinBox, 0, 1);
        gridLayout->addWidget(offsetLabel, 1, 0);
        gridLayout->addWidget(offsetSpinBox, 1, 1);

        freqSpinBoxes.append(freqSpinBox);
        offsetSpinBoxes.append(offsetSpinBox);

        dialogLayout->addWidget(sonarGroup);
    }

    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okButton = new QPushButton("确定");
    QPushButton* cancelButton = new QPushButton("取消");

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    dialogLayout->addLayout(buttonLayout);

    // 显示对话框
    if (dialog.exec() == QDialog::Accepted) {
        // 应用设置
        for (int sonarID = 0; sonarID < 4; sonarID++) {
            double freq = freqSpinBoxes[sonarID]->value();
            double offset = offsetSpinBoxes[sonarID]->value();

            deviceModel->setDIParameters(sonarID, freq, offset);

            addLog(QString("设置 %1 DI参数: f=%.1f kHz, offset=%.1f")
                   .arg(sonarNames[sonarID]).arg(freq).arg(offset));
        }

        addLog("✅ 所有声纳DI参数设置完成");
    }
}
