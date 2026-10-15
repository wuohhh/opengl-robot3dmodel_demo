/**
 * @file robot3dglwidget.cpp
 * @brief 机器人三维显示控件的实现：GL 绘制、视角交互、叠加面板与各数据槽
 */
#include "robot3dglwidget.h"
#include "demo_support.h"

/**
 * @brief 是否显示末端工件（0-不显示，1-显示）
 * @note  全局变量，RobotGeometry::drawGeometry 读取该值决定是否绘制 tool 模型；
 *        由 Robot3dGLWidget::showEndTool_slot() 翻转。
 */
int showEndToolStatus = 0; //是否显示末端工件，0-不显示，1-显示，默认0

/**
 * @brief 构造控件：初始化视角状态、创建叠加面板并连接各模块信号
 * @param parent 父窗口
 */
Robot3dGLWidget::Robot3dGLWidget(QWidget *parent) :  QOpenGLWidget(parent)
{
    this->setObjectName("robot3dGLWidget");
    curRobot3DShowData.scalePara=1;
    curRobot3DShowData.ViewChangeMatrix.setToIdentity();
    curRobot3DShowData.rotate_center=QVector3D(0,0,0);
    curRobot3DShowData.translate_model=QVector3D(0,0,0);
    curRobot3DShowData.rotation_model=QQuaternion(1,0,0,0);

    preRobot3DShowData.scalePara=1;
    preRobot3DShowData.ViewChangeMatrix.setToIdentity();
    preRobot3DShowData.rotate_center=QVector3D(0,0,0);
    preRobot3DShowData.translate_model=QVector3D(0,0,0);
    preRobot3DShowData.rotation_model=QQuaternion(1,0,0,0);

    InitUI();

    QObject::connect(RobotConfig::GetInstance(), SIGNAL(CoordSysChange_signal()), this, SLOT(UpdateCoordSysCombo()));
    //QObject::connect(RobotConfig::GetInstance(), SIGNAL(CoordSysValChange_signal()), this, SLOT(UpdateCoordSysVal()));
    QObject::connect(CommDataController::GetInstance(), SIGNAL(UpdateCurrentTcp_signal()), this, SLOT(UpdateCurrentTcp_slot()));
    QObject::connect(CommDataController::GetInstance(), SIGNAL(UpdateTCPspeed_signal(double)), this, SLOT(UpdateTCPspeed_slot(double)));
    QObject::connect(CommDataController::GetInstance(), SIGNAL(updateUseCoordSysSignal()), this, SLOT(updateUseCoordSysSlot()));

    QObject::connect(SoftwareUI_Controller::GetInstance(), SIGNAL(ChangeRobotType_signal()), this, SLOT(ChangeRobotType_slot()));

    QObject::connect(this, SIGNAL(widgetSizeChanged(int, int)), this, SLOT(resizeCustomedWidget(int, int)));
   // QObject::connect(CommDataController::GetInstance(), SIGNAL(JogCoordSysID_signal()), this, SLOT(JogCoordSysID_slot()));
}

/**
 * @brief 析构：删除 GL 资源时保证 OpenGL 上下文处于当前状态
 */
Robot3dGLWidget::~Robot3dGLWidget()
{
    // Make sure the context is current when deleting the texture and the buffers.
    makeCurrent();
    doneCurrent();
}

/**
 * @brief 创建叠加在 3D 视口之上的操作面板并完成布局
 *
 * 布局自上而下分三层：
 *   顶部：坐标系选择（下拉框 + 当前 TCP 名称）与路点操作条；
 *   中部：机器人操作按钮（置初始位 / 平移旋转切换 / 末端工件显隐）与缩放控件；
 *   底部：TCP 速度显示与位姿编辑的“确定 / 取消”按钮。
 */
void Robot3dGLWidget::InitUI()
{
    //放大缩小功能
    QStringList zoomNames;
    zoomNames << "zoomin" << "zoomout";

    // 缩放功能：zoomin / zoomout 按钮 + 垂直滑条
    zoomWidget = new QWidget(this);
    zoomWidget->setObjectName("zoomWidget");
    QVBoxLayout* zoomLayout = new QVBoxLayout();
    zoomLayout->setSpacing(10);
    zoomWidget->setLayout(zoomLayout);

//    zoomWidget->setProperty("helpPrompt","showHelpPrompt");
//    zoomWidget->setProperty("helpPromptExplain",tr("图形缩放"));
//    zoomWidget->setProperty("helpPromptExplainWidgetObjName","graphicsZoomExplainWidget");
//    zoomWidget->setProperty("helpPromptExplainPos",0);
//    zoomWidget->setProperty("helpPromptExplainFlag",0);
//    zoomWidget->setProperty("helpPromptExplainDirect",1);
//    zoomWidget->setProperty("helpPromptExplainArrowDirect",3);
//    zoomWidget->setProperty("helpPromptExplainMargin","0#80#0#80");

    zoomLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Fixed,QSizePolicy::Expanding));
    for(int index = 0; index < zoomNames.count(); index++)
    {
        QPushButton* zoomPB = new QPushButton(this);
        zoomPB->setObjectName(zoomNames.at(index));
        zoomPB->setStyleSheet(QString("QPushButton{border:none;background-color: #ffffff;qproperty-icon: url(:/resource/image/%1.png); qproperty-iconSize: 16px 16px;}"
                                      "QPushButton:pressed{border:1px solid #32b67a;}").arg(zoomNames.at(index)));
        // 长按不松开时自动重复发送 clicked 信号，便于连续缩放
        zoomPB->setAutoRepeat(true);
        zoomPB->setAutoRepeatDelay(100); //按下按钮不松开，500ms后开始自动发送信号
        zoomPB->setAutoRepeatInterval(100); //每隔100ms发送一次点击信号
        QObject::connect(zoomPB, SIGNAL(clicked()), this, SLOT(zoomChange_slot()));
        zoomLayout->addWidget(zoomPB);

        if(index == 0)
        {
            zoomSlider = new QSlider(Qt::Vertical, this);
            zoomSlider->setRange(0,100);
            zoomSlider->setPageStep(1);
            zoomSlider->setTickInterval(10);  // 设置刻度间隔
            zoomSlider->setTickPosition(QSlider::TicksBelow);  //刻度在上方
            // 默认值 40 → scalePara = 0.4 + 4*0.15 = 1.0（原始比例）
            zoomSlider->setValue(40);
            connect(zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(ZoomSlider_valueChanged(int)));
            zoomLayout->addWidget(zoomSlider);
        }
    }
    zoomLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Fixed,QSizePolicy::Expanding));

    //坐标系选择
    // 坐标系选择：下拉框 + 当前 TCP 名称
    coordSysWidget = new QWidget(this);
    QHBoxLayout* coordSysLayout = new QHBoxLayout();
    coordSysLayout->setSpacing(10);
    coordSysWidget->setLayout(coordSysLayout);

    QLabel* selCoordSysLabel = new QLabel(tr("坐标系:"),this);
    selCoordSysLabel->setStyleSheet("width:50px;");
    selCoordSysCb = new QComboBox(this);
    // 下拉项较多时显示滚动条
    selCoordSysCb->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);//设置滚动条
    selCoordSysCb->setStyleSheet("QComboBox{width:110px;}");
    connect(selCoordSysCb,SIGNAL(currentIndexChanged(int)),this,SLOT(UpdateCoordSysChange_slot(int)));
    QStyledItemDelegate *styledItemDelegate = new QStyledItemDelegate();//为设置下拉框item高度生效
    selCoordSysCb ->setItemDelegate(styledItemDelegate);

    QLabel* curTCPTitleLabel = new QLabel("TCP:",this);
    curTCPTitleLabel->setStyleSheet("width:50px;");
    curTCPNameLabel = new QLabel(this);
    curTCPNameLabel->setStyleSheet("QLabel{min-width:120px;border: 1px solid #efefef;}");

    coordSysLayout->addWidget(selCoordSysLabel);
    coordSysLayout->addWidget(selCoordSysCb);
    coordSysLayout->addItem(new QSpacerItem(30,1,QSizePolicy::Fixed,QSizePolicy::Fixed));
    coordSysLayout->addWidget(curTCPTitleLabel);
    coordSysLayout->addWidget(curTCPNameLabel);

    //
    // TCP 速度显示
    tcpSpeedWidget = new QWidget(this);
    QHBoxLayout* tcpSpeedLayout = new QHBoxLayout();
    tcpSpeedLayout->setContentsMargins(0,0,0,0);
    tcpSpeedWidget->setLayout(tcpSpeedLayout);

    curTcpSpeedLabel = new QLabel(QString("0.000 mm/s"),this);
    curTcpSpeedLabel->setStyleSheet("max-height:36px;min-height:36px");
    curTcpSpeedLabel->setAlignment(Qt::AlignCenter);

    tcpSpeedLayout->addWidget( new QLabel(tr("TCP速度:"),this));
    tcpSpeedLayout->addWidget(curTcpSpeedLabel);
    tcpSpeedLayout->addStretch(0);

    //
    // 位姿编辑的“确定 / 取消”（确定时回调发起编辑的按钮）
    poseOperateWidget = new QWidget(this);
    QHBoxLayout* poseOperateLayout = new QHBoxLayout();
    poseOperateWidget->setLayout(poseOperateLayout);

    QPushButton* btn_ok = new QPushButton(tr("确定"),this);
    btn_ok->setStyleSheet("QPushButton{width:60px;}");
    btn_ok->setObjectName("ok");
    QObject::connect(btn_ok, SIGNAL(clicked()), this, SLOT(EditRobotPoseBtn_slot()));

    QPushButton* btn_cancel = new QPushButton(tr("取消"), this);
    btn_cancel->setStyleSheet("QPushButton{width:65px;border-color: #e82828;background-color: #e82828;}"
                              "QPushButton:pressed{background-color: #9E0F0F;}");
    btn_cancel->setObjectName("cancel");
    QObject::connect(btn_cancel, SIGNAL(clicked()), this, SLOT(EditRobotPoseBtn_slot()));

    poseOperateLayout->setMargin(0);
    poseOperateLayout->addStretch(0);
    poseOperateLayout->addWidget(btn_ok);
    poseOperateLayout->addWidget(btn_cancel);

    //
    // 机器人操作按钮：置初始位 / 平移旋转切换 / 末端工件显隐
    robotOperateWidget = new QWidget(this);
    QVBoxLayout *robotOperateLayout = new QVBoxLayout();
    robotOperateLayout->setContentsMargins(5,5,0,5);
    robotOperateLayout->setSpacing(10);
    robotOperateWidget->setLayout(robotOperateLayout);

    QPushButton* recovery3dPb =new QPushButton(QIcon(":/resource/image/recovery3d.png"),"",this);
    recovery3dPb->setObjectName("recovery3dPb");
    recovery3dPb->setIconSize(QSize(36,36));
    recovery3dPb->setFixedSize(QSize(40,40));
    recovery3dPb->setStyleSheet("QPushButton{border:none;background:transparent;}"
                                  "QPushButton:pressed{border:1px solid #32b67a;}");
    QObject::connect(recovery3dPb, SIGNAL(clicked()), this, SLOT(Recovery3D_slot()));

//    recovery3dPb->setProperty("helpPrompt","showHelpPrompt");
//    recovery3dPb->setProperty("helpPromptExplain",tr("置初始位"));
//    recovery3dPb->setProperty("helpPromptExplainWidgetObjName","recovery3dExplainWidget");
//    recovery3dPb->setProperty("helpPromptExplainPos",2);
//    recovery3dPb->setProperty("helpPromptExplainFlag",0);
//    recovery3dPb->setProperty("helpPromptExplainDirect",0);
//    recovery3dPb->setProperty("helpPromptExplainArrowDirect",2);
//    recovery3dPb->setProperty("helpPromptExplainMargin","0#0#0#0");

    // 初始为旋转模式
    this->rotateTranslateStatus = 0;
     QPushButton* rotateTranslatePb =new QPushButton(QIcon(":/resource/image/R_T_change.png"),"",this);
     rotateTranslatePb->setObjectName("rotateTranslatePb");
     rotateTranslatePb->setIconSize(QSize(36,36));
     rotateTranslatePb->setFixedSize(QSize(40,40));
     rotateTranslatePb->setStyleSheet("QPushButton{border:none;background:transparent;}"
                                   "QPushButton:pressed{border:1px solid #32b67a;}");
     QObject::connect(rotateTranslatePb, SIGNAL(clicked()), this, SLOT(RotateTranslatChange_slot()));

    // 初始不显示末端工件，按钮图标为 end_tool_hide
    showEndToolStatus = 0;
    QPushButton *showEndToolPb = new QPushButton(QIcon(":/resource/image/end_tool_hide.png"),"",this);
    showEndToolPb->setObjectName("showEndToolPb");
    showEndToolPb->setIconSize(QSize(36, 36));
    showEndToolPb->setFixedSize(QSize(40, 40));
    showEndToolPb->setStyleSheet("QPushButton{border:none;background:transparent;}"
                                 "QPushButton:pressed{border:1px solid #32b67a;}");

    QObject::connect(showEndToolPb, SIGNAL(clicked()), this, SLOT(showEndTool_slot()));

//     rotateTranslatePb->setProperty("helpPrompt","showHelpPrompt");
//     rotateTranslatePb->setProperty("helpPromptExplain",tr("平移旋转"));
//     rotateTranslatePb->setProperty("helpPromptExplainWidgetObjName","R_T_changeExplainWidget");
//     rotateTranslatePb->setProperty("helpPromptExplainPos",2);
//     rotateTranslatePb->setProperty("helpPromptExplainFlag",0);
//     rotateTranslatePb->setProperty("helpPromptExplainDirect",0);
//     rotateTranslatePb->setProperty("helpPromptExplainArrowDirect",2);
//     rotateTranslatePb->setProperty("helpPromptExplainMargin","0#0#0#0");

     robotOperateLayout->addWidget(recovery3dPb);
     robotOperateLayout->addWidget(rotateTranslatePb);
     robotOperateLayout->addWidget(showEndToolPb);
     robotOperateLayout->addStretch(0);

     //
     // 路点操作条：5 个可勾选按钮（前 3 个当前隐藏）
     waypointOperateWidget = new QWidget(this);
     QHBoxLayout *waypointOperateLayout = new QHBoxLayout();
     waypointOperateLayout->setSpacing(10);
     waypointOperateWidget->setLayout(waypointOperateLayout);

     // 按钮图标名：machineBase / targetPose / currentPose / waypointPose / pathCurve
     QStringList waypointOpStr;
     waypointOpStr<<"machineBase"<<"targetPose"<<"currentPose"<<"waypointPose"<<"pathCurve";

     for(int index = 0; index < waypointOpStr.count(); index++)
     {
         QPushButton* opPb = new QPushButton(this);
         opPb->setObjectName(waypointOpStr.at(index));
         opPb->setStyleSheet(QString("QPushButton{width:36px;height:36px;}"
                                     "QPushButton:checked{border-image: url(:/resource/image/%1_checked.png);}"
                                     "QPushButton:!checked{border-image: url(:/resource/image/%1_unchecked.png);}").arg(waypointOpStr.at(index)));
         opPb->setCheckable(true);

         if(index == 0)
             opPb->setChecked(false);
         else
             opPb->setChecked(true);

         QObject::connect(opPb, SIGNAL(clicked()), this, SLOT(WaypointOperateChange_slot()));
         waypointOperateLayout->addWidget(opPb);
         waypointOpPb.append(opPb);

         if(index<3)
             opPb->hide();//临时注释

//         if(waypointOpStr.at(index) == "waypointPose")
//         {
//             opPb->setProperty("helpPrompt","showHelpPrompt");
//             opPb->setProperty("helpPromptExplain",tr("路点显示"));
//             opPb->setProperty("helpPromptExplainWidgetObjName","wayPointShowExplainWidget");
//             opPb->setProperty("helpPromptExplainPos",0);
//             opPb->setProperty("helpPromptExplainFlag",0);
//             opPb->setProperty("helpPromptExplainDirect",0);
//             opPb->setProperty("helpPromptExplainArrowDirect",3);
//             opPb->setProperty("helpPromptExplainMargin","0#0#0#0");
//         }
//         else if(waypointOpStr.at(index) == "pathCurve")
//         {
//             opPb->setProperty("helpPrompt","showHelpPrompt");
//             opPb->setProperty("helpPromptExplain",tr("轨迹显示"));
//             opPb->setProperty("helpPromptExplainWidgetObjName","pathCurveShowExplainWidget");
//             opPb->setProperty("helpPromptExplainPos",3);
//             opPb->setProperty("helpPromptExplainFlag",0);
//             opPb->setProperty("helpPromptExplainDirect",0);
//             opPb->setProperty("helpPromptExplainArrowDirect",0);
//             opPb->setProperty("helpPromptExplainMargin","0#0#0#0");
//         }
     }

     // 顶部：坐标系面板 + 路点操作条
     QHBoxLayout* topLayout = new QHBoxLayout();
     topLayout->addWidget(coordSysWidget);
     topLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding,QSizePolicy::Fixed));
     topLayout->addWidget(waypointOperateWidget);

     // 中部：机器人操作按钮（左）与缩放控件（右）
     QHBoxLayout* midLayout = new QHBoxLayout();
     midLayout->setSpacing(0);
     midLayout->addWidget(robotOperateWidget);
     midLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding,QSizePolicy::Expanding));
     midLayout->addWidget(zoomWidget);

     // 底部：TCP 速度与位姿编辑按钮
     QHBoxLayout* bottomLayout = new QHBoxLayout();
     bottomLayout->addItem(new QSpacerItem(30,1,QSizePolicy::Fixed,QSizePolicy::Fixed));
     bottomLayout->addWidget(tcpSpeedWidget);
     bottomLayout->addWidget(poseOperateWidget);
     bottomLayout->addItem(new QSpacerItem(10,1,QSizePolicy::Fixed,QSizePolicy::Fixed));

     // 三段式纵向总布局；上述子控件浮在 GL 视口之上
     QVBoxLayout* mainLayout = new QVBoxLayout(this);
     mainLayout->setMargin(0);
     mainLayout->addLayout(topLayout );
     mainLayout->addLayout(midLayout );
     mainLayout->addLayout(bottomLayout );
     mainLayout->addItem(new QSpacerItem(1,5,QSizePolicy::Fixed,QSizePolicy::Fixed));
     this->setLayout(mainLayout);
}

/**
 * @brief 按位标志设置显示项与显示模式
 * @param showType 由 Robot3DShowType 按位组合而成
 *
 * 处理流程：
 *   1. 解析当前 Jog 坐标系位姿（t_0 取当前位姿；wcs_* 由世界坐标经基坐标变换；
 *      其余取配置中的坐标系位姿），生成坐标系变换矩阵并刷新三条坐标轴线；
 *   2. 记录当前显示模式到 curShowType；
 *   3. 按位控制各面板的显隐与各三维要素的绘制开关；
 *   4. show_Install 时进入世界安装视角模式。
 */
void Robot3dGLWidget::SetShowType(Robot3DShowTypes showType)
{    
    //坐标系切换
    // 解析 Jog 坐标系位姿
    Pose jogCoordSysPose ={0,0,0,0,0,0};
    if(CommDataController::GetInstance()->curJogPara.coordSysID == "t_0")
    {
         CopyPose(CommDataController::GetInstance()->currentPose, &jogCoordSysPose);
    }
    else
    {
        if(CommDataController::GetInstance()->curJogPara.coordSysID.contains("wcs_"))
        {
            Pose workInWorld = RobotConfig::GetInstance()->getWorkSysInWorldPose(CommDataController::GetInstance()->curJogPara.coordSysID);
            Pose worldInBase;
            cr_PoseInv(worldInBase,CommDataController::GetInstance()->baseInWorldPose);
            cr_PoseTrans(jogCoordSysPose,worldInBase,workInWorld);
        }
        else
        {
            jogCoordSysPose = RobotConfig::GetInstance()->GetCoordSysPose(CommDataController::GetInstance()->curJogPara.coordSysID);
        }
    }
    // 生成坐标系变换阵并刷新坐标轴线
    QMatrix4x4 tmpMatrix = this->ViewChangeMatrixFunction(jogCoordSysPose);

    //
    this->curShowType = showType;

    // show_Null：隐藏全部面板并关闭所有三维要素
    if(showType == Robot3DShowType::show_Null)
    {
        coordSysWidget->hide();
        robotOperateWidget->hide();
        zoomWidget->hide();
        poseOperateWidget->hide();
        waypointOperateWidget->hide();
        tcpSpeedWidget->hide();
        secondRobotShowSignal = false;
        waypointShowSignal = false;
        waypointPathShowSignal = false;
        safePlaneShowSignal = false;
        toolShowSignal = false;
        this->IsIntoWorldTransformWidget(false);
        return;
    }

    if(showType & Robot3DShowType::show_CoordSys)
    {
        coordSysWidget->show();
    }
    else
    {
        coordSysWidget->hide();
    }

    if(showType & Robot3DShowType::show_RobotOperate)
    {
        robotOperateWidget->show();
    }
    else
    {
        robotOperateWidget->hide();
    }

    if(showType & Robot3DShowType::show_Zoom)
    {
        zoomWidget->show();
        zoomSlider->setVisible(true);
    }
    else
    {
        zoomWidget->hide();
    }

    if(showType & Robot3DShowType::show_PoseOperate)
    {
        poseOperateWidget->show();
    }
    else
    {
        poseOperateWidget->hide();
    }

    if(showType & Robot3DShowType::show_WaypointOperate)
    {
        waypointOperateWidget->show();
        // 路点 / 轨迹的显示状态取自操作条按钮的勾选状态
        waypointShowSignal = this->waypointOpPb.at(3)->isChecked();
        waypointPathShowSignal = this->waypointOpPb.at(4)->isChecked();
    }
    else
    {
        waypointOperateWidget->hide();
        waypointShowSignal = false;
        waypointPathShowSignal = false;
    }

    if(showType & Robot3DShowType::show_TargetRobot)
    {
        secondRobotShowSignal = true;
    }
    else
    {
        secondRobotShowSignal = false;
    }

    if(showType & Robot3DShowType::show_SafePlane)
    {
        safePlaneShowSignal = true;
    }
    else
    {
        safePlaneShowSignal = false;
    }

    if(showType & Robot3DShowType::show_TcpSpeed)
    {
        tcpSpeedWidget->show();
    }
    else
    {
        tcpSpeedWidget->hide();
    }

    if(showType & Robot3DShowType::show_Install)
    {
        this->IsIntoWorldTransformWidget(true);
    }
    else
    {
        this->IsIntoWorldTransformWidget(false);
    }

    if(showType & Robot3DShowType::show_Tool)
    {
        toolShowSignal = true;
    }
    else
    {
        toolShowSignal = false;
    }
}

/**
 * @brief 位姿编辑“确定 / 取消”按钮的槽（两者共用，通过 objectName 区分）
 *
 * 确定：按发起编辑的按钮类型（editPushButton）回填位姿——
 *       CGXiWaypointSetPushButton 发出 CurPoseChanged_signal；
 *       exjSetWaypointPb 调用 SetPose_ExjMsg；ExJointFixSetWaypointPb 调用 SetPose_ExJointMsg；
 *       其余调用 SetPose_JointAngleMsg。
 * 最后清空 editPushButton 指针并发出 EndEditPose_signals。
 */
void Robot3dGLWidget::EditRobotPoseBtn_slot()
{
    QPushButton* curPB = qobject_cast<QPushButton*>(sender());
    CommDataController::GetInstance()->UserOperateLogPrint(UserOperateType::click,curPB->text(),"");
    if(curPB->objectName() == "ok")
    {
        if(this->editPushButton != NULL)
        {
            if(this->editPushButton->objectName() == "CGXiWaypointSetPushButton")
            {
                emit CurPoseChanged_signal(this->editPushButton,CommDataController::GetInstance()->currentJointPose, CommDataController::GetInstance()->currentPose);
            }
            else if(this->editPushButton->objectName() == "exjSetWaypointPb")
            {
                this->editPushButton->SetPose_ExjMsg(CommDataController::GetInstance()->currentJointPose,
                                                     CommDataController::GetInstance()->currentPose,CommDataController::GetInstance()->currentEXJointPose);
            }
            else if(this->editPushButton->objectName() =="ExJointFixSetWaypointPb")
            {
                this->editPushButton->SetPose_ExJointMsg(CommDataController::GetInstance()->currentEXJointPose);
            }
            else
            {
                this->editPushButton->SetPose_JointAngleMsg(CommDataController::GetInstance()->currentJointPose, CommDataController::GetInstance()->currentPose);
            }
        }
    }
    this->editPushButton = NULL;
    emit EndEditPose_signals();
}

/**
 * @brief 缩放按钮槽：按滑条的 pageStep 增减数值，再由滑条变化驱动 scalePara
 */
void Robot3dGLWidget::zoomChange_slot()
{
    QPushButton* tmpPb = qobject_cast<QPushButton*>(sender());
    if(tmpPb->objectName() == "zoomout")
    {
        zoomSlider->setValue(zoomSlider->value() - zoomSlider->pageStep());
    }
    else
    {
        zoomSlider->setValue(zoomSlider->value() + zoomSlider->pageStep());
    }
}

/**
 * @brief 缩放滑条：0~100 线性映射为 scalePara 0.4~1.9（40 对应 1.0）并请求重绘
 */
void Robot3dGLWidget::ZoomSlider_valueChanged(int value)
{
    curRobot3DShowData.scalePara = 0.4+(double)value/10.0*0.15;
    this->update();
}

/**
 * @brief 鼠标按下：记录起点位置并进入拖拽状态
 */
void Robot3dGLWidget::mousePressEvent(QMouseEvent *e)
{
    // Save mouse press position    
    curPos = QVector2D(e->localPos());
    mousePressSignal = true;
}

/**
 * @brief 鼠标抬起：结束拖拽状态
 */
void Robot3dGLWidget::mouseReleaseEvent(QMouseEvent *e)
{
    // Mouse release position - mouse press position
//    QVector2D diff = QVector2D(e->localPos()) - mousePressPosition;

//    // Rotation axis is perpendicular to the mouse position difference
//    // vector
//    QVector3D n = QVector3D(diff.y(), diff.x(), 0.0).normalized();

//    // Accelerate angular speed relative to the length of the mouse sweep
//    qreal acc = diff.length() / 100.0;

//    // Calculate new rotation axis as weighted sum
//    rotationAxis = (rotationAxis * angularSpeed + n * acc).normalized();

//    // Increase angular speed
//    angularSpeed += acc;
    mousePressSignal = false;
}

/**
 * @brief 鼠标移动：按 rotateTranslateStatus 对模型做增量旋转或平移
 *
 * 旋转：轴取 (0, diffY, diffX)（经当前坐标系变换到模型空间），角度为位移长度 /5 度，
 *       四元数左乘累积；
 * 平移：屏幕 X → 模型 Y，屏幕 Y → 模型 -Z（机器人坐标系 Z 轴向上）。
 */
void Robot3dGLWidget::mouseMoveEvent(QMouseEvent *e)
{
        QVector2D diff = QVector2D(e->localPos()) - curPos;
      curPos = QVector2D(e->localPos());

    // 旋转模式
    if(this->rotateTranslateStatus == 0)
    {
//        QVector2D diff = QVector2D(e->localPos()) - mousePressPosition;
        double rotateAngle = diff.length() / 5;

        QVector3D  rotationAxis= QVector3D( 0.0,diff.y(), diff.x()).normalized();
        QVector4D RotateViewExchange=(curRobot3DShowData.ViewChangeMatrix)*QVector4D(rotationAxis,0);
        curRobot3DShowData.rotation_model = QQuaternion::fromAxisAndAngle(QVector3D(RotateViewExchange), rotateAngle) * curRobot3DShowData.rotation_model;
        //rotation_light = QQuaternion::fromAxisAndAngle(QVector3D(RotateViewExchange), -rotateAngle) * rotation_light;
    }
    // 平移模式
    else if(this->rotateTranslateStatus == 1)
    {
        curRobot3DShowData.translate_model = QVector3D(0,diff.x()/5,-diff.y()/5)+curRobot3DShowData.translate_model;
    }

    mousePressSignal = true;
    update();
}

/**
 * @brief 初始化 GL 状态与全部几何对象
 *
 * 依次创建：机器人本体与第二机器人（零位姿态 + 相机观察参数）、坐标系三条轴线、
 * TCP 球、路点球与轨迹线、安全平面、末端工具实体与工具坐标系。
 */
void Robot3dGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    GetOpenGLVersion();

    // 白色背景；逆时针为正面、背面剔除、开启深度测试
    glClearColor(1, 1, 1, 1);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST); 
    //glEnable(GL_CULL_FACE);//旋转面模型时，某些位姿看不到

    robotGeometry = new RobotGeometry();
    double array[6] = {0,0,0,0,0,0};
    // 先按零位姿态生成各关节顶点
    robotGeometry->multjointTransform(array);
    // 取第 3 关节中心作为整机旋转中心（须在 multjointTransform 之后调用）
    robotGeometry->set_rotate_center(curRobot3DShowData.rotate_center); //该函数需放在multjointTransform之后
    // 由模型包围盒计算相机观察点与位置
    robotGeometry->CalRobotViewTargetPos();
    robotGeometry->initShaders();

    secondRobotGeometry = new RobotGeometry();
    //double array2[6] = {0,90,90,-90,0,0};   //第二个机器人关节变换，以示区别
    secondRobotGeometry->multjointTransform(array);
    secondRobotGeometry->initShaders();
    secondRobotShowSignal = false;

    if (robotGeometry->numTool == 0)
    {
        // robotOperateWidget->findChild<QPushButton *>("showEndToolPb")->setVisible(false);
    }

    // 世界坐标系：三条轴线 X 红 / Y 绿 / Z 蓝，长度 400mm
    for(int index = 0; index < 3; index++)
    {
        LineMsg coordsysLineMsg;
        coordsysLineMsg.startPt = QVector3D(0,0,0);
        coordsysLineMsg.endPt.setX(index==0 ? 400: 0);
        coordsysLineMsg.endPt.setY(index==1 ? 400: 0);
        coordsysLineMsg.endPt.setZ(index==2 ? 400: 0);

        if(index == 0)
            coordsysLineMsg.color.setNamedColor("#b63232");
        else if(index == 1)
            coordsysLineMsg.color.setNamedColor("#32b67a");
        else
            coordsysLineMsg.color.setNamedColor("#3255b6");

        LineGeometry* lineGeometry = new LineGeometry(coordsysLineMsg);
         lineGeometry->initShaders();
         coordSysGeometry.append(lineGeometry);
    }

    // TCP 位置球：球心取自零位正解位姿，半径 10
    //double tmpJoint_pos[ROB_AXIS_NUM] = {0};

    // 零位正解：确定 TCP 球与工具实体的初始位置
    Pose tmpTargetPose;
    JointPose tmpJoint_pos = {0,0,0,0,0,0};
    cr_ForwardKineFull(tmpJoint_pos, tmpTargetPose, CommDataController::GetInstance()->stdRobotWholeDH);

    double tmpPose[ROB_AXIS_NUM] = {0};
    CopyPoseMsg(tmpTargetPose,tmpPose);

    // [demo 适配] 模型顶点已按 G_SCALE 缩放，而位姿为 mm，故球心需同步缩放，
    // 否则 TCP 球会偏离机器人本体（原项目此处存在单位不一致）
    SphereMsg TCP_sphereMsg = {QVector3D(tmpPose[0], tmpPose[1], tmpPose[2]) * G_SCALE, 10, QColor("#3255b6")};
    TCP_Geometry = new SphereGeometry(TCP_sphereMsg);
    TCP_Geometry->initShaders();

    // 路点轨迹折线（点列由 AddRobotWaypointPath 追加）
    QVector<QVector3D> tmpWaypoints;
    waypointPathlineGeometry = new LineGeometry(tmpWaypoints, QColor("#3255b6"));
    waypointPathlineGeometry->initShaders();

    // 路点球（批量绘制，最多 500 个）
    QVector<SphereMsg> sphereMsgs;
    waypointSphereGeometry = new SphereGeometry(sphereMsgs,500);
    waypointSphereGeometry->initShaders();

    // 安全平面：按边界平面数量预创建，初始透明度 0
    for(int planeIndex=0; planeIndex<CR6_SAFETY_LIMITS_BOUNDARY_PLANE_NUM; planeIndex++)
    {
        PlaneMsg planeMsg;
        planeMsg.color = QColor("#32b67a");
        planeMsg.alpha = 0;
        PlaneGeometry* tmpPlaneGeometry = new PlaneGeometry(planeMsg);
        tmpPlaneGeometry->initShaders();
        safePlaneGeometrys.append(tmpPlaneGeometry);
    }

    // 末端工具实体（球 / 圆锥等，由外部数据驱动）
    SubstanceMsg substanceMsg = {0, {0,0,0,0,0,0}, 1.0f, 1.0f, 1.0f, 1.0f, QColor("#3255b6")};
    for(int index=0; index<ROB_AXIS_NUM; index++)
        substanceMsg.centerPose[index] = tmpPose[index];

    tool_Geometry = new SubstanceGeometry(substanceMsg);
    tool_Geometry->initShaders();
    // 工具坐标系：三条轴线，长度 120mm
    for(int index = 0; index < 3; index++)
    {
        LineMsg coordsysLineMsg;
        coordsysLineMsg.startPt = QVector3D(0,0,0);
        coordsysLineMsg.endPt.setX(index==0 ? 120: 0);
        coordsysLineMsg.endPt.setY(index==1 ? 120: 0);
        coordsysLineMsg.endPt.setZ(index==2 ? 120: 0);

        if(index == 0)
            coordsysLineMsg.color.setNamedColor("#b63232");
        else if(index == 1)
            coordsysLineMsg.color.setNamedColor("#32b67a");
        else
            coordsysLineMsg.color.setNamedColor("#3255b6");

        LineGeometry* lineGeometry = new LineGeometry(coordsysLineMsg);
        lineGeometry->initShaders();
        toolCoordSysGeometry.append(lineGeometry);
    }
}

/**
 * @brief 设置透视投影（垂直视角 45°、近平面 30、远平面 400），并通知叠加控件自适应尺寸
 */
void Robot3dGLWidget::resizeGL(int w, int h)
{
    // Calculate aspect ratio
    qreal aspect = qreal(w) / qreal(h ? h : 1);

    // Set near plane to 3.0, far plane to 7.0, field of view 45 degrees
    //const qreal zNear = 3.0, zFar = 7.0, fov = 45.0; //fov越大，看的物体越小
    const qreal zNear = 30, zFar = 400, fov = 45.0;
    // Reset projection
    projection.setToIdentity();

    // Set perspective projection
    projection.perspective(fov, aspect, zNear, zFar);

    // 通知叠加面板调整尺寸（本控件自身的槽 resizeCustomedWidget）
    emit widgetSizeChanged(w, h);
}

/**
 * @brief 视窗高度相对历史最大值缩小到 75% 以下时，缩小操作按钮的图标与尺寸
 */
void Robot3dGLWidget::resizeCustomedWidget(int w, int h)
{
    maxW = maxW < w ? w : maxW;
    maxH = maxH < h ? h : maxH;
    double r = ((double) h) / (maxH ? maxH : 1);
    // 小屏：按钮缩小为 30×30、图标 27×27
    if (r < 0.75)
    {
        for (QObject *o : robotOperateWidget->children())
        {
            QPushButton *pb = qobject_cast<QPushButton *>(o);
            if (pb)
            {
                pb->setIconSize(QSize(27, 27));
                pb->setFixedSize(QSize(30, 30));
            }
        }
    }
    else
    {
        for (QObject *o : robotOperateWidget->children())
        {
            QPushButton *pb = qobject_cast<QPushButton *>(o);
            if (pb)
            {
                pb->setIconSize(QSize(36, 36));
                pb->setFixedSize(QSize(40, 40));
            }
        }
    }
}

/**
 * @brief 绘制一帧：构造 model / view / projection 矩阵后依次绘制各类几何体
 *
 * model 矩阵的代码书写顺序为：缩放 → 坐标系逆变换 → 视图坐标系下的平移 →
 * 平移到旋转中心 → 绕旋转中心旋转 → 平移回原点（与作用于顶点的先后顺序相反）；
 * 相机位置与观察点由 RobotGeometry 依据模型包围盒给出；
 * 绘制顺序：机器人 → 第二机器人（可选）→ TCP 球 → 坐标轴线 →
 *           路点球（可选）→ 轨迹线（可选）→ 安全平面（可选）→ 工具（可选）。
 */
void Robot3dGLWidget::paintGL()
{
    // Clear color and depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //model矩阵先缩放再平移，代码实现时候顺序相反

    QMatrix4x4 model;
    model.setToIdentity();
    // 整体缩放
    model.scale(curRobot3DShowData.scalePara);
    // 叠加当前用户坐标系的逆变换
    model=(curRobot3DShowData.ViewChangeMatrix).inverted()*model;
    // 平移量在用户坐标系下表达，先变换到模型坐标系；除以 scalePara 以抵消缩放
    QVector4D TranslateViewExchange=(curRobot3DShowData.ViewChangeMatrix)*QVector4D(curRobot3DShowData.translate_model,0);
    model.translate(QVector3D(TranslateViewExchange)/curRobot3DShowData.scalePara);

    // 绕旋转中心旋转：先平移到中心，旋转后再平移回原处
    model.translate(curRobot3DShowData.rotate_center);
    // 安装模式下叠加世界安装角（绕 Y 轴）
    if(this->curShowType&Robot3DShowType::show_Install)
    {
        model.rotate(curRobot3DShowData.rotation_model*QQuaternion::fromEulerAngles(QVector3D(0,-RobotConfig::GetInstance()->worldTransform.tiltAngle,-RobotConfig::GetInstance()->worldTransform.baseAngle)));
    }
    else
    {
        model.rotate(curRobot3DShowData.rotation_model);
    }
    model.translate(-curRobot3DShowData.rotate_center);



    QMatrix4x4 view;
    // 机器人坐标系 Z 轴向上
    QVector3D Up(0,0,1);

    view.lookAt(robotGeometry->Pos,robotGeometry->Target,Up);

    // 模型-视图-投影矩阵
    QMatrix4x4 mvpMatrix = projection*view*model;


    //#ifdef ROCKCHIP_ARM
    //    mvpMatrix.setColumn(3,QVector4D(mvpMatrix.column(3).x(),mvpMatrix.column(3).y(),mvpMatrix.column(3).z(),200));
    //#endif

    QMatrix4x4 light_matrix;
    //light_matrix.rotate(rotation_light);

    // 机器人本体
    robotGeometry->drawGeometry( model, mvpMatrix,  light_matrix);

    // 第二台机器人（目标位姿）
    if(secondRobotShowSignal)
        secondRobotGeometry->drawSecondGeometry(model, mvpMatrix, light_matrix);

    // TCP 位置球
    TCP_Geometry->drawGeometry( model, mvpMatrix,  light_matrix);

    // 当前坐标系的三条轴线
    for(int index = 0; index < this->coordSysGeometry.count(); index++)
    {
        coordSysGeometry.at(index)->drawGeometry( model, mvpMatrix,  light_matrix);
    }

    // 路点球
    if(waypointShowSignal)
    {
        if(waypointSphereGeometry != NULL)
            waypointSphereGeometry->drawGeometry( model, mvpMatrix,  light_matrix);
    }

    // 路点轨迹线
    if(waypointPathShowSignal)
    {
        if(waypointPathlineGeometry != NULL)
        {
            waypointPathlineGeometry->drawGeometry( model, mvpMatrix,  light_matrix);
        }
    }

    // 安全平面
    if(safePlaneShowSignal)
    {
        for(int index=0; index<safePlaneGeometrys.count(); index++)
            safePlaneGeometrys.at(index)->drawGeometry(model, mvpMatrix, light_matrix);
    }

    // 末端工具坐标系与工具实体
    if(toolShowSignal && tool_Geometry != NULL)
    {
        for(int index = 0; index < this->toolCoordSysGeometry.count(); index++)
        {
            toolCoordSysGeometry.at(index)->drawGeometry( model, mvpMatrix,  light_matrix);
        }
        tool_Geometry->drawGeometry(model, mvpMatrix, light_matrix);
    }
}

/**
 * @brief 关节角刷新：转换为数组后驱动机器人本体各关节顶点重算并请求重绘
 */
void Robot3dGLWidget::SetRobotJointAngle_slot(JointPose jointPose)
{
    // JointPose 的六个关节角（度）转为数组，顺序 L1~L6
    double tmpJointAngle[6] = {0};
    tmpJointAngle[0] = jointPose.L1;
    tmpJointAngle[1] = jointPose.L2;
    tmpJointAngle[2] = jointPose.L3;
    tmpJointAngle[3] = jointPose.L4;
    tmpJointAngle[4] = jointPose.L5;
    tmpJointAngle[5] = jointPose.L6;

    this->robotGeometry->multjointTransform(tmpJointAngle);

//    if(this->mousePressSignal == true)
//    {
//        robotGeometry->set_rotate_center(this->rotate_center); //该函数需放在multjointTransform之后
//    }

    this->update();
}
/**
 * @brief 末端位姿刷新：更新 TCP 球位置；当前坐标系为工具坐标系时同步刷新坐标轴线
 */
void Robot3dGLWidget::SetRobotToolPosition_slot(Pose pose)
{
     // [demo 适配] 同上：位姿(mm) -> 模型坐标系
     TCP_Geometry->SetSphereData(QVector3D(pose.X, pose.Y, pose.Z) * G_SCALE, TCP_Geometry->sphereMsg.radius);

     //用户工作坐标系下，实时显示坐标系轴位姿
     if(CommDataController::GetInstance()->curJogPara.coordSysID == "t_0")
     {
          QMatrix4x4 tmpMatrix = this->ViewChangeMatrixFunction(pose);
     }
}
/**
 * @brief 末端工具数据刷新：更新工具实体数据，并按工具位姿重算工具坐标系三条轴线
 * @param substanceMsgsIn 工具实体描述（类型 / 中心位姿 / 尺寸 / 颜色）
 */
void Robot3dGLWidget::SetRobotSafetyToolData(QVector<SubstanceMsg> substanceMsgsIn)
{
    tool_Geometry->SetSubstanceData(substanceMsgsIn);
    // 取工具中心位姿用于计算轴线端点
    Pose pose;
    memcpy(&pose, substanceMsgsIn.at(0).centerPose, sizeof(double)*6);
    QMatrix4x4 RotateMatrix = this->AxisAngle2RotateMatrix(pose);

    // 轴线端点 = 工具中心 + 旋转矩阵作用后的轴向（长度 120mm）
    QVector4D Xdirection(120,0,0,0);
    QVector4D Ydirection(0,120,0,0);
    QVector4D Zdirection(0,0,120,0);

    if(toolCoordSysGeometry.count()==3)
    {
        toolCoordSysGeometry.at(0)->SetLineData(QVector3D(pose.X,pose.Y,pose.Z),QVector3D(RotateMatrix*Xdirection)+QVector3D(pose.X,pose.Y,pose.Z));
        toolCoordSysGeometry.at(1)->SetLineData(QVector3D(pose.X,pose.Y,pose.Z),QVector3D(RotateMatrix*Ydirection)+QVector3D(pose.X,pose.Y,pose.Z));
        toolCoordSysGeometry.at(2)->SetLineData(QVector3D(pose.X,pose.Y,pose.Z),QVector3D(RotateMatrix*Zdirection)+QVector3D(pose.X,pose.Y,pose.Z));
    }

}
/**
 * @brief 置初始位：安装模式下重放当前显示模式（回到安装视角）；
 *        否则清零平移 / 旋转，并把缩放恢复为 1.0（滑条回 40）
 */
void Robot3dGLWidget::Recovery3D_slot()
{
    if(this->curShowType & Robot3DShowType::show_Install)
    {
        this->SetShowType(this->curShowType);
    }
    else
    {
        //this->UpdateCoordSysChange_slot(this->selCoordSysCb->currentIndex());
        curRobot3DShowData.translate_model=QVector3D(0,0,0);
        curRobot3DShowData.rotation_model=QQuaternion(1,0,0,0);
        curRobot3DShowData.scalePara=1.0;
        zoomSlider->setValue(40);
    }
}
/**
 * @brief 鼠标拖拽行为切换：0-旋转（图标 R_T_change）↔ 1-平移（图标 T_R_change）
 */
void Robot3dGLWidget::RotateTranslatChange_slot()
{
    QPushButton* tmpPb = qobject_cast<QPushButton*>(sender());
    QString objName = tmpPb->objectName();

    if(this->rotateTranslateStatus == 0)
    {
        this->rotateTranslateStatus = 1;
        tmpPb->setIcon(QPixmap(QString(":/resource/image/%1.png").arg("T_R_change")));
    }
    else if(this->rotateTranslateStatus == 1)
    {
        this->rotateTranslateStatus = 0;
        tmpPb->setIcon(QPixmap(QString(":/resource/image/%1.png").arg("R_T_change")));
    }
}

/**
 * @brief 末端工件显隐切换：翻转全局 showEndToolStatus 并切换按钮图标
 * @note  showEndToolStatus 由 RobotGeometry::drawGeometry 读取，影响 tool 模型是否绘制
 */
void Robot3dGLWidget::showEndTool_slot()
{
    QPushButton *tmpPb = qobject_cast<QPushButton *>(sender());

    showEndToolStatus ^= 1;
    if (showEndToolStatus == 0)
    {
        tmpPb->setIcon(QPixmap(QString(":/resource/image/%1.png").arg("end_tool_hide")));
    }
    else
    {
        tmpPb->setIcon(QPixmap(QString(":/resource/image/%1.png").arg("end_tool_show")));
    }
}

/**
 * @brief 路点操作条按钮槽：按按钮 objectName 更新路点球 / 轨迹线的显示开关
 * @note  machineBase / targetPose / currentPose 三个按钮当前为空实现（预留）
 */
void Robot3dGLWidget::WaypointOperateChange_slot()
{
    QPushButton *tmpButton = qobject_cast<QPushButton*>(sender());
    QString waypointOpStr = tmpButton->objectName();

    if(waypointOpStr == "machineBase")
    {}
    else if(waypointOpStr == "targetPose")
    {}
    else if(waypointOpStr == "currentPose")
    {}
    else if(waypointOpStr == "waypointPose")
    {
        waypointShowSignal = tmpButton->isChecked();
    }
    else if(waypointOpStr == "pathCurve")
    {
        waypointPathShowSignal = tmpButton->isChecked();
    }
}

/**
 * @brief 回读“当前 TCP 所在坐标系”并同步界面与视角
 *
 * 依据 curTcpInCoordSysMsg 中的坐标系类型组装坐标系 ID
 * （b_0 / t_0 / p_i / l_i / f_i / wcs_i；工件坐标系需经世界→基坐标变换），
 * 随后同步下拉框选中项、重算坐标系变换矩阵，并回写 Jog 坐标系 ID。
 * 仅在 updateRealtimeRobot3dMsgSignal 为真（跟随实时数据）时执行。
 */
void Robot3dGLWidget::updateUseCoordSysSlot()
{
    // 未开启实时跟随则不处理
    if (!CommDataController::GetInstance()->updateRealtimeRobot3dMsgSignal)
        return ;
    CoordSysType tmpCoorSysType = CommDataController::GetInstance()->curTcpInCoordSysMsg.coordSysTcpPara.coordSysType;
    QString coordsysID = "b_0";
    Pose coordSysPose={0,0,0,0,0,0};
    // 坐标系类型 → 坐标系 ID，并取出对应坐标系位姿
    switch (tmpCoorSysType){
        case base:
             coordsysID="b_0";
             break;
        case joint:
        case vision:
        case path:
        case world:
             break;
        case point:
            coordsysID = QString("p_%1").arg(CommDataController::GetInstance()->curTcpInCoordSysMsg.coordSysTcpPara.coordinateId);
            for(int index = 0; index < RobotConfig::GetInstance()->coordSysList.count(); index++){
                if(coordsysID == RobotConfig::GetInstance()->coordSysList.at(index).coordSysID){
                    CopyPose(RobotConfig::GetInstance()->coordSysList.at(index).coordSysPose,&coordSysPose);
                }
            }
            break;
        case line:
            coordsysID = QString("l_%1").arg(CommDataController::GetInstance()->curTcpInCoordSysMsg.coordSysTcpPara.coordinateId);
            for(int index = 0; index < RobotConfig::GetInstance()->coordSysList.count(); index++){
                if(coordsysID == RobotConfig::GetInstance()->coordSysList.at(index).coordSysID){
                    CopyPose(RobotConfig::GetInstance()->coordSysList.at(index).coordSysPose,&coordSysPose);
                }
            }
            break;
        case plane:
            coordsysID = QString("f_%1").arg(CommDataController::GetInstance()->curTcpInCoordSysMsg.coordSysTcpPara.coordinateId);
            for(int index = 0; index < RobotConfig::GetInstance()->coordSysList.count(); index++){
                if(coordsysID == RobotConfig::GetInstance()->coordSysList.at(index).coordSysID){
                    CopyPose(RobotConfig::GetInstance()->coordSysList.at(index).coordSysPose,&coordSysPose);
                }
            }
            break;
        case tool:
            coordsysID="t_0";
            break;
        case work:
            coordsysID = QString("wcs_%1").arg(CommDataController::GetInstance()->curTcpInCoordSysMsg.coordSysTcpPara.coordinateId);
            for(int index = 0; index < RobotConfig::GetInstance()->wcsList.count(); index++){
                if(coordsysID == RobotConfig::GetInstance()->wcsList.at(index).wcsID){
                    Pose workInWorld = RobotConfig::GetInstance()->getWorkSysInWorldPose(coordsysID);
                    Pose worldInBase;
                    cr_PoseInv(worldInBase,CommDataController::GetInstance()->baseInWorldPose);
                    cr_PoseTrans(coordSysPose,worldInBase,workInWorld);
                }
            }
            break;
    }


    // 同步下拉框选中项（屏蔽信号，避免再次触发切换槽）
    int index = this->selCoordSysCb->findData(coordsysID);
    if (index != -1) {
        this->selCoordSysCb->blockSignals(true);
        this->selCoordSysCb->setCurrentIndex(index);
        this->selCoordSysCb->blockSignals(false);
    }

    // 更新视角变换，并回写 Jog 坐标系 ID
    curRobot3DShowData.ViewChangeMatrix = ViewChangeMatrixFunction(coordSysPose);
    CommDataController::GetInstance()->curJogPara.coordSysID = coordsysID;
    CommDataController::GetInstance()->curExjJogPara.coordSysID = coordsysID;
}

/**
 * @brief 刷新 TCP 名称：无编号 TCP 时按 TCP 偏置反查名称，否则按 tcp_<ID> 查名
 */
void Robot3dGLWidget::UpdateCurrentTcp_slot()
{
    //curTCPNameLabel->setText(RobotConfig::GetInstance()->GetTCPnameFromID(RobotConfig::GetInstance()->tcpMsg.ActiveTCP_ID));
    // 无编号 TCP：按偏置匹配名称
    if(CommDataController::GetInstance()->curJogPara.tcpID<0)
    {
        curTCPNameLabel->setText(RobotConfig::GetInstance()->GetTCPnameFromPose(CommDataController::GetInstance()->curJogPara.tcpOffset));
    }
    else
    {
        curTCPNameLabel->setText(RobotConfig::GetInstance()->GetTCPnameFromID(QString("tcp_%1").arg(CommDataController::GetInstance()->curJogPara.tcpID)));
    }
}
/**
 * @brief 重建坐标系下拉框：有效用户坐标系 + 全部工件坐标系，并按 ID 还原原选中项
 */
void Robot3dGLWidget::UpdateCoordSysCombo()
{
    // 记录原选中项 ID，重建后按 ID 还原
    QString tmpVarID = this->selCoordSysCb->currentData().toString();

    this->selCoordSysCb->blockSignals(true);
    this->selCoordSysCb->clear();

    int selIndex = -1;
    for(int index = 0; index < RobotConfig::GetInstance()->coordSysList.count(); index++)
    {
        // 仅列出有效的用户坐标系
        if(RobotConfig::GetInstance()->coordSysList.at(index).coordSysValid)
        {
            this->selCoordSysCb->addItem(RobotConfig::GetInstance()->coordSysList.at(index).coordSysName, RobotConfig::GetInstance()->coordSysList.at(index).coordSysID);

            if(tmpVarID == RobotConfig::GetInstance()->coordSysList.at(index).coordSysID)
            {
                selIndex = index;
            }
        }
    }

    // 工件坐标系（始终列出）
    for(int index = 0; index < RobotConfig::GetInstance()->wcsList.count(); index++)
    {
        this->selCoordSysCb->addItem(RobotConfig::GetInstance()->wcsList.at(index).wcsName, RobotConfig::GetInstance()->wcsList.at(index).wcsID);
        if(tmpVarID == RobotConfig::GetInstance()->wcsList.at(index).wcsID)
        {
            selIndex = index;
        }
    }

    // 还原原选中项；找不到时回到第一项
    if(selIndex>=0)
    {
        this->selCoordSysCb->setCurrentIndex(selIndex);
    }
    else
    {
        this->selCoordSysCb->setCurrentIndex(0);
        //UpdateCoordSysChange_slot(0);
    }

    this->selCoordSysCb->blockSignals(false);
}
/**
 * @brief 坐标系数值变化后重新执行一次坐标系切换处理
 * @note  置 coordsysValueChangeSignal，供区分“数值变化”与“用户切换”
 */
void Robot3dGLWidget::UpdateCoordSysVal()
{
    this->coordsysValueChangeSignal = true;
    UpdateCoordSysChange_slot(this->selCoordSysCb->currentIndex());
}
//void Robot3dGLWidget::JogCoordSysID_slot()
//{
//    QString selCoordSysID = this->selCoordSysCb->currentData().toString();
//    if(selCoordSysID != CommDataController::GetInstance()->curJogCoordSysID)
//    {
//        this->selCoordSysCb->setCurrentIndex(this->selCoordSysCb->findData(CommDataController::GetInstance()->curJogCoordSysID));
//    }
//}

/**
 * @brief 坐标系下拉框选择变化：向通信层下发坐标系切换请求
 *
 * 组装 setCoordinateTcpMessage（坐标系类型 / 编号 / TCP 编号）并置 setTCPInCoordSysSignal，
 * 交由通信层下发；视角的实际刷新在 updateUseCoordSysSlot() 中随实时数据回读完成。
 */
void Robot3dGLWidget::UpdateCoordSysChange_slot(int itemIndex)
{
    CommDataController::GetInstance()->UserOperateLogPrint(UserOperateType::combox,"坐标系",this->selCoordSysCb->currentText());
    // 当前选中的坐标系 ID
    QString selCoordSysID = this->selCoordSysCb->currentData().toString();
    CommDataController::GetInstance()->setCoordinateTcpMessage.coordinatetype = static_cast<CRData__CoordinateType>(RobotConfig::GetInstance()->GetCoordSysType(selCoordSysID));
    //CommDataController::GetInstance()->setCoordinateTcpMessage.tcpid =  CommDataController::GetInstance()->curJogPara.tcpID;
    CommDataController::GetInstance()->setCoordinateTcpMessage.tcpid = -1;
    CommDataController::GetInstance()->setCoordinateTcpMessage.coordinateid = RobotConfig::GetInstance()->GetIDNumber(selCoordSysID);
    // 置下发标志，由通信层发送坐标系切换请求
    CommDataController::GetInstance()->setTCPInCoordSysSignal = true;

//    Pose coordSysPose={0,0,0,0,0,0};

//    int selIndex = -1;
//    for(int index = 0; index < RobotConfig::GetInstance()->coordSysList.count(); index++)
//    {
//        if(selCoordSysID == RobotConfig::GetInstance()->coordSysList.at(index).coordSysID)
//        {
//            selIndex = index;
//            CopyPose(RobotConfig::GetInstance()->coordSysList.at(index).coordSysPose,&coordSysPose);
//        }
//    }

//    for(int index = 0; index < RobotConfig::GetInstance()->wcsList.count(); index++)
//    {
//        if(selCoordSysID == RobotConfig::GetInstance()->wcsList.at(index).wcsID)
//        {
//            selIndex = index;
//            Pose workInWorld = RobotConfig::GetInstance()->getWorkSysInWorldPose(selCoordSysID);
//            Pose worldInBase;
//            cr_PoseInv(worldInBase,CommDataController::GetInstance()->baseInWorldPose);
//            cr_PoseTrans(coordSysPose,worldInBase,workInWorld);
//            //CopyPose(RobotConfig::GetInstance()->wcsList.at(index).root2Ref,&coordSysPose);
//        }
//    }

//    if(selIndex>=0)
//    {
//        curRobot3DShowData.ViewChangeMatrix=ViewChangeMatrixFunction(coordSysPose);
////        if(RobotConfig::GetInstance()->coordSysList.at(selIndex).coordSysID== "t_0")
////        {}

//        CommDataController::GetInstance()->curJogPara.coordSysID = selCoordSysID;

////        // 点动坐标系
////        CommDataController::GetInstance()->Enable_Set_ControlSet();

////        QString curCoordSysTypeStr = RobotConfig::GetInstance()->coordSysList.at(selIndex).coordSysID.left(1);
////        CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->has_jogcoordinatetype = 1;
////        if(curCoordSysTypeStr=="b")
////            CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinatetype = CRData__CoordinateType::CR__DATA__COORDINATE_TYPE__baseCoordinate;
////        else if(curCoordSysTypeStr=="t")
////            CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinatetype = CRData__CoordinateType::CR__DATA__COORDINATE_TYPE__ToolBaseCoordinate;
////        else if(curCoordSysTypeStr=="p")
////            CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinatetype = CRData__CoordinateType::CR__DATA__COORDINATE_TYPE__PointCoordinate;
////        else if(curCoordSysTypeStr=="l")
////            CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinatetype = CRData__CoordinateType::CR__DATA__COORDINATE_TYPE__LineCoordinate;
////        else if(curCoordSysTypeStr=="f")
////            CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinatetype = CRData__CoordinateType::CR__DATA__COORDINATE_TYPE__PlaneCoordinate;

////        CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->has_jogcoordinateindex = 1;
////        CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinateindex = RobotConfig::GetInstance()->GetIDNumber(RobotConfig::GetInstance()->coordSysList.at(selIndex).coordSysID);

////        CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->n_jogcoordinatepose = ROB_AXIS_NUM;
////        CommDataController::GetInstance()->CopyPoseMsg(RobotConfig::GetInstance()->coordSysList.at(selIndex).coordSysPose,  CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinatepose);

////        CommDataController::GetInstance()->setSystemVariableSignal = true;
//    }
//    else
//    {
//        CommDataController::GetInstance()->curJogPara.coordSysID = "b_0";
////        Pose pose={0,0,0,0,0,0};
////        ViewChangeMatrix=ViewChangeMatrixFunction(pose);

////        //点动坐标系
////        CommDataController::GetInstance()->Enable_Set_ControlSet();
////        CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->has_jogcoordinatetype = 1;
////            CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinatetype = CRData__CoordinateType::CR__DATA__COORDINATE_TYPE__baseCoordinate;

////        CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->has_jogcoordinateindex = 1;
////        CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinateindex = 0;

////        CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->n_jogcoordinatepose = ROB_AXIS_NUM;
////        CommDataController::GetInstance()->CopyPoseMsg(pose,  CommDataController::GetInstance()->setSystemVar_RobotData.controlsetpara->jogcoordinatepose);

////        CommDataController::GetInstance()->setSystemVariableSignal = true;
//    }

//    curRobot3DShowData.translate_model=QVector3D(0,0,0);
//    curRobot3DShowData.rotation_model=QQuaternion(1,0,0,0);
//    curRobot3DShowData.scalePara=1.0;
//    zoomSlider->setValue(40);

//    if(coordsysValueChangeSignal)  //坐标系值改变 不用下发该配置  但需要更新坐标轴刷新
//    {
//        coordsysValueChangeSignal = false;
//        return;
//    }


}

/**
 * @brief 由坐标系位姿生成变换矩阵，并同步刷新三条坐标轴线
 * @param pose 坐标系位姿（XYZ 单位 mm，Rx/Ry/Rz 为轴角向量）
 * @return 坐标系变换矩阵
 * @note  该函数带有副作用：更新坐标轴线端点并触发一次 update()。
 */
QMatrix4x4 Robot3dGLWidget::ViewChangeMatrixFunction(Pose pose)
{
    //QMatrix4x4 RotateMatrix=this->Euler2RotateMatrix(pose);
    QMatrix4x4 RotateMatrix = this->AxisAngle2RotateMatrix(pose);

    //修改旋转中心
    //rotate_center=QVector3D(pose.X,pose.Y,pose.Z);

    QVector4D Xdirection(400,0,0,0);
    QVector4D Ydirection(0,400,0,0);
    QVector4D Zdirection(0,0,400,0);

    if(coordSysGeometry.count()==3)
    {
        coordSysGeometry.at(0)->SetLineData(QVector3D(pose.X,pose.Y,pose.Z),QVector3D(RotateMatrix*Xdirection)+QVector3D(pose.X,pose.Y,pose.Z));
        coordSysGeometry.at(1)->SetLineData(QVector3D(pose.X,pose.Y,pose.Z),QVector3D(RotateMatrix*Ydirection)+QVector3D(pose.X,pose.Y,pose.Z));
        coordSysGeometry.at(2)->SetLineData(QVector3D(pose.X,pose.Y,pose.Z),QVector3D(RotateMatrix*Zdirection)+QVector3D(pose.X,pose.Y,pose.Z));
    }

    update();
    return   RotateMatrix;
}

/**
 * @brief 欧拉角(ZYX)转旋转矩阵（平移列按 G_SCALE 换算到模型单位）
 */
QMatrix4x4 Robot3dGLWidget::Euler2RotateMatrix(Pose pose)
{
    float sx=qSin(pose.Rx*M_PI/180);
    float cx=qCos(pose.Rx*M_PI/180);
    float sy=qSin(pose.Ry*M_PI/180);
    float cy=qCos(pose.Ry*M_PI/180);
    float sz=qSin(pose.Rz*M_PI/180);
    float cz=qCos(pose.Rz*M_PI/180);
    QMatrix4x4 RotX={1, 0, 0, 0,0, cx, -sx, 0,0, sx, cx,0,0,0,0,1};
    QMatrix4x4 RotY={cy,0, sy, 0, 0, 1, 0,0, -sy, 0, cy,0,0,0,0,1};
    QMatrix4x4 RotZ={cz,-sz, 0,0, sz, cz, 0,0, 0, 0, 1,0,0,0,0,1};
    QMatrix4x4 RotZYX=RotZ*RotY*RotX;
    //由于关节三维数据在加载时候就被压缩了20倍,所以矩阵的变换也要基于被压缩后的数据进行
    RotZYX.setColumn(3,QVector4D(pose.X,pose.Y,pose.Z,1.0/G_SCALE)*G_SCALE);
    return RotZYX;
}
/**
 * @brief 轴角转旋转矩阵（罗德里格斯公式）
 * @param pose XYZ 为平移（mm），Rx/Ry/Rz 组成轴角向量（弧度，模长为旋转角）
 * @note  轴角模长近似为 0 时退化为单位旋转；平移列按 G_SCALE 换算到模型单位。
 */
QMatrix4x4 Robot3dGLWidget::AxisAngle2RotateMatrix(Pose pose)
 {
     double theta;
     double r[3];
     pose.Rx=pose.Rx*M_PI/180;
     pose.Ry=pose.Ry*M_PI/180;
     pose.Rz=pose.Rz*M_PI/180;
     QMatrix4x4 RotZYX;

     theta = sqrt((pose.Rx * pose.Rx + pose.Ry *  pose.Ry +  pose.Rz * pose.Rz))   /*pi/180.0*/;
     // 旋转角近似为 0：退化为单位旋转矩阵
     if (fabs(theta) < 1e-10)
     {
         RotZYX.setToIdentity();
     }
     else
     {
         r[0] = pose.Rx / theta;
         r[1] = pose.Ry / theta;
         r[2] = pose.Rz / theta;
         RotZYX.setRow(0,QVector4D(r[0]*r[0]*(1-cos(theta))+cos(theta),r[0]*r[1]*(1-cos(theta))-r[2]*sin(theta),r[0]*r[2]*(1-cos(theta))+r[1]*sin(theta),1.0));
         RotZYX.setRow(1,QVector4D(r[0]*r[1]*(1-cos(theta))+r[2]*sin(theta),r[1]*r[1]*(1-cos(theta))+cos(theta),r[1]*r[2]*(1-cos(theta))-r[0]*sin(theta),1.0));
         RotZYX.setRow(2,QVector4D(r[0]*r[2]*(1-cos(theta))-r[1]*sin(theta),r[1]*r[2]*(1-cos(theta))+r[0]*sin(theta),r[2]*r[2]*(1-cos(theta))+cos(theta),1.0));

     }
     RotZYX.setColumn(3,QVector4D(pose.X,pose.Y,pose.Z,1.0/G_SCALE)*G_SCALE);
     return RotZYX;
}

/**
 * @brief 由平面位姿与尺寸生成平面四个角点
 * @param planePose 平面中心位姿
 * @param deltZ     沿平面法向的平移量
 * @param alpha     透明度
 * @param color     颜色
 * @param length    平面长度（mm）
 * @param width     平面宽度（mm）
 * @return PlaneMsg 按逆时针排列的四个角点，附带颜色与透明度
 */
PlaneMsg Robot3dGLWidget::PlanePose2Points(Pose planePose, float deltZ, float alpha, QColor color, float length,float width)
{
    QMatrix4x4 rotateMatrix=AxisAngle2RotateMatrix(planePose);
    PlaneMsg planeMsg;

    QVector3D rx =QVector3D(rotateMatrix.column(0).x(),rotateMatrix.column(0).y(),rotateMatrix.column(0).z());
    QVector3D ry =QVector3D(rotateMatrix.column(1).x(),rotateMatrix.column(1).y(),rotateMatrix.column(1).z());
    QVector3D rz =QVector3D(rotateMatrix.column(2).x(),rotateMatrix.column(2).y(),rotateMatrix.column(2).z());
    QVector3D xyz=QVector3D(rotateMatrix.column(3).x(),rotateMatrix.column(3).y(),rotateMatrix.column(3).z())/G_SCALE;

    // 四个角点：中心 ± 半边长（逆时针），并沿法向偏移 deltZ
    planeMsg.firstPt=xyz+(rx*length-ry*width)/2.0+deltZ*rz;
    planeMsg.secondPt=xyz+(rx*length+ry*width)/2.0+deltZ*rz;
    planeMsg.thirdPt=xyz+(-rx*length+ry*width)/2.0+deltZ*rz;
    planeMsg.fourPt=xyz+(-rx*length-ry*width)/2.0+deltZ*rz;
    planeMsg.color=color;
    planeMsg.alpha=alpha;
    return planeMsg;

}
/**
 * @brief 读取 OpenGL 版本号并写入全局 g_openglVersion
 * @note  各几何类依据该值选择顶点缓冲的更新方式（>2 时使用 map/memcpy）。
 */
void Robot3dGLWidget::GetOpenGLVersion()
{
    const GLubyte* OpenGLVersion =glGetString(GL_VERSION); //返回当前OpenGL实现的版本号
    QString version;
    version.sprintf("%s",OpenGLVersion);
    bool sign=true;   //获取版本号标志
    for(int j = 0; j < version.length(); j++)
    {
        // 取版本字符串中的第一个数字字符作为主版本号
        if(version[j] > '0' && version[j] < '9')
        {
            QString tmp=QString(version[j]);
            g_openglVersion=tmp.toInt();
            sign=false;
            break;
        }
    }
//    //如果没有获取到版本号，会将获取到的信息以txt记录下来。
//    if(sign)
//    {
//        QFile file;
//        file.setFileName("openglVersion.txt");
//        if (file.open(QIODevice::ReadWrite | QIODevice::Text))
//        {
//            QTextStream stream(&file);
//            stream << version << endl;
//        }
//        file.close();
//    }
}

/**
 * @brief 刷新 TCP 速度显示（控件可见时更新，保留三位小数）
 */
void Robot3dGLWidget::UpdateTCPspeed_slot(double tcpSpeed)
{
    if(this->isVisible() == true)
        this->curTcpSpeedLabel->setText(QString("%1 mm/s").arg(QString::number(tcpSpeed, 'f', 3)));
}

/**
 * @brief 进入 / 退出世界安装视角模式
 * @param isIn true-进入，false-退出
 *
 * 进入：备份当前视角，重置坐标系变换与旋转中心，平移到 (0,0,20)，缩放恢复为 1；
 * 退出：按当前 Jog 坐标系重算坐标轴线，并恢复进入前的视角；
 * 当前状态由 isInWorldTransSignal 标记。
 */
void Robot3dGLWidget::IsIntoWorldTransformWidget(bool isIn)
{
    // 状态切换时先备份当前视角
    if(this->isInWorldTransSignal == false)
    {
        this->preRobot3DShowData.scalePara=this->curRobot3DShowData.scalePara;
        this->preRobot3DShowData.rotate_center=this->curRobot3DShowData.rotate_center;
        this->preRobot3DShowData.rotation_model=this->curRobot3DShowData.rotation_model;
        this->preRobot3DShowData.translate_model=this->curRobot3DShowData.translate_model;
        this->preRobot3DShowData.ViewChangeMatrix=this->curRobot3DShowData.ViewChangeMatrix;
    }

    // 进入安装视角：复位坐标系变换、旋转中心、缩放与平移
    if(isIn)
    {
        Pose tmpPose = {0,0,0,0,0,0};
        this->ViewChangeMatrixFunction(tmpPose);

        this->curRobot3DShowData.scalePara=1;
        this->curRobot3DShowData.ViewChangeMatrix.setToIdentity();
        this->curRobot3DShowData.rotate_center=QVector3D(0,0,0);
        this->curRobot3DShowData.translate_model=QVector3D(0,0,20);
        this->curRobot3DShowData.rotation_model=QQuaternion(1,0,0,0);
    }
    else
    {
        // 退出安装视角：按当前 Jog 坐标系刷新坐标轴线后恢复备份视角
        if(this->isInWorldTransSignal==true)
        {
            if(CommDataController::GetInstance()->curJogPara.coordSysID.contains("wcs_"))
            {
                Pose workInWorld = RobotConfig::GetInstance()->getWorkSysInWorldPose(CommDataController::GetInstance()->curJogPara.coordSysID);
                Pose worldInBase;
                cr_PoseInv(worldInBase,CommDataController::GetInstance()->baseInWorldPose);
                Pose result;
                cr_PoseTrans(result,worldInBase,workInWorld);
                this->ViewChangeMatrixFunction(result);
            }
            else
            {
                this->ViewChangeMatrixFunction(RobotConfig::GetInstance()->GetCoordSysPose(CommDataController::GetInstance()->curJogPara.coordSysID));
            }

            this->curRobot3DShowData.scalePara=this->preRobot3DShowData.scalePara;
            this->curRobot3DShowData.rotate_center=this->preRobot3DShowData.rotate_center;
            this->curRobot3DShowData.rotation_model=this->preRobot3DShowData.rotation_model;
            this->curRobot3DShowData.translate_model=this->preRobot3DShowData.translate_model;
            this->curRobot3DShowData.ViewChangeMatrix=this->preRobot3DShowData.ViewChangeMatrix;
        }
    }

    this->isInWorldTransSignal = isIn;
}
/**
 * @brief 机型切换：按新机型名与 DH 参数重建两台机器人的模型，并恢复末端工件按钮图标
 */
void Robot3dGLWidget::ChangeRobotType_slot()
{
    if(RamMonotorThread::GetInstance()->MemoryCheck(__FUNCTION__,__FILE__))
        return;

    this->robotGeometry->ChangeRobot3D(CommDataController::GetInstance()->robotType.robotTypeParaStr, CommDataController::GetInstance()->stdRobotWholeDH);
    this->secondRobotGeometry->ChangeRobot3D(CommDataController::GetInstance()->robotType.robotTypeParaStr, CommDataController::GetInstance()->stdRobotWholeDH);
    QPushButton *tmp = this->findChild<QPushButton *>("showEndToolPb");
    if (showEndToolStatus == 0)
    {
        tmp->setIcon(QPixmap(QString(":/resource/image/%1.png").arg("end_tool_hide")));
    }
    else
    {
        tmp->setIcon(QPixmap(QString(":/resource/image/%1.png").arg("end_tool_show")));
    }

}
