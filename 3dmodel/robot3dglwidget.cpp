#include "robot3dglwidget.h"
#include "demo_support.h"

int showEndToolStatus = 0; //是否显示末端工件，0-不显示，1-显示，默认0

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

Robot3dGLWidget::~Robot3dGLWidget()
{
    // Make sure the context is current when deleting the texture and the buffers.
    makeCurrent();
    doneCurrent();
}

void Robot3dGLWidget::InitUI()
{
    //放大缩小功能
    QStringList zoomNames;
    zoomNames << "zoomin" << "zoomout";

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
            zoomSlider->setValue(40);
            connect(zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(ZoomSlider_valueChanged(int)));
            zoomLayout->addWidget(zoomSlider);
        }
    }
    zoomLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Fixed,QSizePolicy::Expanding));

    //坐标系选择
    coordSysWidget = new QWidget(this);
    QHBoxLayout* coordSysLayout = new QHBoxLayout();
    coordSysLayout->setSpacing(10);
    coordSysWidget->setLayout(coordSysLayout);

    QLabel* selCoordSysLabel = new QLabel(tr("坐标系:"),this);
    selCoordSysLabel->setStyleSheet("width:50px;");
    selCoordSysCb = new QComboBox(this);
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

    this->rotateTranslateStatus = 0;
     QPushButton* rotateTranslatePb =new QPushButton(QIcon(":/resource/image/R_T_change.png"),"",this);
     rotateTranslatePb->setObjectName("rotateTranslatePb");
     rotateTranslatePb->setIconSize(QSize(36,36));
     rotateTranslatePb->setFixedSize(QSize(40,40));
     rotateTranslatePb->setStyleSheet("QPushButton{border:none;background:transparent;}"
                                   "QPushButton:pressed{border:1px solid #32b67a;}");
     QObject::connect(rotateTranslatePb, SIGNAL(clicked()), this, SLOT(RotateTranslatChange_slot()));

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
     waypointOperateWidget = new QWidget(this);
     QHBoxLayout *waypointOperateLayout = new QHBoxLayout();
     waypointOperateLayout->setSpacing(10);
     waypointOperateWidget->setLayout(waypointOperateLayout);

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

     QHBoxLayout* topLayout = new QHBoxLayout();
     topLayout->addWidget(coordSysWidget);
     topLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding,QSizePolicy::Fixed));
     topLayout->addWidget(waypointOperateWidget);

     QHBoxLayout* midLayout = new QHBoxLayout();
     midLayout->setSpacing(0);
     midLayout->addWidget(robotOperateWidget);
     midLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding,QSizePolicy::Expanding));
     midLayout->addWidget(zoomWidget);

     QHBoxLayout* bottomLayout = new QHBoxLayout();
     bottomLayout->addItem(new QSpacerItem(30,1,QSizePolicy::Fixed,QSizePolicy::Fixed));
     bottomLayout->addWidget(tcpSpeedWidget);
     bottomLayout->addWidget(poseOperateWidget);
     bottomLayout->addItem(new QSpacerItem(10,1,QSizePolicy::Fixed,QSizePolicy::Fixed));

     QVBoxLayout* mainLayout = new QVBoxLayout(this);
     mainLayout->setMargin(0);
     mainLayout->addLayout(topLayout );
     mainLayout->addLayout(midLayout );
     mainLayout->addLayout(bottomLayout );
     mainLayout->addItem(new QSpacerItem(1,5,QSizePolicy::Fixed,QSizePolicy::Fixed));
     this->setLayout(mainLayout);
}

void Robot3dGLWidget::SetShowType(Robot3DShowTypes showType)
{    
    //坐标系切换
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
    QMatrix4x4 tmpMatrix = this->ViewChangeMatrixFunction(jogCoordSysPose);

    //
    this->curShowType = showType;

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

void Robot3dGLWidget::ZoomSlider_valueChanged(int value)
{
    curRobot3DShowData.scalePara = 0.4+(double)value/10.0*0.15;
    this->update();
}

void Robot3dGLWidget::mousePressEvent(QMouseEvent *e)
{
    // Save mouse press position    
    curPos = QVector2D(e->localPos());
    mousePressSignal = true;
}

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

void Robot3dGLWidget::mouseMoveEvent(QMouseEvent *e)
{
        QVector2D diff = QVector2D(e->localPos()) - curPos;
      curPos = QVector2D(e->localPos());

    if(this->rotateTranslateStatus == 0)
    {
//        QVector2D diff = QVector2D(e->localPos()) - mousePressPosition;
        double rotateAngle = diff.length() / 5;

        QVector3D  rotationAxis= QVector3D( 0.0,diff.y(), diff.x()).normalized();
        QVector4D RotateViewExchange=(curRobot3DShowData.ViewChangeMatrix)*QVector4D(rotationAxis,0);
        curRobot3DShowData.rotation_model = QQuaternion::fromAxisAndAngle(QVector3D(RotateViewExchange), rotateAngle) * curRobot3DShowData.rotation_model;
        //rotation_light = QQuaternion::fromAxisAndAngle(QVector3D(RotateViewExchange), -rotateAngle) * rotation_light;
    }
    else if(this->rotateTranslateStatus == 1)
    {
        curRobot3DShowData.translate_model = QVector3D(0,diff.x()/5,-diff.y()/5)+curRobot3DShowData.translate_model;
    }

    mousePressSignal = true;
    update();
}

void Robot3dGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    GetOpenGLVersion();

    glClearColor(1, 1, 1, 1);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST); 
    //glEnable(GL_CULL_FACE);//旋转面模型时，某些位姿看不到

    robotGeometry = new RobotGeometry();
    double array[6] = {0,0,0,0,0,0};
    robotGeometry->multjointTransform(array);
    robotGeometry->set_rotate_center(curRobot3DShowData.rotate_center); //该函数需放在multjointTransform之后
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

    //坐标系
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

    //TCP
    //double tmpJoint_pos[ROB_AXIS_NUM] = {0};

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

    QVector<QVector3D> tmpWaypoints;
    waypointPathlineGeometry = new LineGeometry(tmpWaypoints, QColor("#3255b6"));
    waypointPathlineGeometry->initShaders();

    //路点
    QVector<SphereMsg> sphereMsgs;
    waypointSphereGeometry = new SphereGeometry(sphereMsgs,500);
    waypointSphereGeometry->initShaders();

    //安全平面
    for(int planeIndex=0; planeIndex<CR6_SAFETY_LIMITS_BOUNDARY_PLANE_NUM; planeIndex++)
    {
        PlaneMsg planeMsg;
        planeMsg.color = QColor("#32b67a");
        planeMsg.alpha = 0;
        PlaneGeometry* tmpPlaneGeometry = new PlaneGeometry(planeMsg);
        tmpPlaneGeometry->initShaders();
        safePlaneGeometrys.append(tmpPlaneGeometry);
    }

    //工具
    SubstanceMsg substanceMsg = {0, {0,0,0,0,0,0}, 1.0f, 1.0f, 1.0f, 1.0f, QColor("#3255b6")};
    for(int index=0; index<ROB_AXIS_NUM; index++)
        substanceMsg.centerPose[index] = tmpPose[index];

    tool_Geometry = new SubstanceGeometry(substanceMsg);
    tool_Geometry->initShaders();
    //工具的坐标系
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

    emit widgetSizeChanged(w, h);
}

void Robot3dGLWidget::resizeCustomedWidget(int w, int h)
{
    maxW = maxW < w ? w : maxW;
    maxH = maxH < h ? h : maxH;
    double r = ((double) h) / (maxH ? maxH : 1);
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

void Robot3dGLWidget::paintGL()
{
    // Clear color and depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //model矩阵先缩放再平移，代码实现时候顺序相反

    QMatrix4x4 model;
    model.setToIdentity();
    model.scale(curRobot3DShowData.scalePara);
    model=(curRobot3DShowData.ViewChangeMatrix).inverted()*model;
    QVector4D TranslateViewExchange=(curRobot3DShowData.ViewChangeMatrix)*QVector4D(curRobot3DShowData.translate_model,0);
    model.translate(QVector3D(TranslateViewExchange)/curRobot3DShowData.scalePara);

    model.translate(curRobot3DShowData.rotate_center);
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
    QVector3D Up(0,0,1);

    view.lookAt(robotGeometry->Pos,robotGeometry->Target,Up);

    QMatrix4x4 mvpMatrix = projection*view*model;


    //#ifdef ROCKCHIP_ARM
    //    mvpMatrix.setColumn(3,QVector4D(mvpMatrix.column(3).x(),mvpMatrix.column(3).y(),mvpMatrix.column(3).z(),200));
    //#endif

    QMatrix4x4 light_matrix;
    //light_matrix.rotate(rotation_light);

    robotGeometry->drawGeometry( model, mvpMatrix,  light_matrix);

    if(secondRobotShowSignal)
        secondRobotGeometry->drawSecondGeometry(model, mvpMatrix, light_matrix);

    TCP_Geometry->drawGeometry( model, mvpMatrix,  light_matrix);

    for(int index = 0; index < this->coordSysGeometry.count(); index++)
    {
        coordSysGeometry.at(index)->drawGeometry( model, mvpMatrix,  light_matrix);
    }

    if(waypointShowSignal)
    {
        if(waypointSphereGeometry != NULL)
            waypointSphereGeometry->drawGeometry( model, mvpMatrix,  light_matrix);
    }

    if(waypointPathShowSignal)
    {
        if(waypointPathlineGeometry != NULL)
        {
            waypointPathlineGeometry->drawGeometry( model, mvpMatrix,  light_matrix);
        }
    }

    if(safePlaneShowSignal)
    {
        for(int index=0; index<safePlaneGeometrys.count(); index++)
            safePlaneGeometrys.at(index)->drawGeometry(model, mvpMatrix, light_matrix);
    }

    if(toolShowSignal && tool_Geometry != NULL)
    {
        for(int index = 0; index < this->toolCoordSysGeometry.count(); index++)
        {
            toolCoordSysGeometry.at(index)->drawGeometry( model, mvpMatrix,  light_matrix);
        }
        tool_Geometry->drawGeometry(model, mvpMatrix, light_matrix);
    }
}

void Robot3dGLWidget::SetRobotJointAngle_slot(JointPose jointPose)
{
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
void Robot3dGLWidget::SetRobotSafetyToolData(QVector<SubstanceMsg> substanceMsgsIn)
{
    tool_Geometry->SetSubstanceData(substanceMsgsIn);
    Pose pose;
    memcpy(&pose, substanceMsgsIn.at(0).centerPose, sizeof(double)*6);
    QMatrix4x4 RotateMatrix = this->AxisAngle2RotateMatrix(pose);

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

void Robot3dGLWidget::updateUseCoordSysSlot()
{
    if (!CommDataController::GetInstance()->updateRealtimeRobot3dMsgSignal)
        return ;
    CoordSysType tmpCoorSysType = CommDataController::GetInstance()->curTcpInCoordSysMsg.coordSysTcpPara.coordSysType;
    QString coordsysID = "b_0";
    Pose coordSysPose={0,0,0,0,0,0};
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


    int index = this->selCoordSysCb->findData(coordsysID);
    if (index != -1) {
        this->selCoordSysCb->blockSignals(true);
        this->selCoordSysCb->setCurrentIndex(index);
        this->selCoordSysCb->blockSignals(false);
    }

    curRobot3DShowData.ViewChangeMatrix = ViewChangeMatrixFunction(coordSysPose);
    CommDataController::GetInstance()->curJogPara.coordSysID = coordsysID;
    CommDataController::GetInstance()->curExjJogPara.coordSysID = coordsysID;
}

void Robot3dGLWidget::UpdateCurrentTcp_slot()
{
    //curTCPNameLabel->setText(RobotConfig::GetInstance()->GetTCPnameFromID(RobotConfig::GetInstance()->tcpMsg.ActiveTCP_ID));
    if(CommDataController::GetInstance()->curJogPara.tcpID<0)
    {
        curTCPNameLabel->setText(RobotConfig::GetInstance()->GetTCPnameFromPose(CommDataController::GetInstance()->curJogPara.tcpOffset));
    }
    else
    {
        curTCPNameLabel->setText(RobotConfig::GetInstance()->GetTCPnameFromID(QString("tcp_%1").arg(CommDataController::GetInstance()->curJogPara.tcpID)));
    }
}
void Robot3dGLWidget::UpdateCoordSysCombo()
{
    QString tmpVarID = this->selCoordSysCb->currentData().toString();

    this->selCoordSysCb->blockSignals(true);
    this->selCoordSysCb->clear();

    int selIndex = -1;
    for(int index = 0; index < RobotConfig::GetInstance()->coordSysList.count(); index++)
    {
        if(RobotConfig::GetInstance()->coordSysList.at(index).coordSysValid)
        {
            this->selCoordSysCb->addItem(RobotConfig::GetInstance()->coordSysList.at(index).coordSysName, RobotConfig::GetInstance()->coordSysList.at(index).coordSysID);

            if(tmpVarID == RobotConfig::GetInstance()->coordSysList.at(index).coordSysID)
            {
                selIndex = index;
            }
        }
    }

    for(int index = 0; index < RobotConfig::GetInstance()->wcsList.count(); index++)
    {
        this->selCoordSysCb->addItem(RobotConfig::GetInstance()->wcsList.at(index).wcsName, RobotConfig::GetInstance()->wcsList.at(index).wcsID);
        if(tmpVarID == RobotConfig::GetInstance()->wcsList.at(index).wcsID)
        {
            selIndex = index;
        }
    }

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

void Robot3dGLWidget::UpdateCoordSysChange_slot(int itemIndex)
{
    CommDataController::GetInstance()->UserOperateLogPrint(UserOperateType::combox,"坐标系",this->selCoordSysCb->currentText());
    QString selCoordSysID = this->selCoordSysCb->currentData().toString();
    CommDataController::GetInstance()->setCoordinateTcpMessage.coordinatetype = static_cast<CRData__CoordinateType>(RobotConfig::GetInstance()->GetCoordSysType(selCoordSysID));
    //CommDataController::GetInstance()->setCoordinateTcpMessage.tcpid =  CommDataController::GetInstance()->curJogPara.tcpID;
    CommDataController::GetInstance()->setCoordinateTcpMessage.tcpid = -1;
    CommDataController::GetInstance()->setCoordinateTcpMessage.coordinateid = RobotConfig::GetInstance()->GetIDNumber(selCoordSysID);
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
QMatrix4x4 Robot3dGLWidget::AxisAngle2RotateMatrix(Pose pose)
 {
     double theta;
     double r[3];
     pose.Rx=pose.Rx*M_PI/180;
     pose.Ry=pose.Ry*M_PI/180;
     pose.Rz=pose.Rz*M_PI/180;
     QMatrix4x4 RotZYX;

     theta = sqrt((pose.Rx * pose.Rx + pose.Ry *  pose.Ry +  pose.Rz * pose.Rz))   /*pi/180.0*/;
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

PlaneMsg Robot3dGLWidget::PlanePose2Points(Pose planePose, float deltZ, float alpha, QColor color, float length,float width)
{
    QMatrix4x4 rotateMatrix=AxisAngle2RotateMatrix(planePose);
    PlaneMsg planeMsg;

    QVector3D rx =QVector3D(rotateMatrix.column(0).x(),rotateMatrix.column(0).y(),rotateMatrix.column(0).z());
    QVector3D ry =QVector3D(rotateMatrix.column(1).x(),rotateMatrix.column(1).y(),rotateMatrix.column(1).z());
    QVector3D rz =QVector3D(rotateMatrix.column(2).x(),rotateMatrix.column(2).y(),rotateMatrix.column(2).z());
    QVector3D xyz=QVector3D(rotateMatrix.column(3).x(),rotateMatrix.column(3).y(),rotateMatrix.column(3).z())/G_SCALE;

    planeMsg.firstPt=xyz+(rx*length-ry*width)/2.0+deltZ*rz;
    planeMsg.secondPt=xyz+(rx*length+ry*width)/2.0+deltZ*rz;
    planeMsg.thirdPt=xyz+(-rx*length+ry*width)/2.0+deltZ*rz;
    planeMsg.fourPt=xyz+(-rx*length-ry*width)/2.0+deltZ*rz;
    planeMsg.color=color;
    planeMsg.alpha=alpha;
    return planeMsg;

}
void Robot3dGLWidget::GetOpenGLVersion()
{
    const GLubyte* OpenGLVersion =glGetString(GL_VERSION); //返回当前OpenGL实现的版本号
    QString version;
    version.sprintf("%s",OpenGLVersion);
    bool sign=true;   //获取版本号标志
    for(int j = 0; j < version.length(); j++)
    {
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

void Robot3dGLWidget::UpdateTCPspeed_slot(double tcpSpeed)
{
    if(this->isVisible() == true)
        this->curTcpSpeedLabel->setText(QString("%1 mm/s").arg(QString::number(tcpSpeed, 'f', 3)));
}

void Robot3dGLWidget::IsIntoWorldTransformWidget(bool isIn)
{
    if(this->isInWorldTransSignal == false)
    {
        this->preRobot3DShowData.scalePara=this->curRobot3DShowData.scalePara;
        this->preRobot3DShowData.rotate_center=this->curRobot3DShowData.rotate_center;
        this->preRobot3DShowData.rotation_model=this->curRobot3DShowData.rotation_model;
        this->preRobot3DShowData.translate_model=this->curRobot3DShowData.translate_model;
        this->preRobot3DShowData.ViewChangeMatrix=this->curRobot3DShowData.ViewChangeMatrix;
    }

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
