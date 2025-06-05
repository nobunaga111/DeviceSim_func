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
    m_tabWidget->addTab(m_messageSendTab, "声纳方程计算");  // 修改标签页名称

    // 创建底部日志容器
    QWidget* logContainer = new QWidget(this);
    QVBoxLayout* logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(0, 0, 0, 0);

    // *** 增加日志显示区域的最小高度 ***
    m_logTextEdit->setMinimumHeight(300);  // 增加日志区域高度

    // 添加日志控件和按钮到底部容器
    logLayout->addWidget(m_logTextEdit);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_clearLogButton);
    logLayout->addLayout(buttonLayout);

    // *** 调整标签页和日志容器的比例 ***
    mainLayout->addWidget(m_tabWidget, 2);      // 标签页占2/3
    mainLayout->addWidget(logContainer, 1);     // 日志区占1/3
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

    // *** 删除原来的声音数据发送组，直接创建声纳方程测试组 ***
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

    // *** 计算结果显示区域 - 大幅增加高度 ***
    m_equationResultsTextEdit = new QTextEdit();
    m_equationResultsTextEdit->setReadOnly(true);
    m_equationResultsTextEdit->setMinimumHeight(400);  // 从200增加到400
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
    equationLayout->addWidget(m_equationResultsTextEdit);  // 这是主要的显示区域

    // 只添加声纳方程测试组到主布局
    layout->addWidget(m_equationGroupBox);
    // 删除 addStretch，让计算结果区域占满剩余空间

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
        resultText += "暂无有效的计算结果\n";
        resultText += "原因可能：\n";
        resultText += "1. 数据未准备完整（需要平台噪声、环境噪声、传播声三种数据）\n";
        resultText += "2. 数据已过期（超过5秒未更新）\n";
        resultText += "3. 声纳未启用\n\n";
        resultText += "建议：点击\"发送完整测试数据\"按钮，等待几秒后再查看结果。";
    } else {
        // 声纳名称映射
        QStringList sonarNames = {"艏端声纳", "舷侧声纳", "粗拖声纳", "细拖声纳"};

        for (int sonarID = 0; sonarID < 4; sonarID++) {
            resultText += QString("%1 (ID=%2):\n").arg(sonarNames[sonarID]).arg(sonarID);

            auto it = results.find(sonarID);
            if (it != results.end()) {
                double result = it->second;
                // 正确的写法 - 使用 Qt 风格的占位符
                resultText += QString(" X = %1\n").arg(QString::number(result, 'f', 3));

                // 检查数据有效性
                bool dataValid = deviceModel->isEquationDataValid(sonarID);
                if (!dataValid) {
                    resultText += " 数据已过期，结果可能不准确\n";
                }
            } else {
                resultText += " 无计算结果（声纳可能未启用或数据不足）\n";
            }
            resultText += "\n";
        }

        // 添加说明
        resultText += "说明：\n";
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
        addLog("已发送环境噪声数据");

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
        int64 platformId = m_agent->getPlatformEntity()->id;
        addLog(QString("准备存储平台自噪声数据: platformId=%1, time=%2").arg(platformId).arg(selfSoundData->time));

        m_agent->addSubscribedData(Data_PlatformSelfSound, platformId, selfSoundData);

        // *** 验证数据是否正确存储 ***
        CSimData* retrievedData = m_agent->getSubscribeSimData(Data_PlatformSelfSound, platformId);
        if (retrievedData) {
            addLog(QString("验证：平台自噪声数据已成功存储，时间戳: %1").arg(retrievedData->time));
        } else {
            addLog("验证失败：无法获取刚存储的平台自噪声数据！");
        }






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


//        CMsg_PropagatedContinuousSoundListStruct continuousSound = createContinuousSoundData();

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
        addLog("已发送传播后连续声数据");

        // *** 关键步骤：手动触发 step() 方法让 DeviceModel 获取订阅数据 ***
        currentTime = QDateTime::currentMSecsSinceEpoch();
        int32 stepInterval = 1000;  // 1秒步长

        m_component->step(currentTime, stepInterval);
        addLog("已触发 step() 方法，DeviceModel 现在可以获取平台自噪声数据了");

        addLog("完整测试数据发送完成！");
        addLog("所有三种数据（环境噪声、平台自噪声、传播声）都已准备就绪");
        addLog("点击\"显示计算结果\"查看声纳方程计算结果");

    } catch(const std::exception& e) {
        addLog(QString("发送测试数据时出错: %1").arg(e.what()));
    } catch(...) {
        addLog("发送测试数据时发生未知错误");
    }




//    QTimer::singleShot(100, this, [this]() {
//        if (m_component) {
//            int64 currentTime = QDateTime::currentMSecsSinceEpoch();
//            m_component->step(currentTime, 1000);
//            addLog("额外触发了一次 step() 方法");
//        }
//    });
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

        addLog("所有声纳DI参数设置完成");
    }
}
