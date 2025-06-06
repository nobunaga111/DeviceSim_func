#include "SeaChartWidget.h"
#include "mainwindow.h"
#include <QPainter>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QApplication>
#include <QTimer>
#include <QGridLayout>
#include <QtMath>
#include <QDebug>

// 静态常量定义
const double SeaChartWidget::DEFAULT_SCALE = 100.0;    // 100米/像素
const double SeaChartWidget::DEFAULT_LAT = 56.654569;  // 默认纬度
const double SeaChartWidget::DEFAULT_LON = 126.564567; // 默认经度

SeaChartWidget::SeaChartWidget(QWidget *parent)
    : QWidget(parent)
    , m_mainWindow(nullptr)
    , m_chartCenter(DEFAULT_LON, DEFAULT_LAT)
    , m_chartScale(DEFAULT_SCALE)
    , m_minScale(10.0)      // 最小10米/像素
    , m_maxScale(10000.0)   // 最大10公里/像素
    , m_nextPlatformId(1000)
    , m_ownShip(nullptr)
    , m_isDragging(false)
    , m_isDraggingPlatform(false)
    , m_draggedPlatformId(-1)
    , m_dataGenerationTimer(nullptr)
    , m_platformMoveTimer(nullptr)
{
    setMinimumSize(600, 400);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);

    // 初始化本艇
    initializeOwnShip();

    // 初始化声纳探测范围
    initializeSonarRanges();

    // 创建右键菜单
    createContextMenu();

    // 创建控制面板
    createControlPanel();

    // 初始化数据生成定时器
    m_dataGenerationTimer = new QTimer(this);
    m_dataGenerationTimer->setInterval(5000); // 5秒间隔
    connect(m_dataGenerationTimer, &QTimer::timeout,
            this, &SeaChartWidget::generateAndSendPropagatedSoundData);
    m_dataGenerationTimer->start();

    qDebug() << "SeaChartWidget initialized with center:" << m_chartCenter << "scale:" << m_chartScale;





    // 在构造函数中添加移动定时器
    m_platformMoveTimer = new QTimer(this);
    connect(m_platformMoveTimer, &QTimer::timeout, this, &SeaChartWidget::updatePlatformMovement);
    m_platformMoveTimer->start(1000); // 每秒更新一次位置
}

SeaChartWidget::~SeaChartWidget()
{
    if (m_dataGenerationTimer) {
        m_dataGenerationTimer->stop();
    }
}

void SeaChartWidget::initializeOwnShip()
{
    ChartPlatform ownShip;
    ownShip.id = 0;
    ownShip.name = "本艇";  // 使用简单的中文字符串
    ownShip.position = m_chartCenter;
    ownShip.heading = 45.0;  // 确保初始航向为45度
    ownShip.speed = 10.0;
    ownShip.color = Qt::red;
    ownShip.isOwnShip = true;
    ownShip.isVisible = true;

    // 先清空现有的本艇（如果有）
    m_platforms.erase(
        std::remove_if(m_platforms.begin(), m_platforms.end(),
            [](const ChartPlatform& platform) {
                return platform.isOwnShip;
            }),
        m_platforms.end()
    );

    m_platforms.append(ownShip);

    // 重新设置本艇指针
    for (auto& platform : m_platforms) {
        if (platform.isOwnShip) {
            m_ownShip = &platform;
            qDebug() << "Own ship initialized with heading:" << platform.heading
                     << "at position:" << platform.position;
            return;
        }
    }

    qDebug() << "Error: Failed to initialize own ship";
}

void SeaChartWidget::initializeSonarRanges()
{
    m_sonarRanges.clear(); // 先清空，避免重复添加

    struct SonarDefinition {
        int id;
        QString name;
        float startAngle;
        float endAngle;
        float maxRange;
        QColor color;
        bool visible;
    };

    QVector<SonarDefinition> sonarDefinitions = {
        // 艏端声纳
        {0, "艏端声纳", -45.0f, 45.0f, 30000.0f, QColor(0, 255, 0, 100), true},

        // 舷侧声纳 - 右舷部分
        {1, "舷侧声纳(右舷)", 45.0f, 135.0f, 25000.0f, QColor(0, 0, 255, 80), true},

        // 舷侧声纳 - 左舷部分
        {1, "舷侧声纳(左舷)", -135.0f, -45.0f, 25000.0f, QColor(0, 0, 255, 80), true},

        // 粗拖声纳
        {2, "粗拖声纳", 135.0f, 225.0f, 35000.0f, QColor(255, 255, 0, 100), true},

        // 细拖声纳
        {3, "细拖声纳", 120.0f, 240.0f, 40000.0f, QColor(255, 0, 255, 100), true}
    };

    for (const auto& sonarDef : sonarDefinitions) {
        SonarDetectionRange range;
        range.sonarId = sonarDef.id;
        range.sonarName = sonarDef.name;
        range.startAngle = sonarDef.startAngle;
        range.endAngle = sonarDef.endAngle;
        range.maxRange = sonarDef.maxRange;
        range.color = sonarDef.color;
        range.isVisible = sonarDef.visible;

        m_sonarRanges.append(range);
    }

    qDebug() << "Initialized" << m_sonarRanges.size() << "sonar ranges including bilateral side sonar";
}
void SeaChartWidget::createContextMenu()
{
    m_contextMenu = new QMenu(this);

    m_addTargetAction = m_contextMenu->addAction("添加目标舰船");
    connect(m_addTargetAction, &QAction::triggered, this, &SeaChartWidget::onAddTargetShip);

    m_removeTargetAction = m_contextMenu->addAction("移除目标");
    connect(m_removeTargetAction, &QAction::triggered, this, &SeaChartWidget::onRemoveTargetShip);

    m_editPlatformAction = m_contextMenu->addAction("编辑平台");
    connect(m_editPlatformAction, &QAction::triggered, this, &SeaChartWidget::onEditPlatform);

    m_contextMenu->addSeparator();

    m_centerOnOwnShipAction = m_contextMenu->addAction("居中本艇");
    connect(m_centerOnOwnShipAction, &QAction::triggered, this, &SeaChartWidget::onCenterOnOwnShip);

    m_autoGenerateAction = m_contextMenu->addAction("自动生成目标");
    connect(m_autoGenerateAction, &QAction::triggered, this, &SeaChartWidget::onAutoGenerateTargets);
}

void SeaChartWidget::createControlPanel()
{
    // 创建控制面板（这个面板会被添加到主窗口的布局中）
    m_controlPanel = new QWidget();
    m_controlPanel->setFixedWidth(300);
    m_controlPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    QVBoxLayout* panelLayout = new QVBoxLayout(m_controlPanel);

    // 海图控制组
    m_chartControlGroup = new QGroupBox("海图控制");
    QGridLayout* chartLayout = new QGridLayout(m_chartControlGroup);

    // 缩放控制
    chartLayout->addWidget(new QLabel("缩放(米/像素):"), 0, 0);
    m_scaleSpinBox = new QDoubleSpinBox();
    m_scaleSpinBox->setRange(m_minScale, m_maxScale);
    m_scaleSpinBox->setValue(m_chartScale);
    m_scaleSpinBox->setDecimals(1);
    connect(m_scaleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double value) {
                m_chartScale = value;
                update();
            });
    chartLayout->addWidget(m_scaleSpinBox, 0, 1);

    // 中心位置控制
    chartLayout->addWidget(new QLabel("中心经度:"), 1, 0);
    m_centerLonEdit = new QLineEdit(QString::number(m_chartCenter.x(), 'f', 6));
    connect(m_centerLonEdit, &QLineEdit::editingFinished, [this]() {
        bool ok;
        double lon = m_centerLonEdit->text().toDouble(&ok);
        if (ok && lon >= -180.0 && lon <= 180.0) {
            m_chartCenter.setX(lon);
            update();
        }
    });
    chartLayout->addWidget(m_centerLonEdit, 1, 1);

    chartLayout->addWidget(new QLabel("中心纬度:"), 2, 0);
    m_centerLatEdit = new QLineEdit(QString::number(m_chartCenter.y(), 'f', 6));
    connect(m_centerLatEdit, &QLineEdit::editingFinished, [this]() {
        bool ok;
        double lat = m_centerLatEdit->text().toDouble(&ok);
        if (ok && lat >= -90.0 && lat <= 90.0) {
            m_chartCenter.setY(lat);
            update();
        }
    });
    chartLayout->addWidget(m_centerLatEdit, 2, 1);

    // 控制按钮
    m_centerOwnShipBtn = new QPushButton("居中本艇");
    connect(m_centerOwnShipBtn, &QPushButton::clicked, this, &SeaChartWidget::onCenterOnOwnShip);
    chartLayout->addWidget(m_centerOwnShipBtn, 3, 0, 1, 2);

    m_autoGenTargetsBtn = new QPushButton("自动生成目标");
    connect(m_autoGenTargetsBtn, &QPushButton::clicked, this, &SeaChartWidget::onAutoGenerateTargets);
    chartLayout->addWidget(m_autoGenTargetsBtn, 4, 0, 1, 2);

    m_clearTargetsBtn = new QPushButton("清除所有目标");
    connect(m_clearTargetsBtn, &QPushButton::clicked, [this]() {
        // 只清除目标，保留本艇
        m_platforms.erase(
            std::remove_if(m_platforms.begin(), m_platforms.end(),
                [](const ChartPlatform& platform) {
                    return !platform.isOwnShip;
                }),
            m_platforms.end()
        );
        emit targetPlatformsUpdated(getTargetPlatforms());
        update();
    });
    chartLayout->addWidget(m_clearTargetsBtn, 5, 0, 1, 2);

    panelLayout->addWidget(m_chartControlGroup);

    // 声纳范围控制组
    m_sonarControlGroup = new QGroupBox("声纳探测范围显示");
    QVBoxLayout* sonarLayout = new QVBoxLayout(m_sonarControlGroup);

    QStringList sonarNames = {"艏端声纳", "舷侧声纳", "粗拖声纳", "细拖声纳"};
    QVector<int> sonarIds = {0, 1, 2, 3};  // 移除ID=11的声纳

    for (int i = 0; i < sonarNames.size(); i++) {
        QCheckBox* checkBox = new QCheckBox(sonarNames[i]);
        checkBox->setChecked(true);
        int sonarId = sonarIds[i];
        connect(checkBox, &QCheckBox::toggled, [this, sonarId](bool checked) {
            setSonarRangeVisible(sonarId, checked);
        });
        m_sonarRangeCheckBoxes.append(checkBox);
        sonarLayout->addWidget(checkBox);
    }
    panelLayout->addWidget(m_sonarControlGroup);

    // 弹性空间
    panelLayout->addStretch(1);
}

void SeaChartWidget::setMainWindow(MainWindow* mainWindow)
{
    m_mainWindow = mainWindow;
}

void SeaChartWidget::addPlatform(const ChartPlatform& platform)
{
    m_platforms.append(platform);

    // 如果是本艇，更新本艇指针
    if (platform.isOwnShip) {
        m_ownShip = &m_platforms.last();
    }

    if (!platform.isOwnShip) {
        emit targetPlatformsUpdated(getTargetPlatforms());
    }

    update();
}

void SeaChartWidget::updatePlatformPosition(int platformId, const QPointF& position, double heading, double speed)
{
    for (auto& platform : m_platforms) {
        if (platform.id == platformId) {
            platform.position = position;
            if (heading >= 0) {
                platform.heading = heading;
            }
            if (speed >= 0) {
                platform.speed = speed;
            }

            // 如果是本艇，更新海图中心
            if (platform.isOwnShip) {
                // 可选择是否自动跟随本艇
                // m_chartCenter = position;
                // updateControlPanelValues();
            }

            update();
            emit platformPositionChanged(platformId, position);

            // 如果是目标平台，发送更新信号
            if (!platform.isOwnShip) {
                emit targetPlatformsUpdated(getTargetPlatforms());
            }

            break;
        }
    }
}

void SeaChartWidget::removePlatform(int platformId)
{
    for (int i = 0; i < m_platforms.size(); i++) {
        if (m_platforms[i].id == platformId && !m_platforms[i].isOwnShip) {
            m_platforms.removeAt(i);
            emit targetPlatformsUpdated(getTargetPlatforms());
            update();
            qDebug() << "Removed platform with ID:" << platformId;
            break;
        }
    }
}

void SeaChartWidget::clearAllPlatforms()
{
    // 清除逻辑，确保本艇不被删除
    ChartPlatform ownShipBackup;
    bool hasOwnShip = false;

    // 先备份本艇信息
    for (const auto& platform : m_platforms) {
        if (platform.isOwnShip) {
            ownShipBackup = platform;
            hasOwnShip = true;
            break;
        }
    }

    // 只移除非本艇平台
    m_platforms.erase(
        std::remove_if(m_platforms.begin(), m_platforms.end(),
            [](const ChartPlatform& platform) {
                return !platform.isOwnShip;
            }),
        m_platforms.end()
    );

    // 重新设置本艇指针
    for (auto& platform : m_platforms) {
        if (platform.isOwnShip) {
            m_ownShip = &platform;
            break;
        }
    }

    emit targetPlatformsUpdated(getTargetPlatforms());
    update();
}

void SeaChartWidget::setOwnShipPosition(const QPointF& position, double heading)
{
    if (m_ownShip) {
        m_ownShip->position = position;
        // 只在明确提供航向时才更新，-1表示保持原有航向
        if (heading >= 0) {
            m_ownShip->heading = heading;
        }
        update();
        qDebug() << "Updated own ship position to:" << position
                 << "heading:" << (heading >= 0 ? QString::number(heading) : "preserved");
    }
}

QPointF SeaChartWidget::getOwnShipPosition() const
{
    // 同样的安全检查
    if (!m_ownShip) {
        qDebug() << "Warning: m_ownShip is null, returning default position";
        return QPointF(DEFAULT_LON, DEFAULT_LAT);
    }

    // 验证指针有效性
    bool isValidPointer = false;
    for (const auto& platform : m_platforms) {
        if (&platform == m_ownShip && platform.isOwnShip) {
            isValidPointer = true;
            break;
        }
    }

    if (!isValidPointer) {
        qDebug() << "Warning: m_ownShip pointer is invalid, searching for own ship";
        for (const auto& platform : m_platforms) {
            if (platform.isOwnShip) {
                return platform.position;
            }
        }
        qDebug() << "Error: No own ship found in platforms list";
        return QPointF(DEFAULT_LON, DEFAULT_LAT);
    }

    return m_ownShip->position;
}

double SeaChartWidget::getOwnShipSpeed() const
{
    if (!m_ownShip) {
        return 10.0; // 默认速度
    }

    // 验证指针有效性
    for (const auto& platform : m_platforms) {
        if (&platform == m_ownShip && platform.isOwnShip) {
            return platform.speed;
        }
    }

    // 指针无效，重新查找
    for (const auto& platform : m_platforms) {
        if (platform.isOwnShip) {
            return platform.speed;
        }
    }

    return 10.0; // 默认速度
}

bool SeaChartWidget::isOwnShipValid() const
{
    if (!m_ownShip) {
        return false;
    }

    // 验证指针是否仍然有效
    for (const auto& platform : m_platforms) {
        if (&platform == m_ownShip && platform.isOwnShip) {
            return true;
        }
    }

    return false;
}

void SeaChartWidget::setSonarDetectionRanges(const QVector<SonarDetectionRange>& ranges)
{
    m_sonarRanges = ranges;
    update();
}

void SeaChartWidget::setSonarRangeVisible(int sonarId, bool visible)
{
    bool found = false;
    for (auto& range : m_sonarRanges) {
        if (range.sonarId == sonarId) {
            range.isVisible = visible;
            found = true;
            break;
        }
    }

    if (found) {
        qDebug() << "Sonar range" << sonarId << "visibility set to:" << visible;
        update();
    } else {
        qDebug() << "Warning: Sonar range" << sonarId << "not found";
    }
}

// *** 检查并修复声纳范围状态 ***
void SeaChartWidget::validateSonarRanges()
{
    bool needUpdate = false;

    // 检查是否包含所有必要的声纳ID (0, 1, 2, 3)
    QSet<int> requiredSonarIds = {0, 1, 2, 3};
    QSet<int> existingSonarIds;

    for (const auto& range : m_sonarRanges) {
        existingSonarIds.insert(range.sonarId);
    }

    // 检查是否缺少必要的声纳ID
    QSet<int> missingSonarIds = requiredSonarIds - existingSonarIds;

    if (!missingSonarIds.isEmpty()) {
        qDebug() << "Warning: Missing sonar IDs:" << missingSonarIds << ", reinitializing";
        initializeSonarRanges();
        needUpdate = true;
    }
    // 如果数量合理（4-6个范围都是可接受的）
    else if (m_sonarRanges.size() < 4 || m_sonarRanges.size() > 6) {
        qDebug() << "Warning: Unusual sonar ranges count:" << m_sonarRanges.size() << ", reinitializing";
        initializeSonarRanges();
        needUpdate = true;
    }

    // 检查每个声纳范围的有效性
    for (auto& range : m_sonarRanges) {
        if (range.color.alpha() == 0) {
            qDebug() << "Warning: Sonar range" << range.sonarId << "has invalid color, fixing";
            // 重新设置颜色
            switch (range.sonarId) {
                case 0: range.color = QColor(0, 255, 0, 100); break;
                case 1: range.color = QColor(0, 0, 255, 80); break;
                case 2: range.color = QColor(255, 255, 0, 100); break;
                case 3: range.color = QColor(255, 0, 255, 100); break;
                default: range.color = QColor(128, 128, 128, 100); break;
            }
            needUpdate = true;
        }
    }

    if (needUpdate) {
        update();
    }
}

QVector<ChartPlatform> SeaChartWidget::getTargetPlatforms() const
{
    QVector<ChartPlatform> targets;
    for (const auto& platform : m_platforms) {
        if (!platform.isOwnShip && platform.isVisible) {
            targets.append(platform);
        }
    }
    return targets;
}

void SeaChartWidget::setChartView(const QPointF& center, double scale)
{
    m_chartCenter = center;
    m_chartScale = qBound(m_minScale, scale, m_maxScale);
    updateControlPanelValues();
    update();
    // 注意：这里不要修改本艇的任何属性
}

// SeaChartWidget.cpp - 绘制和交互逻辑

void SeaChartWidget::updateControlPanelValues()
{
    if (m_scaleSpinBox) {
        m_scaleSpinBox->blockSignals(true);
        m_scaleSpinBox->setValue(m_chartScale);
        m_scaleSpinBox->blockSignals(false);
    }

    if (m_centerLonEdit) {
        m_centerLonEdit->blockSignals(true);
        m_centerLonEdit->setText(QString::number(m_chartCenter.x(), 'f', 6));
        m_centerLonEdit->blockSignals(false);
    }

    if (m_centerLatEdit) {
        m_centerLatEdit->blockSignals(true);
        m_centerLatEdit->setText(QString::number(m_chartCenter.y(), 'f', 6));
        m_centerLatEdit->blockSignals(false);
    }
}

// *** 绘制相关方法 ***
void SeaChartWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    // *** 绘制前验证本艇位置 ***
    validateAndFixOwnShipPosition();

    // *** 每次绘制前验证声纳范围状态 ***
    validateSonarRanges();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景
    painter.fillRect(rect(), QColor(20, 50, 80));

    // 确保每次绘制都重置painter状态
    painter.save();
    drawGrid(painter);
    drawCoordinateAxis(painter);
    painter.restore();

    painter.save();
    drawSonarRanges(painter);  // 现在有了有效的本艇位置
    painter.restore();

    painter.save();
    drawPlatforms(painter);
    painter.restore();
}

void SeaChartWidget::drawGrid(QPainter& painter)
{
    painter.setPen(QPen(QColor(100, 150, 200, 100), 1)); // 半透明网格线

    QSize size = this->size();

    // 计算网格间距（基于缩放级别）
    double gridSpacingMeters = 1000.0; // 1公里网格
    if (m_chartScale < 50) {
        gridSpacingMeters = 500.0;      // 500米
    } else if (m_chartScale > 500) {
        gridSpacingMeters = 5000.0;     // 5公里
    }

    int gridSpacingPixels = static_cast<int>(gridSpacingMeters / m_chartScale);

    if (gridSpacingPixels > 20) { // 只有当网格不太密集时才绘制
        // 垂直线
        int startX = (size.width() / 2) % gridSpacingPixels;
        for (int x = startX; x < size.width(); x += gridSpacingPixels) {
            painter.drawLine(x, 0, x, size.height());
        }

        // 水平线
        int startY = (size.height() / 2) % gridSpacingPixels;
        for (int y = startY; y < size.height(); y += gridSpacingPixels) {
            painter.drawLine(0, y, size.width(), y);
        }
    }
}

void SeaChartWidget::drawCoordinateAxis(QPainter& painter)
{
    painter.setPen(QPen(QColor(255, 255, 255, 150), 2)); // 半透明白色坐标轴

    QPoint center = rect().center();

    // 绘制中心十字线
    painter.drawLine(center.x() - 20, center.y(), center.x() + 20, center.y());
    painter.drawLine(center.x(), center.y() - 20, center.x(), center.y() + 20);

    // 绘制坐标信息
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);

    QString coordText = QString("中心: %1°E, %2°N\n缩放: %3 m/px")
                        .arg(m_chartCenter.x(), 0, 'f', 4)
                        .arg(m_chartCenter.y(), 0, 'f', 4)
                        .arg(m_chartScale, 0, 'f', 1);

    painter.drawText(10, 20, coordText);
}

void SeaChartWidget::drawPlatforms(QPainter& painter)
{
    for (const auto& platform : m_platforms) {
        if (platform.isVisible) {
            drawPlatform(painter, platform);
        }
    }
}

void SeaChartWidget::drawPlatform(QPainter& painter, const ChartPlatform& platform)
{
    QPoint screenPos = geoToScreen(platform.position);

    // 修复：扩大可见区域检查范围，避免边缘目标被过滤
    QRect visibleRect = rect().adjusted(-PLATFORM_SIZE*2, -PLATFORM_SIZE*2,
                                       PLATFORM_SIZE*2, PLATFORM_SIZE*2);
    if (!visibleRect.contains(screenPos)) {
        // 调试信息
        if (!platform.isOwnShip) {
            qDebug() << QString("Platform %1 outside visible area: screen=(%2,%3), rect=(%4,%5,%6,%7)")
                        .arg(platform.name)
                        .arg(screenPos.x()).arg(screenPos.y())
                        .arg(visibleRect.x()).arg(visibleRect.y())
                        .arg(visibleRect.width()).arg(visibleRect.height());
        }
        return;
    }

    painter.save();

    // 移动到平台位置
    painter.translate(screenPos);

    // 旋转到平台航向
    painter.rotate(platform.heading);

    // 设置绘制参数
    QPen pen(platform.color, 2);
    QBrush brush(platform.color);

    if (platform.isOwnShip) {
        // 本艇使用特殊样式
        pen.setWidth(3);
        brush.setColor(QColor(255, 0, 0, 200)); // 半透明红色
        painter.setPen(pen);
        painter.setBrush(brush);

        // 绘制本艇图标（船型）
        QPolygon shipShape;
        shipShape << QPoint(0, -PLATFORM_SIZE)      // 船头
                  << QPoint(-PLATFORM_SIZE/2, PLATFORM_SIZE/2)   // 左后
                  << QPoint(0, PLATFORM_SIZE/3)     // 船尾中部
                  << QPoint(PLATFORM_SIZE/2, PLATFORM_SIZE/2);   // 右后

        painter.drawPolygon(shipShape);

        // 绘制方向指示线
        painter.setPen(QPen(Qt::yellow, 2));
        painter.drawLine(0, -PLATFORM_SIZE, 0, -PLATFORM_SIZE - 15);

    } else {
        // 目标舰船使用更明显的图标
        painter.setPen(pen);
        painter.setBrush(brush);

        // 修复：使用更大更明显的目标图标
        QPolygon targetShape;
        int size = PLATFORM_SIZE + 2; // 比本艇稍大一点
        targetShape << QPoint(0, -size)
                    << QPoint(-size, size)
                    << QPoint(size, size);

        painter.drawPolygon(targetShape);

        // 外框使目标更明显
        painter.setPen(QPen(Qt::white, 1));
        painter.drawPolygon(targetShape);
    }

    painter.restore();




    // 绘制平台标签
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    QString label;

    // 目标舰船显示名称和速度，中间加空格
    label = QString("%1\n %2节")
            .arg(platform.name)
            .arg(platform.speed, 0, 'f', 1);






    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(label);

    // 标签位置在平台右上方
    QPoint labelPos = screenPos + QPoint(PLATFORM_SIZE + 5, -PLATFORM_SIZE);

    // 绘制半透明背景
    painter.fillRect(labelPos.x() - 2, labelPos.y() - textRect.height(),
                    textRect.width() + 4, textRect.height() + 4,
                    QColor(0, 0, 0, 150));

    painter.drawText(labelPos, label);
}

void SeaChartWidget::drawSonarRanges(QPainter& painter)
{
    // *** 关键修复：增强本艇位置验证 ***
    if (!m_ownShip) {
        qDebug() << "Warning: No own ship found for sonar ranges drawing";
        return;
    }

    QPointF ownShipPos = m_ownShip->position;
    double ownShipHeading = m_ownShip->heading;

    // *** 修复核心：验证本艇位置的有效性 ***
    if (qAbs(ownShipPos.x()) < 0.001 && qAbs(ownShipPos.y()) < 0.001) {
        qDebug() << "Warning: Own ship position is (0,0), skipping sonar ranges drawing";
        qDebug() << "Own ship details: id=" << m_ownShip->id
                 << ", name=" << m_ownShip->name
                 << ", position=(" << ownShipPos.x() << "," << ownShipPos.y() << ")"
                 << ", heading=" << ownShipHeading;
        return;
    }

    QPoint ownShipScreen = geoToScreen(ownShipPos);

    // 验证屏幕坐标是否合理
    if (!rect().adjusted(-1000, -1000, 1000, 1000).contains(ownShipScreen)) {
        qDebug() << "Warning: Own ship screen position out of reasonable bounds:" << ownShipScreen;
        // 不return，继续绘制，但记录警告
    }

    painter.save();

    for (const auto& range : m_sonarRanges) {
        if (!range.isVisible) {
            continue;
        }

        // *** 修复：再次验证位置有效性 ***
        if (qAbs(ownShipPos.x()) < 0.001 && qAbs(ownShipPos.y()) < 0.001) {
            qDebug() << "Critical: Own ship position became (0,0) during sonar range drawing!";
            break; // 立即停止绘制
        }

        painter.setPen(QPen(range.color, 2));
        painter.setBrush(QBrush(range.color));

        // 计算声纳扇形的角度范围
        double startAngle = range.startAngle + ownShipHeading; // 相对于北向
        double endAngle = range.endAngle + ownShipHeading;

        // 标准化角度到0-360度
        while (startAngle < 0) startAngle += 360;
        while (startAngle >= 360) startAngle -= 360;
        while (endAngle < 0) endAngle += 360;
        while (endAngle >= 360) endAngle -= 360;

        // 将角度转换为Qt的角度系统（以3点钟方向为0度，顺时针为正）
        double qtStartAngle = 90 - startAngle; // 转换为Qt坐标系
        double qtEndAngle = 90 - endAngle;

        // 计算扫过的角度
        double sweepAngle = endAngle - startAngle;
        if (sweepAngle < 0) sweepAngle += 360;
        if (sweepAngle > 180) sweepAngle = sweepAngle - 360; // 处理跨越边界的情况

        // 将扫过角度转换为Qt坐标系（Qt中逆时针为正）
        double qtSweepAngle = -sweepAngle;

        // 计算声纳覆盖区域的半径（像素）
        double radiusPixels = range.maxRange / m_chartScale;

        // 确保半径不会太大（避免绘制超大扇形导致性能问题）
        radiusPixels = qMin(radiusPixels, 2000.0);

        // 计算扇形的边界矩形
        QRectF boundingRect(ownShipScreen.x() - radiusPixels,
                           ownShipScreen.y() - radiusPixels,
                           2 * radiusPixels,
                           2 * radiusPixels);

        // 绘制扇形
        painter.drawPie(boundingRect,
                       qtStartAngle * 16,  // Qt需要1/16度单位
                       qtSweepAngle * 16);

        // *** 调试信息：记录绘制详情 ***
//        qDebug() << QString("Sonar %1: pos=(%2,%3), screen=(%4,%5), start=%6°, end=%7°, radius=%8m")
//                    .arg(range.sonarId)
//                    .arg(ownShipPos.x(), 0, 'f', 6)
//                    .arg(ownShipPos.y(), 0, 'f', 6)
//                    .arg(ownShipScreen.x())
//                    .arg(ownShipScreen.y())
//                    .arg(startAngle, 0, 'f', 1)
//                    .arg(endAngle, 0, 'f', 1)
//                    .arg(radiusPixels * m_chartScale, 0, 'f', 1);
    }

    painter.restore();
}

void SeaChartWidget::drawSonarRange(QPainter& painter, const SonarDetectionRange& range, const QPointF& centerPos)
{
    QPoint centerScreen = geoToScreen(centerPos);
    double rangeRadius = range.maxRange / m_chartScale;

    if (rangeRadius < 5) return;

    painter.save();

    // *** 确保颜色设置正确 ***
    QPen pen(range.color, 2);
    QBrush brush(range.color);
    painter.setPen(pen);
    painter.setBrush(brush);

    painter.translate(centerScreen);

    // 计算绝对角度（考虑本艇航向）
    float ownShipHeading = m_ownShip ? m_ownShip->heading : 0.0f;
    float startAngle = range.startAngle + ownShipHeading;
    float endAngle = range.endAngle + ownShipHeading;

    // 标准化角度到0-360范围
    auto normalizeAngle = [](float angle) -> float {
        while (angle < 0) angle += 360.0f;
        while (angle >= 360) angle -= 360.0f;
        return angle;
    };

    startAngle = normalizeAngle(startAngle);
    endAngle = normalizeAngle(endAngle);

    // *** 特殊处理跨越0度的情况 ***
    QRectF boundingRect(-rangeRadius, -rangeRadius, 2 * rangeRadius, 2 * rangeRadius);

    if (startAngle > endAngle) {
        // 跨越0度，分两段绘制
        // 第一段：从startAngle到360度
        float span1 = 360.0f - startAngle;
        int qtStart1 = static_cast<int>((90 - startAngle - span1) * 16);
        int qtSpan1 = static_cast<int>(span1 * 16);
        painter.drawPie(boundingRect, qtStart1, qtSpan1);

        // 第二段：从0度到endAngle
        float span2 = endAngle;
        int qtStart2 = static_cast<int>((90 - span2) * 16);
        int qtSpan2 = static_cast<int>(span2 * 16);
        painter.drawPie(boundingRect, qtStart2, qtSpan2);
    } else {
        // 正常情况，不跨越0度
        float spanAngle = endAngle - startAngle;
        int qtStartAngle = static_cast<int>((90 - startAngle - spanAngle) * 16);
        int qtSpanAngle = static_cast<int>(spanAngle * 16);
        painter.drawPie(boundingRect, qtStartAngle, qtSpanAngle);
    }

    painter.restore();
}

// *** 鼠标事件处理 ***
void SeaChartWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();

    if (event->button() == Qt::LeftButton) {
        // 检查是否点击在平台上
        int platformId = findPlatformAt(event->pos());

        if (platformId != -1) {
            m_isDraggingPlatform = true;
            m_draggedPlatformId = platformId;
            setCursor(Qt::ClosedHandCursor);
        } else {
            m_isDragging = true;
            setCursor(Qt::OpenHandCursor);
        }
    }
}

void SeaChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPoint currentPos = event->pos();
    QPoint delta = currentPos - m_lastMousePos;

    if (m_isDraggingPlatform && m_draggedPlatformId != -1) {
        // 拖拽平台
        QPointF geoPos = screenToGeo(currentPos);
        updatePlatformPosition(m_draggedPlatformId, geoPos);

    } else if (m_isDragging) {
        // 拖拽海图
        double deltaLon = -delta.x() * m_chartScale / 111320.0; // 近似：1度经度 ≈ 111320米
        double deltaLat = delta.y() * m_chartScale / 111320.0;   // 近似：1度纬度 ≈ 111320米

        m_chartCenter.setX(m_chartCenter.x() + deltaLon);
        m_chartCenter.setY(m_chartCenter.y() + deltaLat);

        updateControlPanelValues();
        update();
    }

    m_lastMousePos = currentPos;
}

void SeaChartWidget::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);

    m_isDragging = false;
    m_isDraggingPlatform = false;
    m_draggedPlatformId = -1;
    setCursor(Qt::ArrowCursor);
}

void SeaChartWidget::wheelEvent(QWheelEvent* event)
{
    // 鼠标滚轮缩放
    double scaleFactor = 1.0;

    if (event->angleDelta().y() > 0) {
        scaleFactor = 0.8; // 放大
    } else {
        scaleFactor = 1.25; // 缩小
    }

    double newScale = m_chartScale * scaleFactor;
    m_chartScale = qBound(m_minScale, newScale, m_maxScale);

    updateControlPanelValues();
    update();

    event->accept();
}


void SeaChartWidget::contextMenuEvent(QContextMenuEvent* event)
{
    // *** 保存本艇位置状态，防止被污染 ***
    QPointF savedOwnShipPos;
    double savedOwnShipHeading = 0.0;
    bool hasValidOwnShip = false;

    if (m_ownShip) {
        savedOwnShipPos = m_ownShip->position;
        savedOwnShipHeading = m_ownShip->heading;
        hasValidOwnShip = true;

        qDebug() << "Context menu: saving own ship position:" << savedOwnShipPos
                 << "heading:" << savedOwnShipHeading;
    }

    // *** 保存声纳范围状态 ***
    std::vector<bool> sonarRangesBackup;
    sonarRangesBackup.reserve(m_sonarRanges.size());

    for (const auto& range : m_sonarRanges) {
        sonarRangesBackup.push_back(range.isVisible);
    }

    // 检查右键点击位置是否有平台
    int platformIndex = findPlatformAt(event->pos());

    if (platformIndex >= 0 && platformIndex < m_platforms.size()) {
        // 点击在平台上
        m_editPlatformAction->setVisible(true);
        m_removeTargetAction->setVisible(!m_platforms[platformIndex].isOwnShip);
    } else {
        // 点击在空白处
        m_editPlatformAction->setVisible(false);
        m_removeTargetAction->setVisible(false);
    }

    // *** 在显示菜单前验证本艇状态 ***
    if (m_ownShip && hasValidOwnShip) {
        if (qAbs(m_ownShip->position.x() - savedOwnShipPos.x()) > 0.0001 ||
            qAbs(m_ownShip->position.y() - savedOwnShipPos.y()) > 0.0001) {
            qDebug() << "Warning: Own ship position changed before context menu!";
            qDebug() << "Before:" << savedOwnShipPos << "Now:" << m_ownShip->position;
        }
    }

    // 显示菜单
    m_contextMenu->exec(event->globalPos());

    // *** 菜单关闭后，验证并恢复本艇状态 ***
    if (hasValidOwnShip && m_ownShip) {
        if (qAbs(m_ownShip->position.x()) < 0.001 && qAbs(m_ownShip->position.y()) < 0.001) {
            qDebug() << "Critical: Own ship position was reset to (0,0) after context menu!";
            qDebug() << "Restoring saved position:" << savedOwnShipPos;

            // 恢复本艇位置
            m_ownShip->position = savedOwnShipPos;
            m_ownShip->heading = savedOwnShipHeading;
        }
    }

    // *** 恢复声纳范围状态 ***
    bool needRestore = false;
    for (int i = 0; i < m_sonarRanges.size() && i < sonarRangesBackup.size(); i++) {
        if (m_sonarRanges[i].isVisible != sonarRangesBackup[i]) {
            m_sonarRanges[i].isVisible = sonarRangesBackup[i];
            needRestore = true;
        }
    }

    if (needRestore) {
        qDebug() << "Restored sonar ranges visibility after context menu";
    }

    // 强制验证并重绘
    validateSonarRanges();
    update();
}

// *** 坐标转换方法 ***
QPoint SeaChartWidget::geoToScreen(const QPointF& geoPos) const
{
    // 简化的坐标转换，假设局部区域为平面坐标系
    double deltaLon = geoPos.x() - m_chartCenter.x();
    double deltaLat = geoPos.y() - m_chartCenter.y();

    // 转换为米（近似）
    double deltaXMeters = deltaLon * 111320.0; // 1度经度约111320米
    double deltaYMeters = deltaLat * 111320.0; // 1度纬度约111320米

    // 转换为屏幕坐标
    int screenX = rect().center().x() + static_cast<int>(deltaXMeters / m_chartScale);
    int screenY = rect().center().y() - static_cast<int>(deltaYMeters / m_chartScale); // Y轴翻转

    return QPoint(screenX, screenY);
}

QPointF SeaChartWidget::screenToGeo(const QPoint& screenPos) const
{
    // 屏幕坐标转地理坐标
    int deltaX = screenPos.x() - rect().center().x();
    int deltaY = rect().center().y() - screenPos.y(); // Y轴翻转

    // 转换为米
    double deltaXMeters = deltaX * m_chartScale;
    double deltaYMeters = deltaY * m_chartScale;

    // 转换为地理坐标
    double lon = m_chartCenter.x() + deltaXMeters / 111320.0;
    double lat = m_chartCenter.y() + deltaYMeters / 111320.0;

    return QPointF(lon, lat);
}

int SeaChartWidget::findPlatformAt(const QPoint& pos) const
{
    for (int i = 0; i < m_platforms.size(); i++) {
        const auto& platform = m_platforms[i];
        if (!platform.isVisible) continue;

        QPoint platformPos = geoToScreen(platform.position);

        // *** 检查屏幕坐标是否有效 ***
        if (platformPos.x() < -1000 || platformPos.x() > width() + 1000 ||
            platformPos.y() < -1000 || platformPos.y() > height() + 1000) {
            continue; // 平台在屏幕范围外
        }

        QRect platformRect(platformPos - QPoint(PLATFORM_SIZE, PLATFORM_SIZE),
                          QSize(2 * PLATFORM_SIZE, 2 * PLATFORM_SIZE));

        if (platformRect.contains(pos)) {
            return i; // 返回数组索引
        }
    }
    return -1;
}

double SeaChartWidget::calculateDistance(const QPointF& pos1, const QPointF& pos2) const
{
    // 简化的距离计算（适用于较小的区域）
    double deltaLon = pos1.x() - pos2.x();
    double deltaLat = pos1.y() - pos2.y();

    double deltaXMeters = deltaLon * 111320.0;
    double deltaYMeters = deltaLat * 111320.0;

    return qSqrt(deltaXMeters * deltaXMeters + deltaYMeters * deltaYMeters);
}

double SeaChartWidget::calculateBearing(const QPointF& from, const QPointF& to) const
{
    double deltaLon = to.x() - from.x();
    double deltaLat = to.y() - from.y();

    double bearing = qAtan2(deltaLon, deltaLat) * 180.0 / M_PI;

    // 转换为0-360度范围
    if (bearing < 0) {
        bearing += 360.0;
    }

    return bearing;
}

// *** 槽函数实现 ***
void SeaChartWidget::onAddTargetShip()
{
    // *** 保存声纳状态 ***
    QVector<bool> sonarVisibility;
    for (const auto& range : m_sonarRanges) {
        sonarVisibility.append(range.isVisible);
    }

    QPointF mouseGeoPos = screenToGeo(m_lastMousePos);

    bool ok;
    QString name = QInputDialog::getText(this, "添加目标舰船", "舰船名称:",
                                        QLineEdit::Normal,
                                        QString("目标舰船%1").arg(m_nextPlatformId - 999), &ok);

    if (ok && !name.isEmpty()) {
        ChartPlatform newTarget;
        newTarget.id = m_nextPlatformId++;
        newTarget.name = name;
        newTarget.position = mouseGeoPos;
        newTarget.heading = 0.0;
        newTarget.speed = 12.0;
        newTarget.color = Qt::cyan;
        newTarget.isOwnShip = false;
        newTarget.isVisible = true;

        addPlatform(newTarget);
        qDebug() << "Added target ship:" << name << "at position:" << mouseGeoPos;
    }

    // *** 恢复声纳状态（如果需要） ***
    for (int i = 0; i < m_sonarRanges.size() && i < sonarVisibility.size(); i++) {
        if (m_sonarRanges[i].isVisible != sonarVisibility[i]) {
            m_sonarRanges[i].isVisible = sonarVisibility[i];
        }
    }

    update();
}

void SeaChartWidget::onRemoveTargetShip()
{
    int platformIndex = findPlatformAt(m_lastMousePos);
    if (platformIndex >= 0 && platformIndex < m_platforms.size() &&
        !m_platforms[platformIndex].isOwnShip) {
        int platformId = m_platforms[platformIndex].id;
        removePlatform(platformId);
    }

    // *** 强制刷新确保状态正确 ***
    validateSonarRanges();
}

void SeaChartWidget::onEditPlatform()
{
    int platformIndex = findPlatformAt(m_lastMousePos);
    if (platformIndex != -1) {
        ChartPlatform& platform = m_platforms[platformIndex];

        // 简单的编辑对话框
        bool ok;
        double speed = QInputDialog::getDouble(this, "编辑平台", "速度 (节):",
                                             platform.speed, 0.0, 100.0, 1, &ok);
        if (ok) {
            platform.speed = speed;

            double heading = QInputDialog::getDouble(this, "编辑平台", "航向 (度):",
                                                   platform.heading, 0.0, 359.9, 1, &ok);
            if (ok) {
                platform.heading = heading;
                update();

                if (!platform.isOwnShip) {
                    emit targetPlatformsUpdated(getTargetPlatforms());
                }
            }
        }
    }
}

void SeaChartWidget::onCenterOnOwnShip()
{
    if (m_ownShip) {
        // 只更新中心位置，不修改本艇的航向
        m_chartCenter = m_ownShip->position;
        updateControlPanelValues();
        update();

        qDebug() << "Centered on own ship at:" << m_ownShip->position
                 << "heading preserved:" << m_ownShip->heading;
    }
}

void SeaChartWidget::onAutoGenerateTargets()
{
    // 在本艇周围随机生成几个目标
    if (!m_ownShip) {
        qDebug() << "Error: No own ship found for auto generation";
        return;
    }

    QPointF ownPos = m_ownShip->position;
    qDebug() << "Auto generating targets around own ship at:" << ownPos;

    // 生成3-5个目标
    int targetCount = 3 + (qrand() % 3);

    // 修复：根据当前缩放级别调整生成距离
    double maxViewDistance = m_chartScale * qMin(width(), height()) / 4; // 屏幕1/4范围
    double minDistance = qMax(2000.0, maxViewDistance * 0.1);   // 最小2公里或视图范围的10%
    double maxDistance = qMax(8000.0, maxViewDistance * 0.8);   // 最大8公里或视图范围的80%

    qDebug() << "Generation range:" << minDistance/1000.0 << "km to" << maxDistance/1000.0 << "km";
    qDebug() << "Current chart scale:" << m_chartScale << "m/px";

    QVector<ChartPlatform> newTargets; // 收集新生成的目标

    for (int i = 0; i < targetCount; i++) {
        // 修复：调整生成距离范围，确保在可视范围内
        double distance = minDistance + (qrand() % static_cast<int>(maxDistance - minDistance));
        double bearing = (qrand() % 360) * M_PI / 180.0; // 转换为弧度

        // 修复：正确的坐标计算
        // 注意：经度方向是x轴（东向为正），纬度方向是y轴（北向为正）
        double deltaLat = (distance * cos(bearing)) / 111320.0;  // 北向分量
        double deltaLon = (distance * sin(bearing)) / 111320.0;  // 东向分量

        QPointF targetPos(ownPos.x() + deltaLon, ownPos.y() + deltaLat);

        // 验证生成的位置
        QPoint screenPos = geoToScreen(targetPos);
        bool inView = rect().contains(screenPos);

        ChartPlatform newTarget;
        newTarget.id = m_nextPlatformId++;
        newTarget.name = QString("自动目标%1").arg(i + 1);
        newTarget.position = targetPos;
        newTarget.heading = qrand() % 360;
        newTarget.speed = 8.0 + (qrand() % 20); // 8-28节随机速度
        newTarget.color = QColor::fromHsv(qrand() % 360, 200, 255); // 随机颜色
        newTarget.isOwnShip = false;
        newTarget.isVisible = true;

        qDebug() << QString("Generated target %1 at (%2, %3), distance=%4km, screen=(%5,%6), inView=%7")
                    .arg(newTarget.name)
                    .arg(targetPos.x(), 0, 'f', 6)
                    .arg(targetPos.y(), 0, 'f', 6)
                    .arg(distance/1000.0, 0, 'f', 1)
                    .arg(screenPos.x()).arg(screenPos.y())
                    .arg(inView ? "YES" : "NO");

        addPlatform(newTarget);
        newTargets.append(newTarget);
    }

    qDebug() << "Auto generated" << targetCount << "target ships";

    // 修复：生成后自动调整视图以显示所有目标
    if (!newTargets.isEmpty()) {
        // 延迟调整视图，确保平台已被添加
        QTimer::singleShot(100, this, [this]() {
            fitToTargets();
            qDebug() << "Auto-fitted view to show all targets";
        });
    }

    // 强制重绘
    update();
}

void SeaChartWidget::generateAndSendPropagatedSoundData()
{
    // 这个方法会每5秒调用一次，生成传播声数据
    if (m_mainWindow) {
        QVector<ChartPlatform> targets = getTargetPlatforms();
        if (!targets.isEmpty()) {
            // 调用主窗口的方法来生成和发送传播声数据
            // 这里需要主窗口提供相应的接口
            qDebug() << "Generating propagated sound data for" << targets.size() << "targets";

            // 发出信号通知主窗口
            emit targetPlatformsUpdated(targets);
        }
    }
}

// 强制验证并修复本艇位置
bool SeaChartWidget::isValidPosition(const QPointF& pos) const
{
    // 检查是否为(0,0)
    if (qAbs(pos.x()) < 0.001 && qAbs(pos.y()) < 0.001) {
        return false;
    }

    // 检查是否在合理的地理坐标范围内
    // 经度范围：-180 到 180
    // 纬度范围：-90 到 90
    if (qAbs(pos.x()) > 180.0 || qAbs(pos.y()) > 90.0) {
        return false;
    }

    // 检查是否为明显的异常值（如NaN或极大值）
    if (qIsNaN(pos.x()) || qIsNaN(pos.y()) ||
        qIsInf(pos.x()) || qIsInf(pos.y())) {
        return false;
    }

    return true;
}

// === 实现2：本艇位置验证和修复函数 ===
void SeaChartWidget::validateAndFixOwnShipPosition()
{
    if (!m_ownShip) {
        // 查找本艇
        for (auto& platform : m_platforms) {
            if (platform.isOwnShip) {
                m_ownShip = &platform;
                qDebug() << "Found own ship:" << platform.name << "at" << platform.position;
                break;
            }
        }

        if (!m_ownShip) {
            qDebug() << "Warning: No own ship found in platform list";
            return;
        }
    }

    // 验证本艇位置是否有效
    if (!isValidPosition(m_ownShip->position)) {
        qDebug() << "Invalid own ship position detected:" << m_ownShip->position;

        // 尝试从平台列表中找到有效的本艇位置
        bool foundValidOwnShip = false;
        for (auto& platform : m_platforms) {
            if (platform.isOwnShip && isValidPosition(platform.position)) {
                qDebug() << "Found valid own ship in platform list:" << platform.position;
                m_ownShip = &platform;
                foundValidOwnShip = true;
                break;
            }
        }

        // 如果没有找到有效位置，设置默认位置
        if (!foundValidOwnShip) {
            qDebug() << "Setting default own ship position";
            m_ownShip->position = QPointF(DEFAULT_LON, DEFAULT_LAT); // 使用类中定义的默认坐标
            m_ownShip->heading = 45.0; // 默认朝向东北

            qDebug() << "Own ship position reset to:" << m_ownShip->position;
        }
    }

    // 额外验证：确保本艇指针指向的是platforms数组中的有效元素
    bool pointerValid = false;
    for (const auto& platform : m_platforms) {
        if (&platform == m_ownShip) {
            pointerValid = true;
            break;
        }
    }

    if (!pointerValid) {
        qDebug() << "Warning: Own ship pointer is invalid, resetting";
        m_ownShip = nullptr;
        // 递归调用来重新查找
        validateAndFixOwnShipPosition();
    }
}




QWidget* SeaChartWidget::getControlPanel() const
{
    return m_controlPanel;
}

void SeaChartWidget::setSeaChartVisible(bool visible)
{
    setVisible(visible);
}

bool SeaChartWidget::isSeaChartVisible() const
{
    return isVisible();
}

void SeaChartWidget::resetView()
{
    // 重置到默认视图
    m_chartCenter = QPointF(DEFAULT_LON, DEFAULT_LAT);
    m_chartScale = DEFAULT_SCALE;
    updateControlPanelValues();
    update();

    qDebug() << "Sea chart view reset to default";
}

void SeaChartWidget::zoomIn()
{
    double newScale = m_chartScale * 0.8;
    m_chartScale = qBound(m_minScale, newScale, m_maxScale);
    updateControlPanelValues();
    update();
}

void SeaChartWidget::zoomOut()
{
    double newScale = m_chartScale * 1.25;
    m_chartScale = qBound(m_minScale, newScale, m_maxScale);
    updateControlPanelValues();
    update();
}

void SeaChartWidget::panUp(double distance)
{
    double deltaLat = distance / 111320.0; // 转换为度
    m_chartCenter.setY(m_chartCenter.y() + deltaLat);
    updateControlPanelValues();
    update();
}

void SeaChartWidget::panDown(double distance)
{
    double deltaLat = distance / 111320.0; // 转换为度
    m_chartCenter.setY(m_chartCenter.y() - deltaLat);
    updateControlPanelValues();
    update();
}

void SeaChartWidget::panLeft(double distance)
{
    double deltaLon = distance / 111320.0; // 转换为度
    m_chartCenter.setX(m_chartCenter.x() - deltaLon);
    updateControlPanelValues();
    update();
}

void SeaChartWidget::panRight(double distance)
{
    double deltaLon = distance / 111320.0; // 转换为度
    m_chartCenter.setX(m_chartCenter.x() + deltaLon);
    updateControlPanelValues();
    update();
}

void SeaChartWidget::fitToTargets()
{
    if (m_platforms.size() <= 1) {
        // 只有本艇，居中本艇
        if (m_ownShip) {
            setChartView(m_ownShip->position, m_chartScale);
        }
        return;
    }

    // 计算所有平台的边界
    double minLon = 1000, maxLon = -1000;
    double minLat = 1000, maxLat = -1000;
    int validPlatformCount = 0;

    for (const auto& platform : m_platforms) {
        if (platform.isVisible) {
            minLon = qMin(minLon, platform.position.x());
            maxLon = qMax(maxLon, platform.position.x());
            minLat = qMin(minLat, platform.position.y());
            maxLat = qMax(maxLat, platform.position.y());
            validPlatformCount++;
        }
    }

    if (validPlatformCount == 0) {
        qDebug() << "No visible platforms to fit";
        return;
    }

    // 计算中心点
    QPointF center((minLon + maxLon) / 2.0, (minLat + maxLat) / 2.0);

    // 计算合适的缩放级别
    double lonRange = maxLon - minLon;
    double latRange = maxLat - minLat;
    double maxRangeMeters = qMax(lonRange * 111320.0, latRange * 111320.0); // 转换为米

    // 修复：确保有最小显示范围
    maxRangeMeters = qMax(maxRangeMeters, 5000.0); // 最小5公里范围

    double newScale = maxRangeMeters / (qMin(width(), height()) * 0.8); // 留20%边距
    newScale = qBound(m_minScale, newScale, m_maxScale);

    qDebug() << QString("Fitting view: center=(%1,%2), lonRange=%3km, latRange=%4km, newScale=%5")
                .arg(center.x(), 0, 'f', 6)
                .arg(center.y(), 0, 'f', 6)
                .arg(lonRange * 111.32, 0, 'f', 1)
                .arg(latRange * 111.32, 0, 'f', 1)
                .arg(newScale, 0, 'f', 1);

    setChartView(center, newScale);
}

QString SeaChartWidget::getPlatformInfo(int platformId) const
{
    for (const auto& platform : m_platforms) {
        if (platform.id == platformId) {
            return QString("平台: %1\n位置: %2°E, %3°N\n航向: %4°\n速度: %5节")
                   .arg(platform.name)
                   .arg(platform.position.x(), 0, 'f', 4)
                   .arg(platform.position.y(), 0, 'f', 4)
                   .arg(platform.heading, 0, 'f', 1)
                   .arg(platform.speed, 0, 'f', 1);
        }
    }
    return QString("平台ID %1 未找到").arg(platformId);
}

QVector<int> SeaChartWidget::getVisiblePlatformIds() const
{
    QVector<int> ids;
    for (const auto& platform : m_platforms) {
        if (platform.isVisible) {
            ids.append(platform.id);
        }
    }
    return ids;
}

void SeaChartWidget::setPlatformVisible(int platformId, bool visible)
{
    for (auto& platform : m_platforms) {
        if (platform.id == platformId) {
            platform.isVisible = visible;
            update();

            if (!platform.isOwnShip) {
                emit targetPlatformsUpdated(getTargetPlatforms());
            }

            qDebug() << "Platform" << platform.name << "visibility set to:" << visible;
            break;
        }
    }
}

void SeaChartWidget::setAllPlatformsVisible(bool visible)
{
    for (auto& platform : m_platforms) {
        platform.isVisible = visible;
    }
    emit targetPlatformsUpdated(getTargetPlatforms());
    update();

    qDebug() << "All platforms visibility set to:" << visible;
}

void SeaChartWidget::exportChart(const QString& fileName)
{
    // 导出海图为图片
    QPixmap pixmap(size());
    render(&pixmap);

    if (pixmap.save(fileName)) {
        qDebug() << "Chart exported successfully to:" << fileName;
    } else {
        qDebug() << "Failed to export chart to:" << fileName;
    }
}

void SeaChartWidget::saveConfiguration(const QString& fileName)
{
    // 保存海图配置到文件
    QSettings settings(fileName, QSettings::IniFormat);

    // 保存视图参数
    settings.beginGroup("View");
    settings.setValue("centerLon", m_chartCenter.x());
    settings.setValue("centerLat", m_chartCenter.y());
    settings.setValue("scale", m_chartScale);
    settings.endGroup();

    // 保存平台信息
    settings.beginGroup("Platforms");
    settings.setValue("count", m_platforms.size());
    for (int i = 0; i < m_platforms.size(); i++) {
        const auto& platform = m_platforms[i];
        settings.beginGroup(QString("Platform_%1").arg(i));
        settings.setValue("id", platform.id);
        settings.setValue("name", platform.name);
        settings.setValue("lon", platform.position.x());
        settings.setValue("lat", platform.position.y());
        settings.setValue("heading", platform.heading);
        settings.setValue("speed", platform.speed);
        settings.setValue("isOwnShip", platform.isOwnShip);
        settings.setValue("isVisible", platform.isVisible);
        settings.endGroup();
    }
    settings.endGroup();

    // 保存声纳范围显示状态
    settings.beginGroup("SonarRanges");
    for (int i = 0; i < m_sonarRanges.size(); i++) {
        const auto& range = m_sonarRanges[i];
        settings.beginGroup(QString("Sonar_%1").arg(i));
        settings.setValue("sonarId", range.sonarId);
        settings.setValue("visible", range.isVisible);
        settings.endGroup();
    }
    settings.endGroup();

    qDebug() << "Configuration saved to:" << fileName;
}

void SeaChartWidget::loadConfiguration(const QString& fileName)
{
    // 从文件加载海图配置
    QSettings settings(fileName, QSettings::IniFormat);

    if (!QFile::exists(fileName)) {
        qDebug() << "Configuration file does not exist:" << fileName;
        return;
    }

    // 加载视图参数
    settings.beginGroup("View");
    double centerLon = settings.value("centerLon", DEFAULT_LON).toDouble();
    double centerLat = settings.value("centerLat", DEFAULT_LAT).toDouble();
    double scale = settings.value("scale", DEFAULT_SCALE).toDouble();
    settings.endGroup();

    setChartView(QPointF(centerLon, centerLat), scale);

    // 加载平台信息
    settings.beginGroup("Platforms");
    int platformCount = settings.value("count", 0).toInt();

    // 清除当前平台（除了本艇）
    clearAllPlatforms();

    for (int i = 0; i < platformCount; i++) {
        settings.beginGroup(QString("Platform_%1").arg(i));

        ChartPlatform platform;
        platform.id = settings.value("id", -1).toInt();
        platform.name = settings.value("name", "Unknown").toString();
        platform.position.setX(settings.value("lon", 0.0).toDouble());
        platform.position.setY(settings.value("lat", 0.0).toDouble());
        platform.heading = settings.value("heading", 0.0).toDouble();
        platform.speed = settings.value("speed", 0.0).toDouble();
        platform.isOwnShip = settings.value("isOwnShip", false).toBool();
        platform.isVisible = settings.value("isVisible", true).toBool();

        if (platform.isOwnShip) {
            // 更新本艇信息
            setOwnShipPosition(platform.position, platform.heading);
            if (m_ownShip) {
                m_ownShip->speed = platform.speed;
            }
        } else {
            // 目标平台
            addPlatform(platform);
        }

        settings.endGroup();
    }
    settings.endGroup();

    // 加载声纳范围显示状态
    settings.beginGroup("SonarRanges");
    for (int i = 0; i < m_sonarRanges.size(); i++) {
        settings.beginGroup(QString("Sonar_%1").arg(i));
        int sonarId = settings.value("sonarId", -1).toInt();
        bool visible = settings.value("visible", true).toBool();

        if (sonarId >= 0) {
            setSonarRangeVisible(sonarId, visible);
        }

        settings.endGroup();
    }
    settings.endGroup();

    qDebug() << "Configuration loaded from:" << fileName;
}

// 获取海图统计信息
QString SeaChartWidget::getChartStatistics() const
{
    QString stats;
    stats += QString("=== 海图统计信息 ===\n");
    stats += QString("中心位置: %1°E, %2°N\n")
             .arg(m_chartCenter.x(), 0, 'f', 4)
             .arg(m_chartCenter.y(), 0, 'f', 4);
    stats += QString("缩放比例: %1 米/像素\n").arg(m_chartScale, 0, 'f', 1);
    stats += QString("平台总数: %1\n").arg(m_platforms.size());

    int visibleCount = 0;
    int targetCount = 0;
    for (const auto& platform : m_platforms) {
        if (platform.isVisible) {
            visibleCount++;
            if (!platform.isOwnShip) {
                targetCount++;
            }
        }
    }

    stats += QString("可见平台: %1\n").arg(visibleCount);
    stats += QString("目标舰船: %1\n").arg(targetCount);

    if (m_ownShip) {
        stats += QString("本艇位置: %1°E, %2°N\n")
                 .arg(m_ownShip->position.x(), 0, 'f', 4)
                 .arg(m_ownShip->position.y(), 0, 'f', 4);
        stats += QString("本艇航向: %1°\n").arg(m_ownShip->heading, 0, 'f', 1);
        stats += QString("本艇速度: %1节\n").arg(m_ownShip->speed, 0, 'f', 1);
    }

    int activeSonarRanges = 0;
    for (const auto& range : m_sonarRanges) {
        if (range.isVisible) {
            activeSonarRanges++;
        }
    }
    stats += QString("显示声纳范围: %1/%2").arg(activeSonarRanges).arg(m_sonarRanges.size());

    return stats;
}

ChartPlatform SeaChartWidget::getOwnShipInfo() const
{
    if (m_ownShip) {
        return *m_ownShip;
    }
    return ChartPlatform(); // 返回空的平台信息
}

double SeaChartWidget::getOwnShipHeading() const
{
    // 安全检查：确保m_ownShip指针有效且在platforms数组范围内
    if (!m_ownShip) {
        qDebug() << "Warning: m_ownShip is null";
        return 45.0; // 返回默认朝向
    }

    // 验证指针是否仍然在有效范围内
    bool isValidPointer = false;
    for (const auto& platform : m_platforms) {
        if (&platform == m_ownShip && platform.isOwnShip) {
            isValidPointer = true;
            break;
        }
    }

    if (!isValidPointer) {
        qDebug() << "Warning: m_ownShip pointer is invalid, searching for own ship";
        // 重新查找本艇
        for (const auto& platform : m_platforms) {
            if (platform.isOwnShip) {
                return platform.heading;
            }
        }
        qDebug() << "Error: No own ship found in platforms list";
        return 45.0; // 返回默认朝向
    }

    return m_ownShip->heading;
}

void SeaChartWidget::findOwnShip()
{
    ChartPlatform* previousOwnShip = m_ownShip;
    m_ownShip = nullptr;

    for (auto& platform : m_platforms) {
        if (platform.isOwnShip) {
            m_ownShip = &platform;
            break;
        }
    }

    if (!m_ownShip) {
        qDebug() << "Warning: No own ship found in platform list";
        return;
    }

    // *** 验证本艇位置的合理性 ***
    if (qAbs(m_ownShip->position.x()) < 0.001 && qAbs(m_ownShip->position.y()) < 0.001) {
        qDebug() << "Warning: Own ship has invalid position (0,0)";

        // 如果之前有有效的本艇位置，考虑恢复
        if (previousOwnShip &&
            !(qAbs(previousOwnShip->position.x()) < 0.001 && qAbs(previousOwnShip->position.y()) < 0.001)) {
            qDebug() << "Restoring previous own ship position:" << previousOwnShip->position;
            m_ownShip->position = previousOwnShip->position;
            m_ownShip->heading = previousOwnShip->heading;
        } else {
            // 设置默认位置（如果没有之前的有效位置）
            m_ownShip->position = QPointF(120.0, 30.0); // 示例坐标
            m_ownShip->heading = 0.0;
            qDebug() << "Set default own ship position:" << m_ownShip->position;
        }
    }

    qDebug() << "Own ship found: id=" << m_ownShip->id
             << ", name=" << m_ownShip->name
             << ", position=" << m_ownShip->position
             << ", heading=" << m_ownShip->heading;
}

void SeaChartWidget::removeTarget()
{
    // 这个函数可以在右键菜单中调用来移除目标
    // 保存本艇引用
    ChartPlatform* ownShipBackup = m_ownShip;
    QPointF ownShipPosBackup;
    double ownShipHeadingBackup = 0.0;

    if (ownShipBackup) {
        ownShipPosBackup = ownShipBackup->position;
        ownShipHeadingBackup = ownShipBackup->heading;
    }

    // 获取要删除的平台ID（从右键菜单的上下文中）
    int platformToRemove = -1;

    // 查找当前鼠标位置下的平台
    int platformIndex = findPlatformAt(m_lastMousePos);
    if (platformIndex >= 0 && platformIndex < m_platforms.size()) {
        if (!m_platforms[platformIndex].isOwnShip) {
            platformToRemove = m_platforms[platformIndex].id;
        }
    }

    if (platformToRemove == -1) {
        qDebug() << "No valid target to remove";
        return;
    }

    // 移除目标
    for (int i = 0; i < m_platforms.size(); ++i) {
        if (m_platforms[i].id == platformToRemove && !m_platforms[i].isOwnShip) {
            qDebug() << "Removing target:" << m_platforms[i].name;
            m_platforms.removeAt(i);
            break;
        }
    }

    // *** 确保本艇指针仍然有效 ***
    m_ownShip = nullptr; // 先清空，然后重新查找
    bool ownShipFound = false;

    for (auto& platform : m_platforms) {
        if (platform.isOwnShip) {
            m_ownShip = &platform;
            ownShipFound = true;

            // 验证位置是否仍然有效
            if (!isValidPosition(m_ownShip->position) && isValidPosition(ownShipPosBackup)) {
                qDebug() << "Restoring own ship position after target removal";
                m_ownShip->position = ownShipPosBackup;
                m_ownShip->heading = ownShipHeadingBackup;
            }
            break;
        }
    }

    if (!ownShipFound) {
        qDebug() << "Critical: Own ship lost after target removal!";
        // 这种情况不应该发生，但如果发生了，尝试重新初始化本艇
        initializeOwnShip();
    }

    // 通知目标列表更新
    emit targetPlatformsUpdated(getTargetPlatforms());

    // 强制重绘
    update();
}

QPointF SeaChartWidget::getOwnShipPositionSafe() const
{
    // 先验证本艇指针
    if (!m_ownShip) {
        qDebug() << "Warning: m_ownShip is null, returning default position";
        return QPointF(DEFAULT_LON, DEFAULT_LAT);
    }

    // 验证指针有效性
    bool isValidPointer = false;
    for (const auto& platform : m_platforms) {
        if (&platform == m_ownShip && platform.isOwnShip) {
            isValidPointer = true;
            break;
        }
    }

    if (!isValidPointer) {
        qDebug() << "Warning: m_ownShip pointer is invalid, searching for own ship";
        for (const auto& platform : m_platforms) {
            if (platform.isOwnShip) {
                return platform.position;
            }
        }
        qDebug() << "Error: No own ship found in platforms list";
        return QPointF(DEFAULT_LON, DEFAULT_LAT);
    }

    // 验证位置有效性
    if (!isValidPosition(m_ownShip->position)) {
        qDebug() << "Warning: Own ship has invalid position:" << m_ownShip->position;
        return QPointF(DEFAULT_LON, DEFAULT_LAT);
    }

    return m_ownShip->position;
}

// === 实现5：增强的本艇朝向获取函数（安全版本） ===
double SeaChartWidget::getOwnShipHeadingSafe() const
{
    if (!m_ownShip) {
        qDebug() << "Warning: m_ownShip is null, returning default heading";
        return 45.0; // 默认朝向
    }

    // 验证指针有效性
    bool isValidPointer = false;
    for (const auto& platform : m_platforms) {
        if (&platform == m_ownShip && platform.isOwnShip) {
            isValidPointer = true;
            break;
        }
    }

    if (!isValidPointer) {
        qDebug() << "Warning: m_ownShip pointer is invalid, searching for own ship";
        for (const auto& platform : m_platforms) {
            if (platform.isOwnShip) {
                return platform.heading;
            }
        }
        return 45.0; // 默认朝向
    }

    return m_ownShip->heading;
}


//移动更新方法
void SeaChartWidget::updatePlatformMovement()
{
    for (auto& platform : m_platforms) {
        // 移除 !platform.isOwnShip 条件，让本艇也能移动
        if (platform.speed > 0) {
            // 根据航向和速度计算新位置
            double speedMs = platform.speed * 0.514444; // 节转米/秒
            double deltaTime = 1.0; // 1秒间隔
            double distance = speedMs * deltaTime; // 移动距离（米）

            // 计算新的经纬度位置
            double headingRad = platform.heading * M_PI / 180.0;
            double deltaLat = (distance * cos(headingRad)) / 111320.0;
            double deltaLon = (distance * sin(headingRad)) / 111320.0;

            QPointF oldPosition = platform.position;
            platform.position.setX(platform.position.x() + deltaLon);
            platform.position.setY(platform.position.y() + deltaLat);

            // 如果是本艇移动，需要特殊处理
            if (platform.isOwnShip) {
                // 可以选择是否自动调整海图中心跟随本艇
                // m_chartCenter = platform.position;  // 可选：让海图跟随本艇

                emit platformPositionChanged(platform.id, platform.position);

                m_mainWindow->addLog(QString("本艇自动移动: 从(%1,%2)到(%3,%4)")
                       .arg(oldPosition.x(), 0, 'f', 6)
                       .arg(oldPosition.y(), 0, 'f', 6)
                       .arg(platform.position.x(), 0, 'f', 6)
                       .arg(platform.position.y(), 0, 'f', 6));
            }
        }
    }

    emit targetPlatformsUpdated(getTargetPlatforms());
    update();
}
