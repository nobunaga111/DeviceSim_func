#ifndef SEACHARTWIDGET_H
#define SEACHARTWIDGET_H

#include <QWidget>
#include <QPoint>
#include <QPointF>
#include <QVector>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QTimer>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSettings>
#include <QFile>
#include <QPixmap>
#include <cmath>

// 前向声明
class MainWindow;

/**
 * @brief 海图上的平台实体
 */
struct ChartPlatform {
    int id;                     // 平台ID
    QString name;               // 平台名称
    QPointF position;           // 位置 (经度, 纬度)
    double heading;             // 航向角 (度)
    double speed;               // 速度 (节)
    QColor color;               // 显示颜色
    bool isOwnShip;             // 是否为本艇
    bool isVisible;             // 是否可见

    ChartPlatform() : id(-1), heading(0.0), speed(0.0),
                     color(Qt::blue), isOwnShip(false), isVisible(true) {}
};

/**
 * @brief 声纳探测范围显示
 */
struct SonarDetectionRange {
    int sonarId;                // 声纳ID
    QString sonarName;          // 声纳名称
    float startAngle;           // 起始角度
    float endAngle;             // 结束角度
    float maxRange;             // 最大探测距离 (米)
    QColor color;               // 显示颜色
    bool isVisible;             // 是否显示

    SonarDetectionRange() : sonarId(-1), startAngle(0.0f), endAngle(0.0f),
                           maxRange(30000.0f), color(Qt::green), isVisible(true) {}
};

/**
 * @brief 海图导调界面组件
 * 负责显示海图、平台位置、声纳探测范围等
 */
class SeaChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SeaChartWidget(QWidget *parent = nullptr);
    ~SeaChartWidget();

    /**
     * @brief 设置主窗口引用（用于回调）
     */
    void setMainWindow(MainWindow* mainWindow);

    /**
     * @brief 平台到海图
     * @param platform 平台信息
     */
    void addPlatform(const ChartPlatform& platform);

    /**
     * @brief 更新平台位置
     * @param platformId 平台ID
     * @param position 新位置
     * @param heading 新航向
     * @param speed 新速度
     */
    void updatePlatformPosition(int platformId, const QPointF& position, double heading = -1, double speed = -1);

    /**
     * @brief 移除平台
     * @param platformId 平台ID
     */
    void removePlatform(int platformId);

    /**
     * @brief 清除所有平台
     */
    void clearAllPlatforms();

    /**
     * @brief 设置本艇位置
     * @param position 位置 (经度, 纬度)
     * @param heading 航向角
     */
    void setOwnShipPosition(const QPointF& position, double heading = 0.0);

    /**
     * @brief 获取本艇位置
     */
    QPointF getOwnShipPosition() const;

    /**
     * @brief 安全获取本艇速度
     */
    double getOwnShipSpeed() const;

    /**
     * @brief 检查本艇是否有效
     */
    bool isOwnShipValid() const;


    /**
     * @brief 设置声纳探测范围显示
     * @param ranges 声纳探测范围列表
     */
    void setSonarDetectionRanges(const QVector<SonarDetectionRange>& ranges);

    /**
     * @brief 更新声纳探测范围可见性
     * @param sonarId 声纳ID
     * @param visible 是否可见
     */
    void setSonarRangeVisible(int sonarId, bool visible);

    /**
     * @brief 验证并修复声纳范围状态
     */
    void validateSonarRanges();

    /**
     * @brief 获取所有目标平台位置
     * @return 目标平台列表 (不包括本艇)
     */
    QVector<ChartPlatform> getTargetPlatforms() const;

    /**
     * @brief 设置海图显示范围
     * @param center 中心点 (经度, 纬度)
     * @param scale 缩放比例 (米/像素)
     */
    void setChartView(const QPointF& center, double scale);

    /**
     * @brief 获取当前海图中心点
     */
    QPointF getChartCenter() const { return m_chartCenter; }

    /**
     * @brief 获取当前缩放比例
     */
    double getChartScale() const { return m_chartScale; }

    /**
     * @brief 获取控制面板组件
     */
    QWidget* getControlPanel() const;

    /**
     * @brief 设置海图可见性
     */
    void setSeaChartVisible(bool visible);

    /**
     * @brief 获取海图可见性
     */
    bool isSeaChartVisible() const;

    /**
     * @brief 重置视图到默认状态
     */
    void resetView();

    /**
     * @brief 放大视图
     */
    void zoomIn();

    /**
     * @brief 缩小视图
     */
    void zoomOut();

    /**
     * @brief 向上平移
     */
    void panUp(double distance = 1000.0);

    /**
     * @brief 向下平移
     */
    void panDown(double distance = 1000.0);

    /**
     * @brief 向左平移
     */
    void panLeft(double distance = 1000.0);

    /**
     * @brief 向右平移
     */
    void panRight(double distance = 1000.0);

    /**
     * @brief 调整视图以显示所有目标
     */
    void fitToTargets();

    /**
     * @brief 获取平台信息
     */
    QString getPlatformInfo(int platformId) const;

    /**
     * @brief 获取可见平台ID列表
     */
    QVector<int> getVisiblePlatformIds() const;

    /**
     * @brief 设置平台可见性
     */
    void setPlatformVisible(int platformId, bool visible);

    /**
     * @brief 设置所有平台可见性
     */
    void setAllPlatformsVisible(bool visible);

    /**
     * @brief 导出海图为图片
     */
    void exportChart(const QString& fileName);

    /**
     * @brief 保存配置到文件
     */
    void saveConfiguration(const QString& fileName);

    /**
     * @brief 从文件加载配置
     */
    void loadConfiguration(const QString& fileName);

    /**
     * @brief 获取海图统计信息
     */
    QString getChartStatistics() const;

    /**
     * @brief 获取本艇的完整信息
     */
    ChartPlatform getOwnShipInfo() const;

    /**
     * @brief 获取本艇当前航向
     */
    double getOwnShipHeading() const;

    /**
     * @brief 坐标转换：地理坐标 -> 屏幕坐标
     */
    QPoint geoToScreen(const QPointF& geoPos) const;


signals:
    /**
     * @brief 平台位置改变信号
     * @param platformId 平台ID
     * @param newPosition 新位置
     */
    void platformPositionChanged(int platformId, const QPointF& newPosition);

    /**
     * @brief 目标平台数据更新信号（用于生成传播声数据）
     * @param targetPlatforms 目标平台列表
     */
    void targetPlatformsUpdated(const QVector<ChartPlatform>& targetPlatforms);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onAddTargetShip();
    void onRemoveTargetShip();
    void onEditPlatform();
    void onCenterOnOwnShip();
    void onAutoGenerateTargets();
    void updatePlatformMovement();  // 更新平台移动

private:
    /**
     * @brief 初始化本艇
     */
    void initializeOwnShip();

    /**
     * @brief 初始化声纳探测范围
     */
    void initializeSonarRanges();

    /**
     * @brief 创建右键菜单
     */
    void createContextMenu();

    /**
     * @brief 创建控制面板
     */
    void createControlPanel();

    /**
     * @brief 更新控制面板数值显示
     */
    void updateControlPanelValues();


    /**
     * @brief 坐标转换：屏幕坐标 -> 地理坐标
     */
    QPointF screenToGeo(const QPoint& screenPos) const;

    /**
     * @brief 绘制网格
     */
    void drawGrid(QPainter& painter);

    /**
     * @brief 绘制坐标轴
     */
    void drawCoordinateAxis(QPainter& painter);

    /**
     * @brief 绘制平台
     */
    void drawPlatforms(QPainter& painter);

    /**
     * @brief 绘制单个平台
     */
    void drawPlatform(QPainter& painter, const ChartPlatform& platform);

    /**
     * @brief 绘制声纳探测范围
     */
    void drawSonarRanges(QPainter& painter);

    /**
     * @brief 绘制单个声纳探测范围
     */
    void drawSonarRange(QPainter& painter, const SonarDetectionRange& range, const QPointF& centerPos);

    /**
     * @brief 查找指定位置的平台
     * @param pos 屏幕坐标
     * @return 平台ID，-1表示未找到
     */
    int findPlatformAt(const QPoint& pos) const;

    /**
     * @brief 计算两点间距离 (米)
     */
    double calculateDistance(const QPointF& pos1, const QPointF& pos2) const;

    /**
     * @brief 计算两点间方位角 (度)
     */
    double calculateBearing(const QPointF& from, const QPointF& to) const;

    /**
     * @brief 生成传播声数据并发送
     */
    void generateAndSendPropagatedSoundData();

    /**
     * @brief 验证位置是否有效（非零且在合理范围内）
     */
    bool isValidPosition(const QPointF& pos) const;

    /**
     * @brief 强制验证并修复本艇位置
     */
    void validateAndFixOwnShipPosition();
    void findOwnShip();
    /**
     * @brief 移除目标（确保不影响本艇）
     */
    void removeTarget();

    /**
     * @brief 安全获取本艇位置（带验证）
     */
    QPointF getOwnShipPositionSafe() const;

    /**
     * @brief 安全获取本艇朝向（带验证）
     */
    double getOwnShipHeadingSafe() const;


private:
    MainWindow* m_mainWindow;                       // 主窗口引用

    // 海图显示参数
    QPointF m_chartCenter;                          // 海图中心点 (经度, 纬度)
    double m_chartScale;                            // 缩放比例 (米/像素)
    double m_minScale;                              // 最小缩放比例
    double m_maxScale;                              // 最大缩放比例

    // 平台数据
    QVector<ChartPlatform> m_platforms;             // 所有平台列表
    int m_nextPlatformId;                           // 下一个平台ID
    ChartPlatform* m_ownShip;                       // 本艇指针

    // 声纳探测范围
    QVector<SonarDetectionRange> m_sonarRanges;     // 声纳探测范围列表

    // 鼠标操作状态
    bool m_isDragging;                              // 是否正在拖拽
    bool m_isDraggingPlatform;                      // 是否正在拖拽平台
    QPoint m_lastMousePos;                          // 上次鼠标位置
    int m_draggedPlatformId;                        // 被拖拽的平台ID

    // 右键菜单
    QMenu* m_contextMenu;                           // 右键菜单
    QAction* m_addTargetAction;                     // 添加目标动作
    QAction* m_removeTargetAction;                  // 移除目标动作
    QAction* m_editPlatformAction;                  // 编辑平台动作
    QAction* m_centerOnOwnShipAction;               // 居中本艇动作
    QAction* m_autoGenerateAction;                  // 自动生成目标动作

    // 控制面板
    QWidget* m_controlPanel;                        // 控制面板
    QGroupBox* m_chartControlGroup;                 // 海图控制组
    QDoubleSpinBox* m_scaleSpinBox;                 // 缩放控制
    QLineEdit* m_centerLonEdit;                     // 中心经度
    QLineEdit* m_centerLatEdit;                     // 中心纬度
    QPushButton* m_centerOwnShipBtn;                // 居中本艇按钮
    QPushButton* m_autoGenTargetsBtn;               // 自动生成目标按钮
    QPushButton* m_clearTargetsBtn;                 // 清除目标按钮

    QGroupBox* m_sonarControlGroup;                 // 声纳范围控制组
    QVector<QCheckBox*> m_sonarRangeCheckBoxes;     // 声纳范围显示复选框

    // 数据生成定时器
    QTimer* m_dataGenerationTimer;                  // 数据生成定时器 (5秒间隔)

    QTimer* m_platformMoveTimer;                    // 平台移动定时器 (1秒间隔)

    // 绘制参数
    static const int PLATFORM_SIZE = 12;            // 平台绘制大小
    static const int GRID_SIZE = 50;                // 网格大小 (像素)
    static const double DEFAULT_SCALE;              // 默认缩放比例
    static const double DEFAULT_LAT;                // 默认纬度
    static const double DEFAULT_LON;                // 默认经度
};

#endif // SEACHARTWIDGET_H
