#include "robot3d/shell/Robot3dDemoView.h"
#include "robot3d/presentation/Robot3dPresenter.h"
#include "robot3d/shell/RobotViewport.h"
#include "robot3d/shell/WorkpiecePanel.h"
#include <QTabWidget>
#include <QScrollArea>
#include <QCloseEvent>
#include "robot3d/composition/Robot3dComposition.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QStyle>
#include <QSignalBlocker>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QEvent>
#include <QEventLoop>
#include <QScopedValueRollback>
#ifdef ROBOT3D_WITH_VTK
#include "robot3d/shell/PointCloudViewerDialog.h"
#endif
namespace
{
const char *kRobotTypes[] = {"G6L-3-20", "G6-1-1", "X20-1-3"};
}
Robot3dDemoView::Robot3dDemoView(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QString::fromUtf8("CGX-TP 机械臂 3D 显示模块 —— MVP Demo"));
    m_viewport = new RobotViewport(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_viewport);
    splitter->addWidget(BuildControlPanel());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 900 << 360);
    auto *layout = new QVBoxLayout(this);
    layout->setMargin(6);
    layout->addWidget(splitter, 1);
    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(110);
    layout->addWidget(m_logEdit);
    connect(m_simCb, &QCheckBox::toggled, this, &Robot3dDemoView::OnSimToggled);
    composition_.reset(new robot3d::composition::Robot3dComposition(*this, *this, *m_viewport, "robotType"));
    connect(m_viewport, &RobotViewport::framePresented, this, &Robot3dDemoView::OnFramePresented, Qt::QueuedConnection);
    connect(m_viewport, &RobotViewport::renderFailed, this,
            [this](const QString &e)
            {
                showError(e.toStdString());
            });
    if (!qgetenv("DEMO_DUMP_FRAME").isEmpty())
        QTimer::singleShot(30000, this,
                           [this]
                           {
                               if (!m_dumpShotDone)
                               {
                                   qCritical() << "Timed out waiting for a rendered frame";
                                   qApp->exit(2);
                               }
                           });
    connect(m_backendCb, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int i)
            {
                m_viewport->requestBackend(m_backendCb->itemData(i).toString());
            });
    connect(m_viewport, &RobotViewport::backendChanged, this,
            [this](const QString &name)
            {
                QSignalBlocker block(m_backendCb);
                m_backendCb->setCurrentIndex(m_backendCb->findData(name));
                appendLog(std::string("渲染后端已切换: ") + name.toStdString());
            });
    appendLog("MVP: View → Presenter → Service → 场景快照 → Renderer");
}
Robot3dDemoView::~Robot3dDemoView()
{
    composition_.reset();
}
bool Robot3dDemoView::prepareModel(robot3d::domain::RobotConfigurationPtr c, std::string &error)
{
    return m_viewport->prepareModel(c, error);
}
void Robot3dDemoView::appendLog(const std::string &s)
{
    m_logEdit->appendPlainText(QDateTime::currentDateTime().toString("[HH:mm:ss.zzz] ") + QString::fromUtf8(s.c_str()));
}
void Robot3dDemoView::showError(const std::string &e)
{
    appendLog("错误: " + e);
    qWarning() << QString::fromUtf8(e.c_str());
}
void Robot3dDemoView::showState(const robot3d::presentation::Robot3dDisplayModel &m)
{
    model_ = m;
    m_updatingSliders = true;
    {
        QSignalBlocker b(m_robotTypeCb);
        bool changed = m_robotTypeCb->count() != int(m.robotTypes.size());
        for (int i = 0; !changed && i < m_robotTypeCb->count(); ++i)
            changed = m_robotTypeCb->itemText(i).toStdString() != m.robotTypes[i];
        if (changed)
        {
            m_robotTypeCb->clear();
            for (const auto &type : m.robotTypes)
                m_robotTypeCb->addItem(QString::fromStdString(type));
        }
        if (m.state.configuration)
            m_robotTypeCb->setCurrentText(QString::fromStdString(m.state.configuration->id));
    }
    const auto values = robot3d::domain::jointValues(m.state.joints);
    for (int i = 0; i < 6; ++i)
    {
        QSignalBlocker b(m_jointSlider[i]);
        m_jointSlider[i]->setValue(int(values[i]));
        m_jointLabel[i]->setText(QString::number(values[i], 'f', 1));
    }
    const auto &p = m.state.tcp;
    m_poseLabel->setText(
        QString::fromUtf8("当前位姿 (baseInWorld 坐标系, mm/deg)\nX = %1\nY = %2\nZ = %3\nRx = %4\nRy = %5\nRz = %6")
            .arg(p.X, 0, 'f', 2)
            .arg(p.Y, 0, 'f', 2)
            .arg(p.Z, 0, 'f', 2)
            .arg(p.Rx, 0, 'f', 3)
            .arg(p.Ry, 0, 'f', 3)
            .arg(p.Rz, 0, 'f', 3));
    m_speedLabel->setText(QString::fromUtf8("TCP 速度: %1 mm/s").arg(m.state.simulatedTcpSpeed, 0, 'f', 1));
    {
        QSignalBlocker b(m_simCb);
        m_simCb->setChecked(m.simulationRunning);
    }
    {
        QSignalBlocker b(m_coordSysCb);
        m_coordSysCb->setChecked(m.options.coordinatePanel);
    }
    {
        QSignalBlocker b(m_waypointCb);
        m_waypointCb->setChecked(m.options.waypoints);
    }
    {
        QSignalBlocker b(m_pathCb);
        m_pathCb->setChecked(m.options.trajectory);
    }
    {
        QSignalBlocker b(m_toolCb);
        m_toolCb->setChecked(m.options.tool);
    }
    {
        QSignalBlocker b(m_coordSysSelectCb);
        bool changed = m_coordSysSelectCb->count() != int(m.state.coordinates.size());
        for (int i = 0; !changed && i < m_coordSysSelectCb->count(); ++i)
            changed = m_coordSysSelectCb->itemData(i).toString().toStdString() != m.state.coordinates[i].id;
        if (changed)
        {
            m_coordSysSelectCb->clear();
            for (const auto &cs : m.state.coordinates)
                m_coordSysSelectCb->addItem(QString::fromUtf8(cs.name.c_str()) + " (" + QString::fromStdString(cs.id) +
                                                ")",
                                            QString::fromStdString(cs.id));
        }
        m_coordSysSelectCb->setCurrentIndex(m_coordSysSelectCb->findData(QString::fromStdString(m.state.coordinateId)));
    }
    m_viewport->showState(m);
    m_updatingSliders = false;
    layout()->activate();
}
void Robot3dDemoView::submitScene(const robot3d::scene::RobotSceneSnapshot &s)
{
    auto scene = s;
    if (workpiecePanel_) workpiecePanel_->decorateScene(scene);
    m_viewport->submitScene(scene);
}
void Robot3dDemoView::OnJointSliderChanged()
{
    if (m_updatingSliders || !composition_)
        return;
    std::array<double, 6> values;
    for (int i = 0; i < 6; ++i)
        values[i] = m_jointSlider[i]->value();
    composition_->presenter().onJointPoseRequested(robot3d::domain::jointPose(values));
}
void Robot3dDemoView::OnResetJoint()
{
    if (composition_)
    {
        robot3d::domain::JointPoseDeg zero = {};
        composition_->presenter().onJointPoseRequested(zero);
        appendLog("关节已归零，正解位姿已刷新");
    }
}
void Robot3dDemoView::OnRobotTypeChanged(int index)
{
    if (index >= 0 && composition_)
        composition_->presenter().onRobotTypeRequested(m_robotTypeCb->itemText(index).toStdString());
}
void Robot3dDemoView::OnCoordSysChanged(int index)
{
    if (index >= 0 && composition_)
        composition_->presenter().onCoordinateRequested(m_coordSysSelectCb->itemData(index).toString().toStdString());
}
void Robot3dDemoView::OnSimToggled(bool checked)
{
    if (composition_)
        composition_->presenter().onSimulationRequested(checked);
}
void Robot3dDemoView::OnDisplayFlagChanged()
{
    if (!composition_)
        return;
    auto options = model_.options;
    options.coordinatePanel = m_coordSysCb->isChecked();
    options.waypoints = m_waypointCb->isChecked();
    options.trajectory = m_pathCb->isChecked();
    options.tool = m_toolCb->isChecked();
    options.waypointPanel = options.waypoints || options.trajectory;
    composition_->presenter().onDisplayOptionsRequested(options);
}
void Robot3dDemoView::OnFramePresented(qulonglong revision)
{
    if (revision == model_.state.revision)
        OnCheckDump();
}
void Robot3dDemoView::OnCheckDump()
{
    const auto destination = qgetenv("DEMO_DUMP_FRAME");
    if (destination.isEmpty() || m_dumpShotDone || m_dumpInProgress || !model_.state.configuration)
        return;
    QScopedValueRollback<bool> exporting(m_dumpInProgress, true);
    // Complete label layout and viewport resize events before exporting an offline comparison.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if (dumpStage_ == 0)
    {
        dumpStage_ = 1;
        const auto start = qgetenv("DEMO_DUMP_START");
        if (!start.isEmpty() && !m_viewport->capture().save(QString::fromLocal8Bit(start)))
        {
            showError("初始帧保存失败");
            m_dumpShotDone = true;
            qApp->exit(2);
            return;
        }
        bool ok = true;
        const auto text = qgetenv("DEMO_DUMP_SIMSTEPS");
        int steps = text.isEmpty() ? 0 : text.toInt(&ok);
        if (!ok || steps < 0 || steps > 10000)
        {
            showError("非法导帧模拟步数");
            m_dumpShotDone = true;
            qApp->exit(2);
            return;
        }
        if (steps > 0)
        {
            composition_->presenter().advanceSimulation(steps);
            return;
        }
    }
    m_dumpShotDone = true;
    const auto image = m_viewport->capture();
    if (image.isNull() || !image.save(QString::fromLocal8Bit(destination)))
    {
        showError("最终帧保存失败");
        qApp->exit(2);
        return;
    }
    QJsonObject state;
    state["robotType"] = QString::fromStdString(model_.state.configuration->id);
    state["coordinateId"] = QString::fromStdString(model_.state.coordinateId);
    state["renderer"] = m_viewport->backend();
    QJsonArray joints;
    for (double v : robot3d::domain::jointValues(model_.state.joints))
        joints.append(v);
    state["jointsDeg"] = joints;
    const auto &p = model_.state.tcp;
    QJsonArray tcp;
    for (double v : {p.X, p.Y, p.Z, p.Rx, p.Ry, p.Rz})
        tcp.append(v);
    state["tcpMmEulerDeg"] = tcp;
    QFile json(QString::fromLocal8Bit(destination) + ".json");
    if (!json.open(QIODevice::WriteOnly) || json.write(QJsonDocument(state).toJson()) < 0)
    {
        showError("状态快照保存失败");
        qApp->exit(2);
        return;
    }
    qInfo() << "MVP_FRAME" << QString::fromLocal8Bit(destination)
            << QJsonDocument(state).toJson(QJsonDocument::Compact);
    QTimer::singleShot(0, qApp,
                       []
                       {
                           qApp->quit();
                       });
}
QWidget *Robot3dDemoView::BuildControlPanel()
{
    QWidget *panel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setMargin(4);
    auto *pointCloudButton = new QPushButton(QString::fromUtf8("点云查看器…"), panel);
    pointCloudButton->setObjectName("pointCloudButton");
    pointCloudButton->setMinimumHeight(32);
#ifdef ROBOT3D_WITH_VTK
    connect(pointCloudButton, &QPushButton::clicked, this,
            [this]
            {
                if (!pointCloudDialog_)
                    pointCloudDialog_ = new PointCloudViewerDialog(this);
                pointCloudDialog_->show();
                pointCloudDialog_->raise();
                pointCloudDialog_->activateWindow();
            });
#else
    pointCloudButton->setEnabled(false);
    pointCloudButton->setToolTip(QString::fromUtf8("点云查看器需要启用 VTK，请使用 -WithVTK 构建并运行。"));
#endif
    layout->addWidget(pointCloudButton);

    // ------------------------------ 机型 ------------------------------
    QGroupBox *renderBox = new QGroupBox(QString::fromUtf8("渲染后端 / 效果对比"), panel);
    QFormLayout *renderForm = new QFormLayout(renderBox);
    m_backendCb = new QComboBox(renderBox);
    m_backendCb->setObjectName("renderBackendCombo");
    m_backendCb->addItem("OpenGL", "opengl");
    if (RobotViewport::vtkAvailable())
        m_backendCb->addItem("VTK", "vtk");
    m_backendCb->setCurrentIndex(m_backendCb->findData(m_viewport->backend()));
    renderForm->addRow(QString::fromUtf8("后端:"), m_backendCb);
    layout->addWidget(renderBox);
    QGroupBox *typeBox = new QGroupBox(QString::fromUtf8("机型 / 模型"), panel);
    QFormLayout *typeForm = new QFormLayout(typeBox);
    m_robotTypeCb = new QComboBox(typeBox);
    m_robotTypeCb->setObjectName("robotTypeCombo");
    for (unsigned i = 0; i < sizeof(kRobotTypes) / sizeof(kRobotTypes[0]); ++i)
    {
        m_robotTypeCb->addItem(QString(kRobotTypes[i]));
    }
    m_robotTypeCb->setCurrentIndex(0);
    connect(m_robotTypeCb, SIGNAL(currentIndexChanged(int)), this, SLOT(OnRobotTypeChanged(int)));
    typeForm->addRow(QString::fromUtf8("机型:"), m_robotTypeCb);
    layout->addWidget(typeBox);

    // ------------------------------ 关节 ------------------------------
    QGroupBox *jointBox = new QGroupBox(QString::fromUtf8("关节角度 (度)"), panel);
    QVBoxLayout *jointLayout = new QVBoxLayout(jointBox);
    for (int i = 0; i < 6; ++i)
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *nameLabel = new QLabel(QStringLiteral("J%1").arg(i + 1), jointBox);
        nameLabel->setFixedWidth(24);

        m_jointSlider[i] = new QSlider(Qt::Horizontal, jointBox);
        m_jointSlider[i]->setObjectName(QString("jointSlider%1").arg(i + 1));
        m_jointSlider[i]->setRange(-int(robot3d::domain::demoJointLimit(i)), int(robot3d::domain::demoJointLimit(i)));
        m_jointSlider[i]->setValue(0);

        m_jointLabel[i] = new QLabel(QStringLiteral("0.0"), jointBox);
        m_jointLabel[i]->setFixedWidth(48);
        m_jointLabel[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        row->addWidget(nameLabel);
        row->addWidget(m_jointSlider[i], 1);
        row->addWidget(m_jointLabel[i]);
        jointLayout->addLayout(row);

        connect(m_jointSlider[i], SIGNAL(valueChanged(int)), this, SLOT(OnJointSliderChanged()));
    }
    QPushButton *resetPb = new QPushButton(QString::fromUtf8("关节归零"), jointBox);
    connect(resetPb, SIGNAL(clicked()), this, SLOT(OnResetJoint()));
    jointLayout->addWidget(resetPb);
    layout->addWidget(jointBox);

    // ---------------------------- 运动模拟 ----------------------------
    QGroupBox *simBox = new QGroupBox(QString::fromUtf8("运动模拟 / 位姿"), panel);
    QVBoxLayout *simLayout = new QVBoxLayout(simBox);
    m_simCb = new QCheckBox(QString::fromUtf8("启动正弦摆动模拟"), simBox);
    m_simCb->setObjectName("simulationCheck");
    simLayout->addWidget(m_simCb);

    m_poseLabel = new QLabel(simBox);
    m_poseLabel->setWordWrap(true);
    m_poseLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_poseLabel->setStyleSheet("QLabel{background:#f5f5f5;border:1px solid #e0e0e0;padding:4px;}");
    simLayout->addWidget(m_poseLabel);

    m_speedLabel = new QLabel(simBox);
    m_speedLabel->setStyleSheet("QLabel{background:#f5f5f5;border:1px solid #e0e0e0;padding:4px;}");
    simLayout->addWidget(m_speedLabel);
    layout->addWidget(simBox);

    // ---------------------------- 显示项 ------------------------------
    QGroupBox *dispBox = new QGroupBox(QString::fromUtf8("显示项"), panel);
    QVBoxLayout *dispLayout = new QVBoxLayout(dispBox);

    m_coordSysCb = new QCheckBox(QString::fromUtf8("坐标系"), dispBox);
    m_coordSysCb->setChecked(true);
    m_waypointCb = new QCheckBox(QString::fromUtf8("路点"), dispBox);
    m_pathCb = new QCheckBox(QString::fromUtf8("路点轨迹"), dispBox);
    m_pathCb->setObjectName("trajectoryCheck");
    m_toolCb = new QCheckBox(QString::fromUtf8("末端工具"), dispBox);
    dispLayout->addWidget(m_coordSysCb);
    dispLayout->addWidget(m_waypointCb);
    dispLayout->addWidget(m_pathCb);
    dispLayout->addWidget(m_toolCb);
    connect(m_coordSysCb, SIGNAL(clicked()), this, SLOT(OnDisplayFlagChanged()));
    connect(m_waypointCb, SIGNAL(clicked()), this, SLOT(OnDisplayFlagChanged()));
    connect(m_pathCb, SIGNAL(clicked()), this, SLOT(OnDisplayFlagChanged()));
    connect(m_toolCb, SIGNAL(clicked()), this, SLOT(OnDisplayFlagChanged()));

    m_coordSysSelectCb = new QComboBox(dispBox);
    m_coordSysSelectCb->setObjectName("coordinateCombo");
    m_coordSysSelectCb->addItem(QString::fromUtf8("基坐标系 (b_0)"), QString("b_0"));
    m_coordSysSelectCb->addItem(QString::fromUtf8("工具坐标系 (t_0)"), QString("t_0"));
    m_coordSysSelectCb->addItem(QString::fromUtf8("用户坐标系1 (p_0)"), QString("p_0"));
    m_coordSysSelectCb->addItem(QString::fromUtf8("工件坐标系1 (wcs_0)"), QString("wcs_0"));
    connect(m_coordSysSelectCb, SIGNAL(currentIndexChanged(int)), this, SLOT(OnCoordSysChanged(int)));
    dispLayout->addWidget(new QLabel(QString::fromUtf8("Jog 坐标系:"), dispBox));
    dispLayout->addWidget(m_coordSysSelectCb);
    layout->addWidget(dispBox);

    layout->addStretch(1);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("sceneControlTabs");
    auto *robotScroll = new QScrollArea(tabs);
    robotScroll->setWidgetResizable(true);
    robotScroll->setFrameShape(QFrame::NoFrame);
    robotScroll->setWidget(panel);
    tabs->addTab(robotScroll, QString::fromUtf8("机械臂"));
    workpiecePanel_ = new WorkpiecePanel(m_viewport, tabs);
    tabs->addTab(workpiecePanel_, QString::fromUtf8("工件"));
    connect(workpiecePanel_, &WorkpiecePanel::sceneChanged, this, [this] {
        m_viewport->submitScene(workpiecePanel_->currentScene());
    });
    connect(workpiecePanel_, &WorkpiecePanel::closeReady, this, [this] { close(); });
    return tabs;
}

void Robot3dDemoView::closeEvent(QCloseEvent *event)
{
    if (workpiecePanel_ && !workpiecePanel_->mayClose()) { event->ignore(); return; }
    QWidget::closeEvent(event);
}
