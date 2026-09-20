#include "demoui.h"

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

#include "3dmodel/robot3dcontrolwidget.h"
#include "3dmodel/robot3dglwidget.h"

namespace {

const char *kRobotTypes[] = {"G6L-3-20", "G6-1-1", "X20-1-3"};

/** 关节软限位（演示用，单位：度） */
const double kJointLimit[ROB_AXIS_NUM][2] = {
    {-360.0, 360.0},
    {-360.0, 360.0},
    {-165.0, 165.0},
    {-360.0, 360.0},
    {-360.0, 360.0},
    {-360.0, 360.0}
};

} // namespace

DemoUi::DemoUi(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QStringLiteral("CGX-TP 机械臂 3D 显示模块 —— 移植验证 Demo"));

    // ------------------------------------------------------------------
    // 1. 关键：必须先准备好数据源（机型名 + DH 参数），再让 3D 控件去加载模型。
    //    RobotGeometry 构造函数中就会调用 ChangeRobot3D() 读取
    //    robotType/<机型>/model3d/*.obj 与 stdRobotWholeDH。
    // ------------------------------------------------------------------
    DemoDataHub *hub = DemoDataHub::GetInstance();
    hub->LoadRobotType(kRobotTypes[0], QStringLiteral("robotType"));

    // 构造默认的用户坐标系 / 工件坐标系，供"坐标系"下拉框演示
    RobotConfig *cfg = RobotConfig::GetInstance();
    {
        CoordSysMsg cs;
        cs.coordSysID = "p_0";
        cs.coordSysName = QStringLiteral("用户坐标系1");
        cs.coordSysPose = {400, 0, 200, 0, 0, 0};
        cs.coordSysValid = true;
        cfg->coordSysList.append(cs);

        WcsMsg wcs;
        wcs.wcsID = "wcs_0";
        wcs.wcsName = QStringLiteral("工件坐标系1");
        wcs.root2Ref = {200, 300, 0, 0, 0, 0};
        cfg->wcsList.append(wcs);
    }

    // ------------------------------------------------------------------
    // 2. 取出原项目的 3D 控件（全局单例，与示教器中用法完全一致）
    // ------------------------------------------------------------------
    Robot3dControlWidget::InitInstance(this);
    Robot3DShowTypes showType = static_cast<Robot3DShowTypes>(
                Robot3DShowType::show_CoordSys
                | Robot3DShowType::show_RobotOperate
                | Robot3DShowType::show_Zoom
                | Robot3DShowType::show_WaypointOperate
                | Robot3DShowType::show_TcpSpeed);
    m_glWidget = Robot3dControlWidget::GetInstance()->GetRobot3dGLWidget(showType, true);

    // ------------------------------------------------------------------
    // 3. 布局
    // ------------------------------------------------------------------
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_glWidget);
    splitter->addWidget(BuildControlPanel());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 900 << 360);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setMargin(6);
    mainLayout->addWidget(splitter, 1);

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(110);
    mainLayout->addWidget(m_logEdit);

    // ------------------------------------------------------------------
    // 4. 模拟定时器
    // ------------------------------------------------------------------
    m_simTimer = new QTimer(this);
    m_simTimer->setInterval(40);   // 25 FPS
    connect(m_simTimer, SIGNAL(timeout()), this, SLOT(OnSimTimer()));
    connect(m_simCb, SIGNAL(toggled(bool)), this, SLOT(OnSimToggled(bool)));

    AddLog(QStringLiteral("=== 机械臂 3D 显示模块移植 Demo ==="));
    AddLog(QStringLiteral("模型路径: %1").arg(QFileInfo("robotType/" + QString(kRobotTypes[0])).absoluteFilePath()));
    AddLog(QStringLiteral("已载入机型 %1，请用右侧面板操作（鼠标左键拖拽=旋转，切到平移模式=移动）")
           .arg(kRobotTypes[0]));

    // 5. 延迟到窗口显示、OpenGL 初始化完成后再灌入第一批位置数据
    QTimer::singleShot(120, this, SLOT(OnResetJoint()));
    QTimer::singleShot(900, this, SLOT(OnCheckDump()));
}

DemoUi::~DemoUi()
{
}

QWidget *DemoUi::BuildControlPanel()
{
    QWidget *panel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setMargin(4);

    // ------------------------------ 机型 ------------------------------
    QGroupBox *typeBox = new QGroupBox(QStringLiteral("机型 / 模型"), panel);
    QFormLayout *typeForm = new QFormLayout(typeBox);
    m_robotTypeCb = new QComboBox(typeBox);
    for (unsigned i = 0; i < sizeof(kRobotTypes) / sizeof(kRobotTypes[0]); ++i) {
        m_robotTypeCb->addItem(QString(kRobotTypes[i]));
    }
    m_robotTypeCb->setCurrentIndex(0);
    connect(m_robotTypeCb, SIGNAL(currentIndexChanged(int)), this, SLOT(OnRobotTypeChanged(int)));
    typeForm->addRow(QStringLiteral("机型:"), m_robotTypeCb);
    layout->addWidget(typeBox);

    // ------------------------------ 关节 ------------------------------
    QGroupBox *jointBox = new QGroupBox(QStringLiteral("关节角度 (度)"), panel);
    QVBoxLayout *jointLayout = new QVBoxLayout(jointBox);
    for (int i = 0; i < ROB_AXIS_NUM; ++i) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *nameLabel = new QLabel(QStringLiteral("J%1").arg(i + 1), jointBox);
        nameLabel->setFixedWidth(24);

        m_jointSlider[i] = new QSlider(Qt::Horizontal, jointBox);
        m_jointSlider[i]->setRange(static_cast<int>(kJointLimit[i][0]), static_cast<int>(kJointLimit[i][1]));
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
    QPushButton *resetPb = new QPushButton(QStringLiteral("关节归零"), jointBox);
    connect(resetPb, SIGNAL(clicked()), this, SLOT(OnResetJoint()));
    jointLayout->addWidget(resetPb);
    layout->addWidget(jointBox);

    // ---------------------------- 运动模拟 ----------------------------
    QGroupBox *simBox = new QGroupBox(QStringLiteral("运动模拟 / 位姿"), panel);
    QVBoxLayout *simLayout = new QVBoxLayout(simBox);
    m_simCb = new QCheckBox(QStringLiteral("启动正弦摆动模拟"), simBox);
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
    QGroupBox *dispBox = new QGroupBox(QStringLiteral("显示项 (SetShowType 位标志)"), panel);
    QVBoxLayout *dispLayout = new QVBoxLayout(dispBox);

    m_coordSysCb = new QCheckBox(QStringLiteral("坐标系"), dispBox);
    m_coordSysCb->setChecked(true);
    m_waypointCb = new QCheckBox(QStringLiteral("路点"), dispBox);
    m_pathCb = new QCheckBox(QStringLiteral("路点轨迹"), dispBox);
    m_toolCb = new QCheckBox(QStringLiteral("末端工具"), dispBox);
    dispLayout->addWidget(m_coordSysCb);
    dispLayout->addWidget(m_waypointCb);
    dispLayout->addWidget(m_pathCb);
    dispLayout->addWidget(m_toolCb);
    connect(m_coordSysCb, SIGNAL(clicked()), this, SLOT(OnDisplayFlagChanged()));
    connect(m_waypointCb, SIGNAL(clicked()), this, SLOT(OnDisplayFlagChanged()));
    connect(m_pathCb, SIGNAL(clicked()), this, SLOT(OnDisplayFlagChanged()));
    connect(m_toolCb, SIGNAL(clicked()), this, SLOT(OnDisplayFlagChanged()));

    m_coordSysSelectCb = new QComboBox(dispBox);
    m_coordSysSelectCb->addItem(QStringLiteral("基坐标系 (b_0)"), QString("b_0"));
    m_coordSysSelectCb->addItem(QStringLiteral("工具坐标系 (t_0)"), QString("t_0"));
    m_coordSysSelectCb->addItem(QStringLiteral("用户坐标系1 (p_0)"), QString("p_0"));
    m_coordSysSelectCb->addItem(QStringLiteral("工件坐标系1 (wcs_0)"), QString("wcs_0"));
    connect(m_coordSysSelectCb, SIGNAL(currentIndexChanged(int)), this, SLOT(OnCoordSysChanged(int)));
    dispLayout->addWidget(new QLabel(QStringLiteral("Jog 坐标系:"), dispBox));
    dispLayout->addWidget(m_coordSysSelectCb);
    layout->addWidget(dispBox);

    layout->addStretch(1);
    return panel;
}

void DemoUi::AddLog(const QString &msg)
{
    if (m_logEdit == nullptr) return;
    m_logEdit->appendPlainText(QString("[%1] %2")
                               .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"))
                               .arg(msg));
}

void DemoUi::RefreshPoseLabels()
{
    DemoDataHub *hub = DemoDataHub::GetInstance();
    const Pose &p = hub->currentPose;

    m_poseLabel->setText(QStringLiteral("当前位姿 (baseInWorld 坐标系, mm/deg)\n"
                                        "X = %1\nY = %2\nZ = %3\nRx = %4\nRy = %5\nRz = %6")
                         .arg(p.X, 0, 'f', 2).arg(p.Y, 0, 'f', 2).arg(p.Z, 0, 'f', 2)
                         .arg(p.Rx, 0, 'f', 3).arg(p.Ry, 0, 'f', 3).arg(p.Rz, 0, 'f', 3));
    m_speedLabel->setText(QStringLiteral("TCP 速度: %1 mm/s").arg(hub->curTcpSpeed, 0, 'f', 1));
}

void DemoUi::ApplyJointFromSliders()
{
    DemoDataHub *hub = DemoDataHub::GetInstance();
    JointPose jp;
    jp.L1 = m_jointSlider[0]->value();
    jp.L2 = m_jointSlider[1]->value();
    jp.L3 = m_jointSlider[2]->value();
    jp.L4 = m_jointSlider[3]->value();
    jp.L5 = m_jointSlider[4]->value();
    jp.L6 = m_jointSlider[5]->value();

    hub->ApplyJointPose(jp);

    // 灌给 3D 控件（与原示教器中 UpdateRobot3dJointAngle/ToolPosition 用法一致）
    Robot3dControlWidget::GetInstance()->UpdateRobot3dJointAngle(hub->currentJointPose);
    Robot3dControlWidget::GetInstance()->UpdateRobot3dToolPosition(hub->currentPose);

    RefreshPoseLabels();
}

void DemoUi::OnJointSliderChanged()
{
    if (m_updatingSliders) return;
    for (int i = 0; i < ROB_AXIS_NUM; ++i) {
        m_jointLabel[i]->setText(QString::number(m_jointSlider[i]->value(), 'f', 1));
    }
    ApplyJointFromSliders();
}

void DemoUi::OnResetJoint()
{
    m_updatingSliders = true;
    for (int i = 0; i < ROB_AXIS_NUM; ++i) {
        m_jointSlider[i]->setValue(0);
        m_jointLabel[i]->setText(QStringLiteral("0.0"));
    }
    m_updatingSliders = false;
    ApplyJointFromSliders();
    AddLog(QStringLiteral("关节已归零，正解位姿已刷新"));
}

void DemoUi::OnSimTimer()
{
    DemoDataHub *hub = DemoDataHub::GetInstance();
    hub->StepSimulation(m_simTimer->interval() / 1000.0);
    hub->curTcpSpeed = 120.0 + 100.0 * sin(QDateTime::currentMSecsSinceEpoch() / 500.0);

    // 实时速度标签（原控件槽函数）
    hub->UpdateTCPspeed_signal(hub->curTcpSpeed);

    m_updatingSliders = true;
    const double vals[ROB_AXIS_NUM] = {hub->currentJointPose.L1, hub->currentJointPose.L2,
                                       hub->currentJointPose.L3, hub->currentJointPose.L4,
                                       hub->currentJointPose.L5, hub->currentJointPose.L6};
    for (int i = 0; i < ROB_AXIS_NUM; ++i) {
        m_jointSlider[i]->setValue(static_cast<int>(vals[i]));
        m_jointLabel[i]->setText(QString::number(vals[i], 'f', 1));
    }
    m_updatingSliders = false;

    Robot3dControlWidget::GetInstance()->UpdateRobot3dJointAngle(hub->currentJointPose);
    Robot3dControlWidget::GetInstance()->UpdateRobot3dToolPosition(hub->currentPose);
    RefreshPoseLabels();
}

void DemoUi::OnSimToggled(bool checked)
{
    if (checked) {
        m_simTimer->start();
        AddLog(QStringLiteral("开始运动模拟"));
    } else {
        m_simTimer->stop();
        AddLog(QStringLiteral("停止运动模拟"));
    }
}

void DemoUi::OnCoordSysChanged(int index)
{
    if (index < 0) return;
    const QString id = m_coordSysSelectCb->itemData(index).toString();
    DemoDataHub::GetInstance()->curJogPara.coordSysID = id;

    // 直接复用 3D 控件内部的坐标系切换槽（其 UI 下拉框对象名为 selCoordSysCb）
    QComboBox *innerCb = m_glWidget->findChild<QComboBox *>("selCoordSysCb");
    if (innerCb != nullptr) {
        innerCb->blockSignals(true);
        int idx = innerCb->findData(id);
        if (idx >= 0) {
            innerCb->setCurrentIndex(idx);
        }
        innerCb->blockSignals(false);
    }
    m_glWidget->UpdateCoordSysChange_slot(index);
    AddLog(QStringLiteral("切换 Jog 坐标系: %1").arg(id));
}

void DemoUi::OnRobotTypeChanged(int index)
{
    if (index < 0) return;
    const QString typeName = m_robotTypeCb->itemText(index);

    DemoDataHub *hub = DemoDataHub::GetInstance();
    if (!hub->LoadRobotType(typeName, QStringLiteral("robotType"))) {
        AddLog(QStringLiteral("载入机型失败: %1（请确认 robotType/%1 目录存在）").arg(typeName));
        return;
    }

    // 与原示教器一致：机型变化后重建 3D 模型
    m_glWidget->ChangeRobotType_slot();

    hub->ApplyJointPose(hub->currentJointPose);
    Robot3dControlWidget::GetInstance()->UpdateRobot3dJointAngle(hub->currentJointPose);
    Robot3dControlWidget::GetInstance()->UpdateRobot3dToolPosition(hub->currentPose);
    Robot3dControlWidget::GetInstance()->ResizeRobot3dMode();
    RefreshPoseLabels();
    AddLog(QStringLiteral("切换机型: %1").arg(typeName));
}

void DemoUi::OnDisplayFlagChanged()
{
    Robot3DShowTypes showType = static_cast<Robot3DShowTypes>(Robot3DShowType::show_RobotOperate
                                                             | Robot3DShowType::show_Zoom
                                                             | Robot3DShowType::show_TcpSpeed);
    if (m_coordSysCb->isChecked())  showType |= Robot3DShowType::show_CoordSys;
    if (m_waypointCb->isChecked() || m_pathCb->isChecked())
        showType |= Robot3DShowType::show_WaypointOperate;
    if (m_toolCb->isChecked())      showType |= Robot3DShowType::show_Tool;

    m_glWidget->SetShowType(showType);

    // 路点 / 轨迹显示的开关由 SetShowType 关联到内部按钮勾选状态，
    // 这里直接设置标志位以便演示
    m_glWidget->waypointShowSignal     = m_waypointCb->isChecked();
    m_glWidget->waypointPathShowSignal = m_pathCb->isChecked();
    m_glWidget->toolShowSignal         = m_toolCb->isChecked();
    m_glWidget->update();

    AddLog(QStringLiteral("显示项更新: 坐标系=%1 路点=%2 轨迹=%3 工具=%4")
           .arg(m_coordSysCb->isChecked()).arg(m_waypointCb->isChecked())
           .arg(m_pathCb->isChecked()).arg(m_toolCb->isChecked()));
}

// 无人值守验证：设置环境变量 DEMO_DUMP_FRAME=<文件名> 时，
// 在 OpenGL 初始化并灌入一帧数据后导出帧缓冲并退出。
//   DEMO_DUMP_FRAME     导出文件名
//   DEMO_DUMP_SIMSTEPS  导出前推进的模拟步数（验证关节联动）
//   DEMO_DUMP_START     先把初始位姿存一张，便于与模拟后对比
// 用法（cmd）:  set DEMO_DUMP_FRAME=frame.png && set DEMO_DUMP_SIMSTEPS=90 && robot3ddemo.exe
void DemoUi::OnCheckDump()
{
    const QByteArray dump = qgetenv("DEMO_DUMP_FRAME");
    if (dump.isEmpty() || m_dumpShotDone || m_glWidget == nullptr) return;
    m_dumpShotDone = true;

    // 1) 初始位姿存盘
    const QByteArray startName = qgetenv("DEMO_DUMP_START");
    if (!startName.isEmpty()) {
        const QImage startImg = m_glWidget->grabFramebuffer();
        if (!startImg.isNull()) {
            startImg.save(QString::fromLocal8Bit(startName));
            AddLog(QStringLiteral("已导出初始位姿帧: %1").arg(QString::fromLocal8Bit(startName)));
        }
    }

    // 2) 推进运动模拟，验证关节联动与正解
    const QByteArray stepsStr = qgetenv("DEMO_DUMP_SIMSTEPS");
    const int steps = stepsStr.isEmpty() ? 0 : QString::fromLatin1(stepsStr).toInt();
    if (steps > 0) {
        DemoDataHub *hub = DemoDataHub::GetInstance();
        for (int i = 0; i < steps; ++i) {
            hub->StepSimulation(0.04);
        }
        Robot3dControlWidget::GetInstance()->UpdateRobot3dJointAngle(hub->currentJointPose);
        Robot3dControlWidget::GetInstance()->UpdateRobot3dToolPosition(hub->currentPose);
        AddLog(QStringLiteral("已推进 %1 步模拟，关节角: %2, %3, %4, %5, %6, %7")
               .arg(steps)
               .arg(hub->currentJointPose.L1, 0, 'f', 2).arg(hub->currentJointPose.L2, 0, 'f', 2)
               .arg(hub->currentJointPose.L3, 0, 'f', 2).arg(hub->currentJointPose.L4, 0, 'f', 2)
               .arg(hub->currentJointPose.L5, 0, 'f', 2).arg(hub->currentJointPose.L6, 0, 'f', 2));
        AddLog(QStringLiteral("正解 TCP: X=%1 Y=%2 Z=%3")
               .arg(hub->currentPose.X, 0, 'f', 2)
               .arg(hub->currentPose.Y, 0, 'f', 2)
               .arg(hub->currentPose.Z, 0, 'f', 2));
        m_glWidget->update();
    }

    // 3) 导出结果帧
    const QImage img = m_glWidget->grabFramebuffer();
    if (!img.isNull()) {
        const bool ok = img.save(QString::fromLocal8Bit(dump));
        AddLog(QStringLiteral("已导出 3D 帧缓冲: %1 (成功=%2, %3x%4)")
               .arg(QString::fromLocal8Bit(dump)).arg(ok).arg(img.width()).arg(img.height()));
    } else {
        AddLog(QStringLiteral("导出 3D 帧缓冲失败: grabFramebuffer 返回空图像"));
    }
    QTimer::singleShot(300, qApp, SLOT(quit()));
}
