#include "mainwindow.h"
#include <random>
#include <chrono>
#include <cstring>
#include <QSplitter>
#include <QHeaderView>
#include <QTableWidget>
#include <SonarConfig.h>
#include <QDebug>

// 静态常量定义
const QStringList MainWindow::SONAR_NAMES = {
    "艏端声纳", "舷侧声纳", "粗拖声纳", "细拖声纳"
};

const QList<QColor> MainWindow::SONAR_COLORS = {
    QColor(0, 255, 0),      // 艏端声纳 - 绿色
    QColor(0, 0, 255),      // 舷侧声纳 - 蓝色
    QColor(255, 255, 0),    // 粗拖声纳 - 黄色
    QColor(255, 0, 255)     // 细拖声纳 - 洋红色
};

// 构造函数
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mainSplitter(nullptr)
    , m_rightSplitter(nullptr)
    , m_seaChartWidget(nullptr)
    , m_seaChartPanel(nullptr)
    , m_controlPanel(nullptr)
    , m_component(nullptr)
    , m_agent(nullptr)
    , m_statusUpdateTimer(nullptr)
    , m_dataGenerationTimer(nullptr)
    , m_platformSelfSoundSent(false)
    , m_environmentNoiseSent(false)
{
    // 设置窗口属性
    setWindowTitle("多目标声纳系统导调平台");
    resize(1400, 900);
    setMinimumSize(1200, 800);

    // 居中显示窗口
    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    // 初始化声纳状态
    for (int i = 0; i < 4; i++) {
        m_sonarEnabledStates[i] = true;
    }

    // 1 初始化声纳范围配置的默认值
    m_sonarRangeConfigs[0] = SonarRangeConfig{0, "艏端声纳", 30000.0f, -45.0f, 45.0f, 0.0f, 0.0f, false};
    m_sonarRangeConfigs[1] = SonarRangeConfig{1, "舷侧声纳", 25000.0f, 45.0f, 135.0f, -135.0f, -45.0f, true};
    m_sonarRangeConfigs[2] = SonarRangeConfig{2, "粗拖声纳", 35000.0f, 135.0f, 225.0f, 0.0f, 0.0f, false};
    m_sonarRangeConfigs[3] = SonarRangeConfig{3, "细拖声纳", 40000.0f, 120.0f, 240.0f, 0.0f, 0.0f, false};

    // 2 *** 初始化界面，确保UI组件都被创建 ***
    initializeUI();

    // 3 *** 加载扩展配置 ***
    loadExtendedConfig();

    // 4 创建声纳模型实例
    m_component = createComponent("sonar");
    m_agent = new DeviceModelAgent();

    // 5 初始化声纳模型
    if (m_component && m_agent) {
        m_component->init(m_agent, nullptr);
        m_component->start();
        addLog("多目标声纳模型初始化完成");
    } else {
        addLog("错误：声纳模型初始化失败");
    }

    // 6 初始化定时器
    initializeTimers();

    // 7 发送基础数据
    generateAndSendEnvironmentNoiseData();
    generateAndSendPlatformSelfSoundData();

    // 8. 同步配置到界面和海图（在模型初始化之后）
    if (m_component) {
        QTimer::singleShot(100, this, [this]() {
            syncThresholdFromModel();
            syncSonarRangeToChart();
            addLog("阈值和范围配置界面已同步");
        });
    }

    addLog("多目标声纳系统导调平台启动完成");
}

// 析构函数
MainWindow::~MainWindow()
{
    // 停止定时器
    if (m_statusUpdateTimer) {
        m_statusUpdateTimer->stop();
    }
    if (m_dataGenerationTimer) {
        m_dataGenerationTimer->stop();
    }

    // 释放资源
    if (m_component) {
        m_component->stop();
        delete m_component;
        m_component = nullptr;
    }

    if (m_agent) {
        delete m_agent;
        m_agent = nullptr;
    }
}

void MainWindow::initializeUI()
{
    // 创建主布局
    createMainLayout();

    // 创建海图面板
    createSeaChartPanel();

    // 创建控制面板
    createControlPanel();

    // 创建声纳状态面板
    createSonarStatusPanel();

    // 创建日志面板
    createLogPanel();

    // 设置分割器比例 - 调整比例以适应更宽的控制面板
    m_mainSplitter->setSizes({600, 1000});  // 海图:控制面板 = 600:1000
    m_rightSplitter->setSizes({600, 200});  // 控制:日志 = 600:200
}

// 调整分割器比例
void MainWindow::createMainLayout()
{
    // 创建中央部件
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    // 创建主分割器 (水平分割：海图 | 控制面板)
    m_mainSplitter = new QSplitter(Qt::Horizontal, m_centralWidget);

    // 创建右侧分割器 (垂直分割：控制面板 / 日志)
    m_rightSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);

    // 设置主布局
    QHBoxLayout* mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->addWidget(m_mainSplitter);
}

void MainWindow::createSeaChartPanel()
{
    // 创建海图面板容器
    m_seaChartPanel = new QWidget();
    m_seaChartPanel->setMinimumWidth(600);

    // 创建海图组件
    m_seaChartWidget = new SeaChartWidget(m_seaChartPanel);
    m_seaChartWidget->setMainWindow(this);

    // 连接海图信号
    connect(m_seaChartWidget, &SeaChartWidget::targetPlatformsUpdated,
            this, &MainWindow::onTargetPlatformsUpdated);
    connect(m_seaChartWidget, &SeaChartWidget::platformPositionChanged,
            this, &MainWindow::onPlatformPositionChanged);

    // 设置海图面板布局
    QVBoxLayout* seaChartLayout = new QVBoxLayout(m_seaChartPanel);
    seaChartLayout->setContentsMargins(0, 0, 0, 0);
    seaChartLayout->addWidget(m_seaChartWidget);

    // 主分割器
    m_mainSplitter->addWidget(m_seaChartPanel);
}

void MainWindow::createControlPanel()
{
    // 创建控制面板
    m_controlPanel = new QWidget();
    m_controlPanel->setMinimumWidth(800);  // 从400到800
    m_controlPanel->setMaximumWidth(1000); // 从500到1000

    // 创建并保存布局
    m_controlPanelLayout = new QVBoxLayout(m_controlPanel);
    m_controlPanelLayout->setSpacing(10);
    m_controlPanelLayout->setContentsMargins(10, 10, 10, 10);

    // 右侧分割器
    m_rightSplitter->addWidget(m_controlPanel);
}

void MainWindow::createSonarStatusPanel()
{
    // 获取已创建的控制面板布局
//    QVBoxLayout* controlLayout = qobject_cast<QVBoxLayout*>(m_controlPanel->layout());
    QVBoxLayout* controlLayout = m_controlPanelLayout;


    // 如果布局不存在，创建一个
    if (!controlLayout) {
        controlLayout = new QVBoxLayout(m_controlPanel);
        controlLayout->setSpacing(10);
        controlLayout->setContentsMargins(10, 10, 10, 10);
    }

    QPushButton* debugButton = new QPushButton("调试：显示所有目标");
    debugButton->setStyleSheet("background-color: lightcoral;");
    connect(debugButton, &QPushButton::clicked, [this]() {
       if (m_seaChartWidget) {
           // 输出所有平台信息
           auto targets = m_seaChartWidget->getTargetPlatforms();
           addLog(QString("=== 调试信息 ==="));
           addLog(QString("目标数量: %1").arg(targets.size()));

           for (const auto& target : targets) {
               QPoint screenPos = m_seaChartWidget->geoToScreen(target.position);
               addLog(QString("目标: %1, 位置: (%.6f, %.6f), 屏幕: (%2, %3), 可见: %4")
                      .arg(target.name)
                      .arg(target.position.x(), 0, 'f', 6)
                      .arg(target.position.y(), 0, 'f', 6)
                      .arg(screenPos.x())
                      .arg(screenPos.y())
                      .arg(target.isVisible ? "是" : "否"));
           }

           // 自动调整视图
           m_seaChartWidget->fitToTargets();
           addLog("已自动调整视图显示所有目标");
       }
    });

    controlLayout->addWidget(debugButton);

    // === 声纳状态控制组 - 改为两列布局 ===
    m_sonarStatusGroup = new QGroupBox("声纳阵列状态与控制");
    m_sonarStatusGroup->setStyleSheet(
        "QGroupBox { font-weight: bold; font-size: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; padding: 0 5px; }"
    );

    // 修改：使用网格布局替代垂直布局，实现两列排列
    QGridLayout* sonarMainLayout = new QGridLayout(m_sonarStatusGroup);
    sonarMainLayout->setSpacing(10);

    // 为每个声纳创建控制组件 - 按2列排列
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        QFrame* sonarFrame = new QFrame();
        sonarFrame->setFrameStyle(QFrame::Box | QFrame::Raised);
        sonarFrame->setStyleSheet("QFrame { border: 1px solid gray; border-radius: 5px; padding: 5px; }");

        QGridLayout* frameLayout = new QGridLayout(sonarFrame);
        frameLayout->setSpacing(5);

        SonarControlWidget control;

        // 声纳名称和启用开关
        control.nameLabel = new QLabel(SONAR_NAMES[sonarID]);
        control.nameLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: " + SONAR_COLORS[sonarID].name() + ";");
        frameLayout->addWidget(control.nameLabel, 0, 0, 1, 2);

        control.enableCheckBox = new QCheckBox("启用");
        control.enableCheckBox->setChecked(true);
        connect(control.enableCheckBox, &QCheckBox::toggled,
                [this, sonarID](bool checked) { onSonarSwitchToggled(sonarID, checked); });
        frameLayout->addWidget(control.enableCheckBox, 0, 2);

        // 状态指示
        control.statusLabel = new QLabel("正常");
        control.statusLabel->setStyleSheet("color: green; font-weight: bold;");
        frameLayout->addWidget(new QLabel("状态:"), 1, 0);
        frameLayout->addWidget(control.statusLabel, 1, 1, 1, 2);

        // 目标数量显示
        control.targetCountDisplay = new QLCDNumber(2);
        control.targetCountDisplay->setStyleSheet("QLCDNumber { background-color: black; color: lime; }");
        control.targetCountDisplay->setSegmentStyle(QLCDNumber::Flat);
        control.targetCountDisplay->display(0);
        frameLayout->addWidget(new QLabel("目标数:"), 2, 0);
        frameLayout->addWidget(control.targetCountDisplay, 2, 1, 1, 2);

        // 信号强度条
        control.signalStrengthBar = new QProgressBar();
        control.signalStrengthBar->setRange(0, 100);
        control.signalStrengthBar->setValue(0);
        control.signalStrengthBar->setStyleSheet(
            "QProgressBar { border: 2px solid grey; border-radius: 5px; text-align: center; }"
            "QProgressBar::chunk { background-color: " + SONAR_COLORS[sonarID].name() + "; width: 10px; }"
        );
        frameLayout->addWidget(new QLabel("信号强度:"), 3, 0);
        frameLayout->addWidget(control.signalStrengthBar, 3, 1, 1, 2);

        // 方程结果标签
        control.equationResultLabel = new QLabel("等待数据...");
        control.equationResultLabel->setStyleSheet("color: blue; font-size: 10px;");
        control.equationResultLabel->setWordWrap(true);
        frameLayout->addWidget(new QLabel("方程结果:"), 4, 0);
        frameLayout->addWidget(control.equationResultLabel, 4, 1, 1, 2);

        m_sonarControls[sonarID] = control;

        // 计算网格位置：2列排列
        int row = sonarID / 2;    // 行号：0,0,1,1
        int col = sonarID % 2;    // 列号：0,1,0,1
        sonarMainLayout->addWidget(sonarFrame, row, col);
    }

    controlLayout->addWidget(m_sonarStatusGroup);

    // === 创建水平布局容器放置系统状态和声纳方程结果 ===
    QHBoxLayout* horizontalStatsLayout = new QHBoxLayout();

    // === 系统状态组 ===
    m_systemStatusGroup = new QGroupBox("系统状态信息");
    m_systemStatusGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }");
    m_systemStatusGroup->setMinimumWidth(350); // 设置最小宽度

    QGridLayout* systemLayout = new QGridLayout(m_systemStatusGroup);

    // 平台位置
    systemLayout->addWidget(new QLabel("平台位置:"), 0, 0);
    m_platformPositionLabel = new QLabel("126.56°E, 56.65°N");
    m_platformPositionLabel->setStyleSheet("color: blue; font-family: monospace; font-size: 12px;");
    systemLayout->addWidget(m_platformPositionLabel, 0, 1);

    // 目标数量
    systemLayout->addWidget(new QLabel("目标数量:"), 1, 0);
    m_targetCountLabel = new QLabel("0");
    m_targetCountLabel->setStyleSheet("color: red; font-weight: bold; font-size: 14px;");
    systemLayout->addWidget(m_targetCountLabel, 1, 1);

    // 数据状态
    systemLayout->addWidget(new QLabel("数据状态:"), 2, 0);
    m_dataStatusLabel = new QLabel("初始化中...");
    m_dataStatusLabel->setStyleSheet("color: orange; font-size: 12px;");
    systemLayout->addWidget(m_dataStatusLabel, 2, 1);

    // 仿真时间
    systemLayout->addWidget(new QLabel("仿真时间:"), 3, 0);
    m_simulationTimeLabel = new QLabel("00:00:00");
    m_simulationTimeLabel->setStyleSheet("color: green; font-family: monospace; font-size: 14px;");
    systemLayout->addWidget(m_simulationTimeLabel, 3, 1);

    horizontalStatsLayout->addWidget(m_systemStatusGroup);

    // === 声纳方程结果组 ===
    m_equationResultsGroup = new QGroupBox("声纳方程计算结果");
    m_equationResultsGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }");
    m_equationResultsGroup->setMinimumWidth(400); // 设置最小宽度

    QVBoxLayout* resultsLayout = new QVBoxLayout(m_equationResultsGroup);

    // 控制按钮
    QHBoxLayout* resultsButtonLayout = new QHBoxLayout();
    m_refreshResultsButton = new QPushButton("刷新结果");
    m_refreshResultsButton->setStyleSheet("background-color: lightblue;");
    connect(m_refreshResultsButton, &QPushButton::clicked, this, &MainWindow::updateSonarEquationResults);

    m_exportResultsButton = new QPushButton("导出结果");
    m_exportResultsButton->setStyleSheet("background-color: lightgreen;");
    // TODO: 连接导出功能

    resultsButtonLayout->addWidget(m_refreshResultsButton);
    resultsButtonLayout->addWidget(m_exportResultsButton);
    resultsLayout->addLayout(resultsButtonLayout);

    // 结果显示区域
    m_equationResultsDisplay = new QTextEdit();
    m_equationResultsDisplay->setMaximumHeight(180); // 稍微减小高度
    m_equationResultsDisplay->setReadOnly(true);
    m_equationResultsDisplay->setStyleSheet(
        "QTextEdit { font-family: monospace; font-size: 12px; background-color: #f0f0f0; }"
    );
    m_equationResultsDisplay->setPlainText("等待目标数据和声纳方程计算...\n点击\"刷新结果\"查看最新计算结果。");

    resultsLayout->addWidget(m_equationResultsDisplay);

    horizontalStatsLayout->addWidget(m_equationResultsGroup);

    // 将水平布局添加到主布局
    controlLayout->addLayout(horizontalStatsLayout);

    // 阈值配置面板（controlLayout在此作用域内有效）
    createThresholdConfigPanel();
    controlLayout->addWidget(m_thresholdConfigGroup);

    // 声纳范围配置面板
    createSonarRangeConfigPanel();
    controlLayout->addWidget(m_sonarRangeConfigGroup);

    // 弹性空间
    controlLayout->addStretch(1);
}

void MainWindow::createLogPanel()
{
    // 创建日志面板容器
    QWidget* logPanel = new QWidget();

    // 创建日志面板布局
    QVBoxLayout* logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(10, 10, 10, 10);

    // 日志标题
    QLabel* logTitle = new QLabel("系统日志");
    logTitle->setStyleSheet("font-weight: bold; font-size: 18px; color: darkblue;");
    logLayout->addWidget(logTitle);

    // 创建日志文本编辑框
    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setMinimumHeight(150);
    m_logTextEdit->setStyleSheet(
        "QTextEdit { font-family: Consolas, monospace; font-size: 14px; "
        "background-color: #1e1e1e; color: #ffffff; }"
    );

    logLayout->addWidget(m_logTextEdit);




    // 创建日志控制按钮
    QHBoxLayout* logButtonLayout = new QHBoxLayout();

    m_clearLogButton = new QPushButton("清除日志");
    m_clearLogButton->setStyleSheet("background-color: #ff6b6b; color: white;");
    connect(m_clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);

    // 切换文件日志按钮
    m_toggleFileLogButton = new QPushButton("暂停文件日志");
    m_toggleFileLogButton->setStyleSheet("background-color: #45b7d1; color: white;");
    connect(m_toggleFileLogButton, &QPushButton::clicked, this, &MainWindow::onToggleFileLogClicked);

    logButtonLayout->addStretch(1);
    logButtonLayout->addWidget(m_toggleFileLogButton);
    logButtonLayout->addWidget(m_clearLogButton);

    logLayout->addLayout(logButtonLayout);



    // 右侧分割器
    m_rightSplitter->addWidget(logPanel);
}

void MainWindow::initializeTimers()
{
    // 状态更新定时器 (每秒更新)
    m_statusUpdateTimer = new QTimer(this);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &MainWindow::onUpdateSonarStatus);
    m_statusUpdateTimer->start(1000); // 1秒间隔

    // 数据生成定时器 (每5秒生成数据)
    m_dataGenerationTimer = new QTimer(this);
    connect(m_dataGenerationTimer, &QTimer::timeout, this, &MainWindow::onDataGenerationTimer);
    m_dataGenerationTimer->start(5000); // 5秒间隔

    //初始化时同步声纳状态到海图
    if (m_seaChartWidget) {
        for (int sonarID = 0; sonarID < 4; sonarID++) {
            bool enabled = m_sonarEnabledStates.value(sonarID, true);
            m_seaChartWidget->setSonarRangeVisible(sonarID, enabled);
        }
        addLog("已同步声纳状态到海图显示");
    }

    addLog("定时器初始化完成 - 状态更新: 1秒, 数据生成: 5秒");
}

// mainwindow.cpp - 槽函数和数据生成逻辑实现

// ========== 槽函数实现 ==========

void MainWindow::onSonarSwitchToggled(int sonarID, bool enabled)
{
    if (sonarID < 0 || sonarID >= 4)
    {
        return;
    }

    m_sonarEnabledStates[sonarID] = enabled;

    // 发送声纳控制命令
    sendSonarControlOrder(sonarID, enabled);

    // 新增：同步控制海图上的声纳范围显示
    if (m_seaChartWidget) {
        m_seaChartWidget->setSonarRangeVisible(sonarID, enabled);
        addLog(QString("海图上声纳 %1 覆盖范围已%2").arg(sonarID).arg(enabled ? "显示" : "隐藏"));
    }

    // 更新状态显示
    auto& control = m_sonarControls[sonarID];
    control.statusLabel->setText(enabled ? "正常" : "关闭");
    control.statusLabel->setStyleSheet(enabled ? "color: green; font-weight: bold;" : "color: red; font-weight: bold;");

    if (!enabled) {
        control.targetCountDisplay->display(0);
        control.signalStrengthBar->setValue(0);
        control.equationResultLabel->setText("声纳已关闭");
    }

    addLog(QString("声纳 %1 (%2) 已%3")
           .arg(sonarID)
           .arg(SONAR_NAMES[sonarID])
           .arg(enabled ? "启用" : "禁用"));
}

void MainWindow::onTargetPlatformsUpdated(const QVector<ChartPlatform>& targetPlatforms)
{
    m_currentTargets = targetPlatforms;

    // 更新目标数量显示
    m_targetCountLabel->setText(QString::number(targetPlatforms.size()));
    m_targetCountLabel->setStyleSheet(
        targetPlatforms.isEmpty() ? "color: gray;" :
        targetPlatforms.size() > 8 ? "color: red; font-weight: bold;" : "color: green; font-weight: bold;"
    );

    // 无论目标列表是否为空，都发送数据
    generateAndSendPropagatedSoundData(targetPlatforms);

    if (!targetPlatforms.isEmpty()) {
        addLog(QString("已更新 %1 个目标平台的传播声数据").arg(targetPlatforms.size()));
    } else {
        addLog("已清空所有目标的传播声数据");
    }

    // 更新数据状态
    if (m_platformSelfSoundSent && m_environmentNoiseSent && !targetPlatforms.isEmpty()) {
        m_dataStatusLabel->setText("数据完整");
        m_dataStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_dataStatusLabel->setText("数据不完整");
        m_dataStatusLabel->setStyleSheet("color: orange;");
    }
}

void MainWindow::onPlatformPositionChanged(int platformId, const QPointF& newPosition)
{
    Q_UNUSED(platformId);

    // 更新平台位置显示
    m_platformPositionLabel->setText(QString("%1°E, %2°N")
                                     .arg(newPosition.x(), 0, 'f', 4)
                                     .arg(newPosition.y(), 0, 'f', 4));

    // 格式化：
    addLog(QString("平台位置更新: %1°E, %2°N")
           .arg(newPosition.x(), 0, 'f', 4)
           .arg(newPosition.y(), 0, 'f', 4));
}

void MainWindow::onClearLogClicked()
{
    if (m_logTextEdit) {
        m_logTextEdit->clear();
        addLog("日志已清除");
    }
}

void MainWindow::onUpdateSonarStatus()
{
    // 更新仿真时间
    static int simulationSeconds = 0;
    simulationSeconds++;
    int hours = simulationSeconds / 3600;
    int minutes = (simulationSeconds % 3600) / 60;
    int seconds = simulationSeconds % 60;

    m_simulationTimeLabel->setText(QString("%1:%2:%3")
                                   .arg(hours, 2, 10, QChar('0'))
                                   .arg(minutes, 2, 10, QChar('0'))
                                   .arg(seconds, 2, 10, QChar('0')));

    // 更新声纳状态显示
    updateSonarStatusDisplay();
}



void MainWindow::onDataGenerationTimer()
{
    // 每5秒更新一次平台机动数据（包括朝向信息）
    generateAndSendPlatformMotionData();

    // 每5秒触发一次，生成新的传播声数据
    if (!m_currentTargets.isEmpty() && m_component) {
        generateAndSendPropagatedSoundData(m_currentTargets);

        // 触发声纳模型的step方法
        int64 currentTime = QDateTime::currentMSecsSinceEpoch();
        m_component->step(currentTime, 5000); // 5秒步长

        // 延迟更新结果显示，给声纳模型时间处理数据
        QTimer::singleShot(500, this, &MainWindow::updateSonarEquationResults);
    }
}




// ========== 辅助函数实现 ==========

void MainWindow::addLog(const QString& message)
{
    // 防御性检查：确保UI组件已经初始化
    if (!m_logTextEdit) {
        // 如果UI还没初始化，先输出到控制台
        qDebug() << "[EARLY LOG]:" << message;

        // 可以选择将早期的日志保存到队列中，待UI初始化后再显示
        static QStringList earlyLogs;
        earlyLogs.append(QString("[%1] %2")
                        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                        .arg(message));

        // 如果队列太长，移除最老的日志
        if (earlyLogs.size() > 100) {
            earlyLogs.removeFirst();
        }

        return;
    }

    // 正常的日志记录逻辑
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp).arg(message);

    // 安全地添加到UI
    m_logTextEdit->append(logEntry);

    // 自动滚动到底部
    QScrollBar* scrollBar = m_logTextEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());

    // 可选：同时输出到控制台
    qDebug() << logEntry;

    // 如果有早期日志队列，第一次正常记录时将它们都显示出来
    static bool hasDisplayedEarlyLogs = false;
    if (!hasDisplayedEarlyLogs) {
        static QStringList earlyLogs;
        for (const QString& earlyLog : earlyLogs) {
            m_logTextEdit->append(earlyLog);
        }
        earlyLogs.clear();
        hasDisplayedEarlyLogs = true;
    }
}

void MainWindow::updateSonarStatusDisplay()
{
    if (!m_component) return;

    // 将组件转换为DeviceModel以访问多目标结果
    DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
    if (!deviceModel) return;

    // 获取所有声纳的目标结果
    auto allResults = deviceModel->getAllSonarTargetsResults();

    for (int sonarID = 0; sonarID < 4; sonarID++) {
        auto& control = m_sonarControls[sonarID];

        if (!m_sonarEnabledStates[sonarID]) {
            // 声纳未启用
            control.targetCountDisplay->display(0);
            control.signalStrengthBar->setValue(0);
            control.equationResultLabel->setText("声纳已关闭");
            continue;
        }

        // 获取该声纳的目标结果
        auto it = allResults.find(sonarID);
        if (it != allResults.end()) {
            const auto& targets = it->second;

            // 统计有效目标数量
            int validTargetCount = 0;
            double maxEquationResult = 0.0;
            QString resultText = "";

            for (const auto& target : targets) {
                if (target.isValid) {
                    validTargetCount++;
                    maxEquationResult = std::max(maxEquationResult, target.equationResult);

                    if (resultText.length() < 100) { // 限制显示长度
                        resultText += QString("T%1:%2 ")
                                     .arg(target.targetId)
                                     .arg(target.equationResult, 0, 'f', 1);
                    }
                }
            }

            // 更新目标数量显示
            control.targetCountDisplay->display(validTargetCount);

            // 更新信号强度条 (基于最大方程结果)
            int strengthValue = static_cast<int>(std::min(100.0, std::max(0.0, maxEquationResult * 2))); // 假设50为满强度
            control.signalStrengthBar->setValue(strengthValue);

            // 更新方程结果显示
            if (validTargetCount > 0) {
                if (resultText.isEmpty()) {
                    control.equationResultLabel->setText(QString("%1个目标").arg(validTargetCount));
                } else {
                    control.equationResultLabel->setText(resultText.trimmed());
                }
            } else {
                control.equationResultLabel->setText("无有效目标");
            }

        } else {
            // 没有结果数据
            control.targetCountDisplay->display(0);
            control.signalStrengthBar->setValue(0);
            control.equationResultLabel->setText("等待数据...");
        }
    }
}

void MainWindow::sendSonarControlOrder(int sonarID, bool enabled)
{
    if (!m_component) return;

    try {
        // 创建声纳控制命令
        CMsg_SonarCommandControlOrder order;
        order.sonarID = sonarID;
        order.arrayWorkingOrder = enabled ? 1 : 0;
        order.passiveWorkingOrder = enabled ? 1 : 0;
        order.activeWorkingOrder = enabled ? 1 : 0;
        order.scoutingWorkingOrder = enabled ? 1 : 0;
        order.multiSendWorkingOrder = 0;       // 默认关闭
        order.multiReceiveWorkingOrder = 0;    // 默认关闭
        order.activeTransmitWorkingOrder = 0;  // 默认关闭

        // 创建控制消息
        CSimMessage controlMsg;
        controlMsg.dataFormat = STRUCT;
        controlMsg.time = QDateTime::currentMSecsSinceEpoch();
        controlMsg.sender = 1;
        controlMsg.senderComponentId = 1;
        controlMsg.receiver = 1;
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

// ========== 数据生成函数实现 ==========

void MainWindow::generateAndSendPropagatedSoundData(const QVector<ChartPlatform>& targetPlatforms)
{
    if (!m_component || !m_agent) {
        return;
    }

    try {
        // 即使目标列表为空，也创建并发送数据结构
        CMsg_PropagatedContinuousSoundListStruct continuousSound = createPropagatedSoundData(targetPlatforms);

        // 创建消息
        CSimMessage continuousMsg;
        continuousMsg.dataFormat = STRUCT;
        continuousMsg.time = QDateTime::currentMSecsSinceEpoch();
        continuousMsg.sender = 1;
        continuousMsg.senderComponentId = 1;
        continuousMsg.receiver = 1;
        continuousMsg.data = &continuousSound;
        continuousMsg.length = sizeof(continuousSound);
        memcpy(continuousMsg.topic, MSG_PropagatedContinuousSound, strlen(MSG_PropagatedContinuousSound) + 1);

        // 发送给声纳模型
        m_component->onMessage(&continuousMsg);

        if (targetPlatforms.isEmpty()) {
            addLog("已发送空的传播声数据（清空目标）");
        } else {
            addLog(QString("已生成并发送 %1 个目标的传播声数据").arg(targetPlatforms.size()));
        }

    } catch(const std::exception& e) {
        addLog(QString("生成传播声数据时出错: %1").arg(e.what()));
    } catch(...) {
        addLog("生成传播声数据时发生未知错误");
    }
}

CMsg_PropagatedContinuousSoundListStruct MainWindow::createPropagatedSoundData(const QVector<ChartPlatform>& targetPlatforms)
{
    CMsg_PropagatedContinuousSoundListStruct soundListStruct;

    // 获取本艇位置
    QPointF ownShipPos = m_seaChartWidget->getOwnShipPosition();

    // 随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> signalDist(60.0f, 85.0f); // 信号强度范围

    for (const auto& target : targetPlatforms) {
        C_PropagatedContinuousSoundStruct soundData;

        // 计算目标相对于本艇的位置参数
        double deltaLon = target.position.x() - ownShipPos.x();
        double deltaLat = target.position.y() - ownShipPos.y();

        // 计算距离 (简化计算，单位：米)
        double deltaXMeters = deltaLon * 111320.0; // 1度经度约111320米
        double deltaYMeters = deltaLat * 111320.0; // 1度纬度约111320米
        double distance = sqrt(deltaXMeters * deltaXMeters + deltaYMeters * deltaYMeters);

        // 计算方位角 (相对于北向，顺时针为正)
        double bearing = atan2(deltaXMeters, deltaYMeters) * 180.0 / M_PI;
        if (bearing < 0) bearing += 360.0;

        // 设置目标参数
        soundData.targetDistance = static_cast<float>(distance);
        soundData.arrivalSideAngle = static_cast<float>(bearing);
        soundData.arrivalPitchAngle = 0.0f; // 假设目标在同一水平面
        soundData.platType = 1; // 潜艇类型

        // 生成模拟的频谱数据
        // 根据距离调整信号强度：距离越远，信号越弱
        float baseSignalLevel = 80.0f - static_cast<float>(distance / 1000.0f); // 每公里衰减1dB
        baseSignalLevel = std::max(40.0f, std::min(85.0f, baseSignalLevel)); // 限制在40-85dB范围

        for (int i = 0; i < 5296; i++) {
            // 一些随机变化和频率特性
            float freqFactor = 1.0f + 0.1f * sin(i * 0.01f); // 模拟频率响应
            float randomVariation = signalDist(gen) * 0.1f - 0.05f; // ±5%随机变化

            soundData.spectrumData[i] = baseSignalLevel * freqFactor + randomVariation;
        }

        soundListStruct.propagatedContinuousList.push_back(soundData);

        addLog(QString("目标 %1: 距离=%2km, 方位=%3°, 信号强度=%4dB")
               .arg(target.name)
               .arg(distance / 1000.0, 0, 'f', 1)
               .arg(bearing, 0, 'f', 1)
               .arg(baseSignalLevel, 0, 'f', 1));
    }

    return soundListStruct;
}

void MainWindow::generateAndSendPlatformSelfSoundData()
{
    if (!m_component || !m_agent || m_platformSelfSoundSent) {
        return; // 平台自噪声只需要发送一次
    }

    try {
        // 创建平台自噪声数据
        CData_PlatformSelfSound* platformSelfSound = createPlatformSelfSoundData();

        // 创建 CSimData 包装器
        CSimData* selfSoundData = new CSimData();
        selfSoundData->dataFormat = STRUCT;
        selfSoundData->time = QDateTime::currentMSecsSinceEpoch();
        selfSoundData->sender = m_agent->getPlatformEntity()->id;
        selfSoundData->receiver = m_agent->getPlatformEntity()->id;
        selfSoundData->componentId = 1;
        selfSoundData->data = platformSelfSound; // 直接赋值，不需要release()
        selfSoundData->length = sizeof(CData_PlatformSelfSound);
        memcpy(selfSoundData->topic, Data_PlatformSelfSound, strlen(Data_PlatformSelfSound) + 1);

        // 存储到agent中（agent会负责内存管理）
        int64 platformId = m_agent->getPlatformEntity()->id;
        m_agent->addSubscribedData(Data_PlatformSelfSound, platformId, selfSoundData);

        m_platformSelfSoundSent = true;
        addLog("已生成并存储平台自噪声数据（4个声纳位置）");

    } catch(const std::exception& e) {
        addLog(QString("生成平台自噪声数据时出错: %1").arg(e.what()));
    } catch(...) {
        addLog("生成平台自噪声数据时发生未知错误");
    }
}

CData_PlatformSelfSound* MainWindow::createPlatformSelfSoundData()
{
    CData_PlatformSelfSound* platformSelfSound = new CData_PlatformSelfSound();

    // 随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());

    // 修正：只为4个声纳位置创建自噪声数据（ID 0-3）
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        C_SelfSoundSpectrumStruct spectrumStruct;
        spectrumStruct.sonarID = sonarID;

        // 根据声纳类型设置不同的噪声特性
        float baseNoiseLevel = 35.0f; // 基础噪声级别
        float noiseVariation = 10.0f; // 噪声变化范围

        switch (sonarID) {
            case 0: // 艏端声纳 - 受机械噪声影响较大
                baseNoiseLevel = 40.0f;
                noiseVariation = 8.0f;
                break;
            case 1: // 舷侧声纳 - 中等噪声级别
                baseNoiseLevel = 35.0f;
                noiseVariation = 6.0f;
                break;
            case 2: // 粗拖声纳 - 远离船体，噪声较低
                baseNoiseLevel = 30.0f;
                noiseVariation = 5.0f;
                break;
            case 3: // 细拖声纳 - 噪声最低
                baseNoiseLevel = 28.0f;
                noiseVariation = 4.0f;
                break;
        }

        std::uniform_real_distribution<float> noiseDist(
            baseNoiseLevel - noiseVariation/2,
            baseNoiseLevel + noiseVariation/2
        );

        // 生成频谱数据
        for (int i = 0; i < 5296; i++) {
            // 频率相关的噪声特性
            float freqFactor = 1.0f - 0.3f * (i / 5296.0f); // 高频噪声衰减
            spectrumStruct.spectumData[i] = noiseDist(gen) * freqFactor;
        }

        platformSelfSound->selfSoundSpectrumList.push_back(spectrumStruct);

        addLog(QString("声纳%1自噪声: 基准%2dB±%3dB")
               .arg(sonarID)
               .arg(baseNoiseLevel, 0, 'f', 1)
               .arg(noiseVariation, 0, 'f', 1));
    }

    return platformSelfSound;
}

void MainWindow::generateAndSendEnvironmentNoiseData()
{
    if (!m_component || m_environmentNoiseSent) {
        return; // 环境噪声只需要发送一次
    }

    try {
        // 创建环境噪声数据
        CMsg_EnvironmentNoiseToSonarStruct envNoise = createEnvironmentNoiseData();

        // 创建消息
        CSimMessage envMsg;
        envMsg.dataFormat = STRUCT;
        envMsg.time = QDateTime::currentMSecsSinceEpoch();
        envMsg.sender = 1;
        envMsg.senderComponentId = 1;
        envMsg.receiver = 1;
        envMsg.data = &envNoise;
        envMsg.length = sizeof(envNoise);
        memcpy(envMsg.topic, MSG_EnvironmentNoiseToSonar, strlen(MSG_EnvironmentNoiseToSonar) + 1);

        // 发送给声纳模型
        m_component->onMessage(&envMsg);

        m_environmentNoiseSent = true;
        addLog("已生成并发送海洋环境噪声数据");

    } catch(const std::exception& e) {
        addLog(QString("生成环境噪声数据时出错: %1").arg(e.what()));
    } catch(...) {
        addLog("生成环境噪声数据时发生未知错误");
    }
}

CMsg_EnvironmentNoiseToSonarStruct MainWindow::createEnvironmentNoiseData()
{
    CMsg_EnvironmentNoiseToSonarStruct envNoise;

    // 设置环境参数
    envNoise.acousticVel = 1500.0f; // 声速 m/s

    // 生成模拟的环境噪声频谱数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> envNoiseDist(20.0f, 35.0f);  // 20-35 dB

    for (int i = 0; i < 5296; i++) {
        // 模拟海洋环境噪声的频率特性
        // 低频噪声较高，高频噪声较低
        float freqRatio = static_cast<float>(i) / 5296.0f;
        float freqFactor = 1.5f - freqRatio; // 高频衰减

        envNoise.spectrumData[i] = envNoiseDist(gen) * freqFactor;
    }

    addLog("环境噪声特性: 20-35dB，低频偏高，高频衰减");
    return envNoise;
}

void MainWindow::updateSonarEquationResults()
{
    if (!m_component) {
        m_equationResultsDisplay->setPlainText("错误：声纳组件未初始化");
        return;
    }

    // 将组件转换为 DeviceModel* 以访问多目标结果
    DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
    if (!deviceModel) {
        m_equationResultsDisplay->setPlainText("错误：无法转换为 DeviceModel 类型");
        return;
    }

    // 获取所有声纳的计算结果
    auto allResults = deviceModel->getAllSonarTargetsResults();

    QString resultText = formatSonarResults(allResults);
    m_equationResultsDisplay->setPlainText(resultText);

    addLog("已更新声纳方程计算结果显示");
}

QString MainWindow::formatSonarResults(const std::map<int, std::vector<DeviceModel::TargetEquationResult>>& allResults)
{
    QString resultText;
    resultText += "============ 多目标声纳方程计算结果 ============\n";
    resultText += QString("更新时间：%1\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    resultText += "公式：SL-TL-NL+DI=X (每个目标独立计算)\n\n";

    if (allResults.empty()) {
        resultText += "暂无有效的计算结果\n";
        resultText += "可能原因：\n";
        resultText += "1. 没有目标平台数据\n";
        resultText += "2. 声纳未启用\n";
        resultText += "3. 基础数据未准备完整\n\n";
        resultText += "建议：在海图上添加目标舰船，确保声纳已启用。";
        return resultText;
    }

    int totalValidTargets = 0;

    for (int sonarID = 0; sonarID < 4; sonarID++) {
        resultText += QString("%1 (ID=%2):\n").arg(SONAR_NAMES[sonarID]).arg(sonarID);

        auto it = allResults.find(sonarID);
        if (it != allResults.end()) {
            const auto& targets = it->second;

            if (targets.empty()) {
                resultText += " 无目标数据\n";
            } else {
                int validCount = 0;
                for (const auto& target : targets) {
                    if (target.isValid) {
                        validCount++;
                        totalValidTargets++;
                        resultText += QString(" 目标%1: X=%2 (距离=%3km, 方位=%4°)\n")
                                     .arg(target.targetId)
                                     .arg(target.equationResult, 0, 'f', 2)
                                     .arg(target.targetDistance / 1000.0, 0, 'f', 1)
                                     .arg(target.targetBearing, 0, 'f', 1);
                    } else {
                        resultText += QString(" 目标%1: 计算失败\n").arg(target.targetId);
                    }
                }

                if (validCount == 0) {
                    resultText += " 所有目标计算失败\n";
                } else {
                    resultText += QString(" 有效目标: %1/%2\n").arg(validCount).arg(targets.size());
                }
            }
        } else {
            if (!m_sonarEnabledStates[sonarID]) {
                resultText += " 声纳已禁用\n";
            } else {
                resultText += " 无计算结果\n";
            }
        }
        resultText += "\n";
    }

    // 统计信息
    resultText += QString("总计有效目标检测: %1\n").arg(totalValidTargets);
    resultText += "\n说明：\n";
    resultText += "• X值越大表示目标信号相对噪声越强\n";
    resultText += "• 每个声纳最多可同时跟踪8个目标\n";
    resultText += "• 超出范围的目标不会被计算\n";
    resultText += "• 数据每5秒自动更新一次";

    return resultText;
}


void MainWindow::generateAndSendPlatformMotionData()
{
    if (!m_component || !m_agent || !m_seaChartWidget) {
        return;
    }

    try {
        // 使用安全的方法获取本艇信息
        if (!m_seaChartWidget->isOwnShipValid()) {
            addLog("警告：本艇信息无效，跳过平台机动数据更新");
            return;
        }

        QPointF ownShipPos = m_seaChartWidget->getOwnShipPosition();
        double ownShipHeading = m_seaChartWidget->getOwnShipHeading();
        double ownShipSpeed = m_seaChartWidget->getOwnShipSpeed();

        // 创建平台机动数据 - 使用栈分配而不是堆分配
        CData_Motion motionData; // 不使用new
        motionData.name = nullptr;
        motionData.action = true;
        motionData.isPending = false;
        motionData.x = ownShipPos.x();
        motionData.y = ownShipPos.y();
        motionData.z = -200.0;
        motionData.curSpeed = ownShipSpeed;
        motionData.rotation = ownShipHeading;
        motionData.mVerticalSpeed = 0.0;
        motionData.mAcceleration = 0.0;
        motionData.roll = 0.0;
        motionData.yaw = 0.0;
        motionData.pitch = 0.0;
        motionData.mRollVel = 0.0;
        motionData.mPitchVel = 0.0;
        motionData.mYawVel = 0.0;

        // 创建 CSimData 包装器
        CSimData* simData = new CSimData();
        simData->dataFormat = STRUCT;
        simData->time = QDateTime::currentMSecsSinceEpoch();
        simData->sender = m_agent->getPlatformEntity()->id;
        simData->receiver = m_agent->getPlatformEntity()->id;
        simData->componentId = 1;

        // 创建motionData的深拷贝
        CData_Motion* motionDataCopy = new CData_Motion(motionData);
        simData->data = motionDataCopy;
        simData->length = sizeof(CData_Motion);
        memcpy(simData->topic, Data_Motion, strlen(Data_Motion) + 1);

        // 存储到agent中
        int64 platformId = m_agent->getPlatformEntity()->id;
        m_agent->addSubscribedData(Data_Motion, platformId, simData);

        addLog(QString("已更新平台机动数据 - 位置:(%1, %2), 朝向:%3°, 速度:%4节")
               .arg(ownShipPos.x(), 0, 'f', 4)
               .arg(ownShipPos.y(), 0, 'f', 4)
               .arg(ownShipHeading, 0, 'f', 1)
               .arg(ownShipSpeed, 0, 'f', 1));

    } catch(const std::exception& e) {
        addLog(QString("生成平台机动数据时出错: %1").arg(e.what()));
    } catch(...) {
        addLog("生成平台机动数据时发生未知错误");
    }
}

// 声纳阈值配置面板
void MainWindow::createThresholdConfigPanel()
{
    // === 阈值配置组 ===
    m_thresholdConfigGroup = new QGroupBox("声纳探测阈值配置");
    m_thresholdConfigGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }");

    QVBoxLayout* thresholdLayout = new QVBoxLayout(m_thresholdConfigGroup);

//    QLabel* individualLabel = new QLabel("各声纳探测阈值:");
//    individualLabel->setStyleSheet("font-weight: bold; color: blue; font-size: 14px;");
//    thresholdLayout->addWidget(individualLabel);

    // 使用1行4列网格布局 - 每个声纳独立一列
    QGridLayout* sonarThresholdLayout = new QGridLayout();
    sonarThresholdLayout->setSpacing(12); // 增加列间距

    // 设置4列等宽
    for (int i = 0; i < 4; i++) {
        sonarThresholdLayout->setColumnStretch(i, 1);
    }

    for (int sonarID = 0; sonarID < 4; sonarID++) {
        SonarThresholdWidget thresholdWidget;

        // 每个声纳占一列：第0行，第sonarID列
        int row = 0;
        int col = sonarID;

        // 为每个声纳创建一个容器框架
        QFrame* thresholdFrame = new QFrame();
        thresholdFrame->setFrameStyle(QFrame::Box);
        thresholdFrame->setStyleSheet("QFrame { border: 1px solid lightgray; border-radius: 5px; padding: 12px; }");
        thresholdFrame->setMinimumWidth(120);  // 适中的最小宽度
        thresholdFrame->setMinimumHeight(100); // 足够的高度

        QVBoxLayout* frameLayout = new QVBoxLayout(thresholdFrame);
        frameLayout->setSpacing(4);

        // 声纳名称标签 - 居中显示
        thresholdWidget.nameLabel = new QLabel(SONAR_NAMES[sonarID]);
        thresholdWidget.nameLabel->setStyleSheet("color: " + SONAR_COLORS[sonarID].name() + "; font-weight: bold; font-size: 13px;");
        thresholdWidget.nameLabel->setAlignment(Qt::AlignCenter);
        frameLayout->addWidget(thresholdWidget.nameLabel);

        // 阈值标签
        QLabel* thresholdLabel = new QLabel("阈值:");
        thresholdLabel->setStyleSheet("font-size: 12px; color: #666;");
        thresholdLabel->setAlignment(Qt::AlignCenter);
        frameLayout->addWidget(thresholdLabel);

        // 阈值输入框 - 居中
        thresholdWidget.thresholdSpinBox = new QDoubleSpinBox();
        thresholdWidget.thresholdSpinBox->setRange(0.0, 200.0);
        thresholdWidget.thresholdSpinBox->setSingleStep(0.1);
        thresholdWidget.thresholdSpinBox->setDecimals(2);
        thresholdWidget.thresholdSpinBox->setMinimumWidth(80);
        thresholdWidget.thresholdSpinBox->setAlignment(Qt::AlignCenter);

        // 设置默认值
        double defaultValues[] = {35.0, 33.0, 43.0, 43.0};
        thresholdWidget.thresholdSpinBox->setValue(defaultValues[sonarID]);
        thresholdWidget.thresholdSpinBox->setSuffix(" dB");

        // 连接信号
        connect(thresholdWidget.thresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, sonarID](double value) { onSonarThresholdChanged(sonarID, value); });

        frameLayout->addWidget(thresholdWidget.thresholdSpinBox, 0, Qt::AlignCenter);

        // 状态标签 - 居中显示
        thresholdWidget.statusLabel = new QLabel(QString("当前: %1").arg(defaultValues[sonarID], 0, 'f', 2));
        thresholdWidget.statusLabel->setStyleSheet("color: blue; font-size: 10px;");
        thresholdWidget.statusLabel->setAlignment(Qt::AlignCenter);
        frameLayout->addWidget(thresholdWidget.statusLabel);

        sonarThresholdLayout->addWidget(thresholdFrame, row, col);
        m_thresholdControls[sonarID] = thresholdWidget;
    }

    thresholdLayout->addLayout(sonarThresholdLayout);

    // 控制按钮 - 水平排列，居中
    QHBoxLayout* thresholdButtonLayout = new QHBoxLayout();
    thresholdButtonLayout->addStretch(); // 左侧弹性空间

    m_saveThresholdConfigButton = new QPushButton("保存配置");
    m_saveThresholdConfigButton->setStyleSheet("background-color: lightgreen; padding: 6px 12px; font-size: 12px;");
    connect(m_saveThresholdConfigButton, &QPushButton::clicked,
            this, &MainWindow::onSaveThresholdConfig);

    m_loadThresholdConfigButton = new QPushButton("加载配置");
    m_loadThresholdConfigButton->setStyleSheet("background-color: lightblue; padding: 6px 12px; font-size: 12px;");
    connect(m_loadThresholdConfigButton, &QPushButton::clicked,
            this, &MainWindow::onLoadThresholdConfig);

    m_resetThresholdButton = new QPushButton("重置阈值");
    m_resetThresholdButton->setStyleSheet("background-color: lightyellow; padding: 6px 12px; font-size: 12px;");
    connect(m_resetThresholdButton, &QPushButton::clicked,
            this, &MainWindow::onResetThreshold);

    thresholdButtonLayout->addWidget(m_saveThresholdConfigButton);
    thresholdButtonLayout->addWidget(m_loadThresholdConfigButton);
    thresholdButtonLayout->addWidget(m_resetThresholdButton);
    thresholdButtonLayout->addStretch(); // 右侧弹性空间

    thresholdLayout->addLayout(thresholdButtonLayout);
}

// 声纳范围配置面板
void MainWindow::createSonarRangeConfigPanel()
{
    // === 声纳范围配置组 ===
    m_sonarRangeConfigGroup = new QGroupBox("声纳最大探测距离配置");
    m_sonarRangeConfigGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 14px; }");

    QVBoxLayout* rangeLayout = new QVBoxLayout(m_sonarRangeConfigGroup);

    // 使用1行4列网格布局
    QGridLayout* sonarRangeLayout = new QGridLayout();
    sonarRangeLayout->setSpacing(4);

    // 设置4列等宽
    for (int i = 0; i < 4; i++) {
        sonarRangeLayout->setColumnStretch(i, 1);
    }

    for (int sonarID = 0; sonarID < 4; sonarID++) {
        SonarRangeWidget rangeWidget;

        // 每个声纳占一列
        int row = 0;
        int col = sonarID;

        // 为每个声纳创建一个容器框架
        QFrame* rangeFrame = new QFrame();
        rangeFrame->setFrameStyle(QFrame::Box);
        rangeFrame->setStyleSheet("QFrame { border: 1px solid lightgray; border-radius: 5px; padding: 12px; }");
        rangeFrame->setMinimumWidth(120);
        rangeFrame->setMinimumHeight(100);

        QVBoxLayout* frameLayout = new QVBoxLayout(rangeFrame);
        frameLayout->setSpacing(4);

        // 声纳名称标签
        rangeWidget.nameLabel = new QLabel(SONAR_NAMES[sonarID]);
        rangeWidget.nameLabel->setStyleSheet("color: " + SONAR_COLORS[sonarID].name() + "; font-weight: bold; font-size: 13px;");
        rangeWidget.nameLabel->setAlignment(Qt::AlignCenter);
        frameLayout->addWidget(rangeWidget.nameLabel);

        // 距离标签
        QLabel* distanceLabel = new QLabel("最大距离:");
        distanceLabel->setStyleSheet("font-size: 12px; color: #666;");
        distanceLabel->setAlignment(Qt::AlignCenter);
        frameLayout->addWidget(distanceLabel);

        // 最大距离输入框
        rangeWidget.rangeSpinBox = new QDoubleSpinBox();
        rangeWidget.rangeSpinBox->setRange(1000.0, 100000.0);
        rangeWidget.rangeSpinBox->setSingleStep(1000.0);
        rangeWidget.rangeSpinBox->setDecimals(0);
        rangeWidget.rangeSpinBox->setValue(m_sonarRangeConfigs[sonarID].maxRange);
        rangeWidget.rangeSpinBox->setSuffix(" 米");
        rangeWidget.rangeSpinBox->setMinimumWidth(80);
        rangeWidget.rangeSpinBox->setAlignment(Qt::AlignCenter);

        // 连接信号
        connect(rangeWidget.rangeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, sonarID](double value) { onSonarRangeChanged(sonarID, value); });

        frameLayout->addWidget(rangeWidget.rangeSpinBox, 0, Qt::AlignCenter);

        // 状态标签
        rangeWidget.statusLabel = new QLabel(QString("当前: %1m").arg(m_sonarRangeConfigs[sonarID].maxRange, 0, 'f', 0));
        rangeWidget.statusLabel->setStyleSheet("color: purple; font-size: 10px;");
        rangeWidget.statusLabel->setAlignment(Qt::AlignCenter);
        frameLayout->addWidget(rangeWidget.statusLabel);

        sonarRangeLayout->addWidget(rangeFrame, row, col);
        m_rangeControls[sonarID] = rangeWidget;
    }

    rangeLayout->addLayout(sonarRangeLayout);

    // 控制按钮 - 居中排列
    QHBoxLayout* rangeButtonLayout = new QHBoxLayout();
    rangeButtonLayout->addStretch();

    m_saveRangeConfigButton = new QPushButton("保存范围配置");
    m_saveRangeConfigButton->setStyleSheet("background-color: lightcoral; padding: 6px 12px; font-size: 12px;");
    connect(m_saveRangeConfigButton, &QPushButton::clicked,
            this, &MainWindow::onSaveRangeConfig);

    m_resetRangeConfigButton = new QPushButton("重置范围配置");
    m_resetRangeConfigButton->setStyleSheet("background-color: lightgray; padding: 6px 12px; font-size: 12px;");
    connect(m_resetRangeConfigButton, &QPushButton::clicked,
            this, &MainWindow::onResetRangeConfig);

    rangeButtonLayout->addWidget(m_saveRangeConfigButton);
    rangeButtonLayout->addWidget(m_resetRangeConfigButton);
    rangeButtonLayout->addStretch();

    rangeLayout->addLayout(rangeButtonLayout);
}

void MainWindow::onToggleFileLogClicked()
{
    m_fileLogEnabled = !m_fileLogEnabled;

    // 设置两个Logger实例
    Logger::getInstance().enableFileOutput(m_fileLogEnabled);

    // 通过 DeviceModel 设置它的 Logger
    if (m_component) {
        DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
        if (deviceModel) {
            deviceModel->setFileLogEnabled(m_fileLogEnabled);
        }
    }

    // 更新按钮
    if (m_fileLogEnabled) {
        m_toggleFileLogButton->setText("暂停文件日志");
        m_toggleFileLogButton->setStyleSheet("background-color: #45b7d1; color: white;");
        addLog(" 文件日志输出已启用");
    } else {
        m_toggleFileLogButton->setText("恢复文件日志");
        m_toggleFileLogButton->setStyleSheet("background-color: #95a5a6; color: white;");
        addLog(" 文件日志输出已暂停");
    }
}

void MainWindow::onSonarThresholdChanged(int sonarID, double value)
{
    if (m_component) {
        DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
        if (deviceModel) {
            deviceModel->setSonarDetectionThreshold(sonarID, value);
            updateThresholdDisplay();
            addLog(QString("设置%1探测阈值: %2 dB")
                   .arg(SONAR_NAMES[sonarID])
                   .arg(value, 0, 'f', 2));
        }
    }
}

void MainWindow::onSaveThresholdConfig()
{
    if (m_component) {
        DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
        if (deviceModel) {
            try {
                QString configFileName = "threshold_config.ini";
                deviceModel->saveThresholdConfig(configFileName.toStdString());
                addLog(QString("阈值配置已保存到: %1").arg(configFileName));
            } catch (const std::exception& e) {
                addLog(QString("保存阈值配置失败: %1").arg(e.what()));
            }
        }
    }
}

// 同时修改 onLoadThresholdConfig 方法，确保加载的也是同一个文件
void MainWindow::onLoadThresholdConfig()
{
    if (m_component) {
        DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
        if (deviceModel) {
            try {
                deviceModel->loadThresholdConfig("threshold_config.ini");
                syncThresholdFromModel();
                addLog("阈值配置已从 threshold_config.ini 加载");
            } catch (const std::exception& e) {
                addLog(QString("加载阈值配置失败: %1").arg(e.what()));
            }
        }
    }
}

void MainWindow::onResetThreshold()
{
    if (m_component) {
        DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
        if (deviceModel) {
            // 重置为默认值
            double defaultValues[] = {35.0, 33.0, 43.0, 43.0};
            for (int sonarID = 0; sonarID < 4; sonarID++) {
                deviceModel->setSonarDetectionThreshold(sonarID, defaultValues[sonarID]);
            }

            syncThresholdFromModel();
            addLog("阈值配置已重置为默认值");
        }
    }
}

// 声纳范围配置槽函数实现
void MainWindow::onSonarRangeChanged(int sonarID, double value)
{
    if (sonarID >= 0 && sonarID < 4) {
        m_sonarRangeConfigs[sonarID].maxRange = static_cast<float>(value);
        updateSonarRangeDisplay();
        syncSonarRangeToChart();
        addLog(QString("设置%1最大显示距离: %2 米")
               .arg(SONAR_NAMES[sonarID])
               .arg(value, 0, 'f', 0));
    }
}

void MainWindow::onSaveRangeConfig()
{
    saveExtendedConfig();
    addLog("声纳范围配置已保存到 threshold_config.ini");
}

void MainWindow::updateThresholdDisplay()
{
    if (!m_component) return;

    DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
    if (!deviceModel) return;

    // 更新各声纳的状态标签
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        auto& thresholdWidget = m_thresholdControls[sonarID];
        double currentThreshold = deviceModel->getSonarDetectionThreshold(sonarID);

        // 调整字体大小
        thresholdWidget.statusLabel->setText(QString("当前: %1").arg(currentThreshold, 0, 'f', 2));
        thresholdWidget.statusLabel->setStyleSheet("color: blue; font-size: 12px;");
    }
}

void MainWindow::syncThresholdFromModel()
{
    if (!m_component) return;

    DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
    if (!deviceModel) return;

    // 同步各声纳阈值设置
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        auto& thresholdWidget = m_thresholdControls[sonarID];
        thresholdWidget.thresholdSpinBox->blockSignals(true);
        thresholdWidget.thresholdSpinBox->setValue(deviceModel->getSonarDetectionThreshold(sonarID));
        thresholdWidget.thresholdSpinBox->blockSignals(false);
    }

    updateThresholdDisplay();
}




void MainWindow::onResetRangeConfig()
{
    // 重置为默认值
    m_sonarRangeConfigs[0].maxRange = 30000.0f;
    m_sonarRangeConfigs[1].maxRange = 25000.0f;
    m_sonarRangeConfigs[2].maxRange = 35000.0f;
    m_sonarRangeConfigs[3].maxRange = 40000.0f;

    // 更新界面
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        auto& rangeWidget = m_rangeControls[sonarID];
        rangeWidget.rangeSpinBox->blockSignals(true);
        rangeWidget.rangeSpinBox->setValue(m_sonarRangeConfigs[sonarID].maxRange);
        rangeWidget.rangeSpinBox->blockSignals(false);
    }

    updateSonarRangeDisplay();
    syncSonarRangeToChart();
    addLog("声纳范围配置已重置为默认值");
}

void MainWindow::updateSonarRangeDisplay()
{
    for (int sonarID = 0; sonarID < 4; sonarID++) {
        auto& rangeWidget = m_rangeControls[sonarID];
        rangeWidget.statusLabel->setText(QString("当前: %1m").arg(m_sonarRangeConfigs[sonarID].maxRange, 0, 'f', 0));
    }
}

void MainWindow::syncSonarRangeToChart()
{
    if (m_seaChartWidget) {
        // 将配置的范围同步到海图显示
        QVector<SonarDetectionRange> ranges;

        for (int sonarID = 0; sonarID < 4; sonarID++) {
            const auto& config = m_sonarRangeConfigs[sonarID];

            if (sonarID == 1 && config.hasTwoSegments) {
                // 舷侧声纳有两个分段
                SonarDetectionRange range1;
                range1.sonarId = sonarID;
                range1.sonarName = config.name + "(右舷)";
                range1.startAngle = config.startAngle1;
                range1.endAngle = config.endAngle1;
                range1.maxRange = config.maxRange;
                range1.color = SONAR_COLORS[sonarID];
                range1.isVisible = true;
                ranges.append(range1);

                SonarDetectionRange range2;
                range2.sonarId = sonarID;
                range2.sonarName = config.name + "(左舷)";
                range2.startAngle = config.startAngle2;
                range2.endAngle = config.endAngle2;
                range2.maxRange = config.maxRange;
                range2.color = SONAR_COLORS[sonarID];
                range2.isVisible = true;
                ranges.append(range2);
            } else {
                // 其他声纳只有一个分段
                SonarDetectionRange range;
                range.sonarId = sonarID;
                range.sonarName = config.name;
                range.startAngle = config.startAngle1;
                range.endAngle = config.endAngle1;
                range.maxRange = config.maxRange;
                range.color = SONAR_COLORS[sonarID];
                range.isVisible = true;
                ranges.append(range);
            }
        }

        m_seaChartWidget->setSonarDetectionRanges(ranges);
        addLog("已同步声纳范围配置到海图显示");
    }
}


// 扩展配置文件操作
void MainWindow::loadExtendedConfig()
{
    try {
        std::ifstream configFile("threshold_config.ini");
        if (!configFile.is_open()) {
            addLog("配置文件不存在，使用默认声纳范围配置");
            return;
        }

        std::string line;
        std::string currentSection;

        while (std::getline(configFile, line)) {
            // 跳过空行和注释
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // 检查是否是节标题
            if (line[0] == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.length() - 2);
                continue;
            }

            // 解析键值对
            size_t equalPos = line.find('=');
            if (equalPos == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);

            // 移除注释部分
            size_t commentPos = value.find('#');
            if (commentPos != std::string::npos) {
                value = value.substr(0, commentPos);
            }

            // 移除前后空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // 处理声纳范围设置
            if (currentSection == "SonarRanges") {
                if (key.substr(0, 5) == "Sonar" && key.substr(6) == "_MaxRange") {
                    int sonarID = std::stoi(key.substr(5, 1));
                    if (sonarID >= 0 && sonarID < 4) {
                        m_sonarRangeConfigs[sonarID].maxRange = std::stof(value);
                        addLog(QString("加载声纳%1最大距离: %2米").arg(sonarID).arg(std::stof(value), 0, 'f', 0));
                    }
                }
            }
        }

        configFile.close();
        addLog("扩展配置已从文件加载: threshold_config.ini");

    } catch (const std::exception& e) {
        addLog(QString("加载扩展配置时出错: %1，使用默认值").arg(e.what()));
    }
}

void MainWindow::saveExtendedConfig()
{
    if (m_component) {
        DeviceModel* deviceModel = dynamic_cast<DeviceModel*>(m_component);
        if (deviceModel) {
            try {
                std::string filename = "threshold_config.ini";
                auto stdMap = convertToDeviceModelConfig(m_sonarRangeConfigs);
                deviceModel->saveExtendedConfig(filename, stdMap);
                addLog("声纳阈值和范围配置已保存到: threshold_config.ini");
            } catch (const std::exception& e) {
                addLog(QString("保存扩展配置失败: %1").arg(e.what()));
            }
        }
    }
}

std::map<int, DeviceModel::SonarRangeConfig> MainWindow::convertToDeviceModelConfig(const QMap<int, SonarRangeConfig>& qmap) const
{
    std::map<int, DeviceModel::SonarRangeConfig> stdMap;

    for (auto it = qmap.begin(); it != qmap.end(); ++it) {
        int sonarId = it.key();
        const SonarRangeConfig& qConfig = it.value();

        DeviceModel::SonarRangeConfig dmConfig;
        dmConfig.sonarId = qConfig.sonarId;
        dmConfig.maxRange = qConfig.maxRange;
        dmConfig.startAngle1 = qConfig.startAngle1;
        dmConfig.endAngle1 = qConfig.endAngle1;
        dmConfig.startAngle2 = qConfig.startAngle2;
        dmConfig.endAngle2 = qConfig.endAngle2;
        dmConfig.hasTwoSegments = qConfig.hasTwoSegments;

        stdMap[sonarId] = dmConfig;
    }

    return stdMap;
}
