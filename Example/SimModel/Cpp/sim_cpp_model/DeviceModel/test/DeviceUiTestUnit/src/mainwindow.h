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
#include <QScrollBar>
#include <QDateTime>
#include <QApplication>
#include <QScreen>
#include <QMap>
#include <QSplitter>
#include <QProgressBar>
#include <QLCDNumber>
#include <QCheckBox>
#include <QFrame>
#include <QDoubleSpinBox>

#include "CreateDeviceModel.h"
#include "CSimComponentBase.h"
#include "DeviceModelAgent.h"
#include "DeviceTestInOut.h"
#include "devicemodel.h"
#include "seachartwidget.h"
#include "SonarConfig.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /**
     * @brief 生成并发送传播后连续声数据
     * @param targetPlatforms 目标平台列表
     */
    void generateAndSendPropagatedSoundData(const QVector<ChartPlatform>& targetPlatforms);

    /**
     * @brief 生成并发送平台自噪声数据
     */
    void generateAndSendPlatformSelfSoundData();

    /**
     * @brief 生成并发送环境噪声数据
     */
    void generateAndSendEnvironmentNoiseData();

    void addLog(const QString& message);



private slots:
    //========== 声纳控制功能 ==========//
    void onSonarSwitchToggled(int sonarID, bool enabled);

    //========== 海图交互功能 ==========//
    void onTargetPlatformsUpdated(const QVector<ChartPlatform>& targetPlatforms);
    void onPlatformPositionChanged(int platformId, const QPointF& newPosition);

    //========== 日志功能 ==========//
    void onClearLogClicked();
    void onToggleFileLogClicked();

    //========== 定时更新功能 ==========//
    void onUpdateSonarStatus();
    void onDataGenerationTimer();

    //========== 阈值设置功能 ==========//
    void onSonarThresholdChanged(int sonarID, double value);
    void onSaveThresholdConfig();
    void onLoadThresholdConfig();
    void onResetThreshold();

    //========== 声纳范围配置功能 ==========//
    void onSonarRangeChanged(int sonarID, double value);
    void onSaveRangeConfig();
    void onResetRangeConfig();

private:
    //========== UI初始化函数 ==========//
    void initializeUI();
    void createMainLayout();
    void createSeaChartPanel();
    void createControlPanel();
    void createSonarStatusPanel();
    void createLogPanel();
    void initializeTimers();

    //========== 辅助函数 ==========//
    void updateSonarStatusDisplay();
    void sendSonarControlOrder(int sonarID, bool enabled);

    //========== 数据生成函数 ==========//
    CMsg_PropagatedContinuousSoundListStruct createPropagatedSoundData(const QVector<ChartPlatform>& targetPlatforms);
    CData_PlatformSelfSound* createPlatformSelfSoundData();
    CMsg_EnvironmentNoiseToSonarStruct createEnvironmentNoiseData();

    //========== 声纳方程结果显示 ==========//
    void updateSonarEquationResults();
    QString formatSonarResults(const std::map<int, std::vector<DeviceModel::TargetEquationResult>>& allResults);

    /**
      * @brief 生成并发送平台机动数据（包括朝向信息）
      */
    void generateAndSendPlatformMotionData();

    //========== 阈值设置辅助函数 ==========//
    void createThresholdConfigPanel();
    void updateThresholdDisplay();
    void syncThresholdFromModel();

    //========== 声纳范围配置辅助函数 ==========//
    void createSonarRangeConfigPanel();
    void updateSonarRangeDisplay();
    void syncSonarRangeToChart();

    //========== 配置文件操作 ==========//
    void loadExtendedConfig();
    void saveExtendedConfig();

private:
    //========== 主要布局组件 ==========//
    QWidget* m_centralWidget;                       // 中央部件
    QSplitter* m_mainSplitter;                      // 主分割器 (水平)
    QSplitter* m_rightSplitter;                     // 右侧分割器 (垂直)

    //========== 海图相关组件 ==========//
    SeaChartWidget* m_seaChartWidget;               // 海图组件
    QWidget* m_seaChartPanel;                       // 海图面板容器

    //========== 控制面板组件 ==========//
    QWidget* m_controlPanel;                        // 右侧控制面板

    //========== 声纳状态面板 ==========//
    QGroupBox* m_sonarStatusGroup;                  // 声纳状态组

    struct SonarControlWidget {
        QLabel* nameLabel;                          // 声纳名称标签
        QCheckBox* enableCheckBox;                  // 启用复选框
        QLabel* statusLabel;                        // 状态标签
        QLCDNumber* targetCountDisplay;             // 目标数量显示
        QProgressBar* signalStrengthBar;            // 信号强度条
        QLabel* equationResultLabel;                // 方程结果标签
    };

    QMap<int, SonarControlWidget> m_sonarControls; // 声纳控制组件映射表

    //========== 系统状态面板 ==========//
    QGroupBox* m_systemStatusGroup;                 // 系统状态组
    QLabel* m_platformPositionLabel;                // 平台位置标签
    QLabel* m_targetCountLabel;                     // 目标数量标签
    QLabel* m_dataStatusLabel;                      // 数据状态标签
    QLabel* m_simulationTimeLabel;                  // 仿真时间标签

    //========== 声纳方程结果面板 ==========//
    QGroupBox* m_equationResultsGroup;              // 声纳方程结果组
    QTextEdit* m_equationResultsDisplay;            // 结果显示区域
    QPushButton* m_refreshResultsButton;            // 刷新结果按钮
    QPushButton* m_exportResultsButton;             // 导出结果按钮

    //========== 日志面板 ==========//
    QTextEdit* m_logTextEdit;                       // 日志文本编辑框
    QPushButton* m_clearLogButton;                  // 清除日志按钮
    QPushButton* m_toggleFileLogButton;             // 切换文件日志按钮

    //========== 模型组件 ==========//
    CSimComponentBase* m_component;                 // 声纳组件
    DeviceModelAgent* m_agent;                      // 代理

    //========== 定时器 ==========//
    QTimer* m_statusUpdateTimer;                    // 状态更新定时器
    QTimer* m_dataGenerationTimer;                  // 数据生成定时器

    //========== 状态信息 ==========//
    QMap<int, bool> m_sonarEnabledStates;           // 声纳启用状态
    QVector<ChartPlatform> m_currentTargets;        // 当前目标列表
    bool m_platformSelfSoundSent;                   // 平台自噪声是否已发送
    bool m_environmentNoiseSent;                    // 环境噪声是否已发送

    //========== 声纳配置信息 ==========//
    static const QStringList SONAR_NAMES;          // 声纳名称列表
    static const QList<QColor> SONAR_COLORS;       // 声纳颜色列表

    //========== 阈值设置面板 ==========//
    QGroupBox* m_thresholdConfigGroup;              // 阈值配置组

    struct SonarThresholdWidget {
        QLabel* nameLabel;                          // 声纳名称标签
        QDoubleSpinBox* thresholdSpinBox;          // 阈值输入框
        QLabel* statusLabel;                        // 状态标签（显示当前使用的阈值）
    };

    QMap<int, SonarThresholdWidget> m_thresholdControls; // 阈值控制组件映射表

    QPushButton* m_saveThresholdConfigButton;       // 保存配置按钮
    QPushButton* m_loadThresholdConfigButton;       // 加载配置按钮
    QPushButton* m_resetThresholdButton;            // 重置阈值按钮

    //========== 声纳范围配置面板 ==========//
    QGroupBox* m_sonarRangeConfigGroup;             // 声纳范围配置组

    struct SonarRangeWidget {
        QLabel* nameLabel;                          // 声纳名称标签
        QDoubleSpinBox* rangeSpinBox;              // 最大距离输入框
        QLabel* statusLabel;                        // 状态标签
    };

    QMap<int, SonarRangeWidget> m_rangeControls;    // 范围控制组件映射表
    QPushButton* m_saveRangeConfigButton;           // 保存范围配置按钮
    QPushButton* m_resetRangeConfigButton;          // 重置范围配置按钮

    //========== 声纳范围配置数据 ==========//
    QMap<int, SonarRangeConfig> m_sonarRangeConfigs; // 声纳范围配置

    bool m_fileLogEnabled = true;                   // 文件日志是否启用
    QVBoxLayout* m_controlPanelLayout;              // 控制面板布局


    std::map<int, DeviceModel::SonarRangeConfig> convertToDeviceModelConfig(const QMap<int, SonarRangeConfig>& qmap) const;

};

#endif // MAINWINDOW_H
