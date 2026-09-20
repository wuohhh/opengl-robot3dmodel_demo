#include "robot3dcontrolwidget.h"
#include "demo_support.h"

Robot3dControlWidget* Robot3dControlWidget::robot3dControl_handle = NULL;

Robot3dControlWidget::Robot3dControlWidget(QWidget *parent) : QWidget(parent)
{
    robot3dGLWidget = new Robot3dGLWidget(parent);
}

void Robot3dControlWidget::InitInstance(QWidget *parent)
{
    if(robot3dControl_handle == NULL)
    {
        robot3dControl_handle = new Robot3dControlWidget(parent);
    }
}

void Robot3dControlWidget::UnInitInstance()
{
    if(robot3dControl_handle != NULL)
    {
    }
}

Robot3dControlWidget* Robot3dControlWidget::GetInstance()
{
    return robot3dControl_handle;
}

Robot3dGLWidget* Robot3dControlWidget::GetRobot3dGLWidget(Robot3DShowTypes showType, bool isUpdateRTRobot3dMsg)
{
    CommDataController::GetInstance()->updateRealtimeRobot3dMsgSignal =  isUpdateRTRobot3dMsg;
    robot3dGLWidget->SetShowType(showType);
    return  robot3dGLWidget;
}
void Robot3dControlWidget::UpdateRobot3dJointAngle(JointPose jointPose)
{
    robot3dGLWidget->SetRobotJointAngle_slot(jointPose);
}
void Robot3dControlWidget::UpdateRobot3dToolPosition(Pose pose)
{
    robot3dGLWidget->SetRobotToolPosition_slot(pose);     
}
void Robot3dControlWidget::UpdateRobot3dSafetyToolData(QVector<SubstanceMsg> substanceMsgsIn)
{
    robot3dGLWidget->SetRobotSafetyToolData(substanceMsgsIn);
}

//重新调整三维窗口尺寸
void Robot3dControlWidget::ResizeRobot3dMode()
{
    robot3dGLWidget->resize(robot3dGLWidget->size());
}

void Robot3dControlWidget::AddRobotWaypointPath(Pose pose)
{
    if(robot3dGLWidget->waypointPathlineGeometry->pointList.count()>=robot3dGLWidget->waypointPathlineGeometry->maxNum)
    {
        robot3dGLWidget->waypointPathlineGeometry->pointList.removeFirst();
    }

    //路点轨迹曲线
    if((CommDataController::GetInstance()->curLuaScriptStatus==CR__DATA__LUA__SCRIPT_STATUS__lua_Script_run)
        ||(CommDataController::GetInstance()->curLuaScriptStatus==CR__DATA__LUA__SCRIPT_STATUS__lua_Script_pause))//if(addWaypointPathLineSignal==true)
    {
        if(robot3dGLWidget->waypointPathlineGeometry->pointList.count()>0)
        {
            if((fabs(pose.X-preTcpPose.X)<5.0) && (fabs(pose.Y-preTcpPose.Y)<5.0) && (fabs(pose.Z-preTcpPose.Z)<5.0))
            {
                return;
            }
        }

        robot3dGLWidget->waypointPathlineGeometry->pointList.append(QVector3D(pose.X,pose.Y,pose.Z));

        if(robot3dGLWidget->waypointPathlineGeometry!=NULL)
        {
            robot3dGLWidget->waypointPathlineGeometry->AddDeleteLineData(robot3dGLWidget->waypointPathlineGeometry->pointList);
        }
    }
    //路点轨迹曲线

    this->preTcpPose = pose;
}

TouchLongPressFilter::TouchLongPressFilter(QObject *parent) : QObject(parent)
{
//        timer.setSingleShot(true);
    pixmapName<<"pose_move_x+.png"<<"pose_move_x-.png"<<"pose_move_y+.png"<<"pose_move_y-.png"<<"pose_move_z+.png"<<"pose_move_z-.png"
            <<"orient_move_x+.png"<<"orient_move_x-.png"<<"orient_move_y+.png"<<"orient_move_y-.png"<<"orient_move_z+.png"<<"orient_move_z-.png";
    pixmapNamePress<<"pose_move_x+_press.png"<<"pose_move_x-_press.png"<<"pose_move_y+_press.png"<<"pose_move_y-_press.png"<<"pose_move_z+_press.png"<<"pose_move_z-_press.png"
            <<"orient_move_x+_press.png"<<"orient_move_x-_press.png"<<"orient_move_y+_press.png"<<"orient_move_y-_press.png"<<"orient_move_z+_press.png"<<"orient_move_z-_press.png";
    moveBtnString = "QPushButton{border:none; height:36px; background-color: #32b67a; color: #ffffff; border-radius: 4px;}"\
                    "QPushButton:pressed{ background-color:#109358; border:none; border-radius: 4px;}"\
                    "QPushButton:disabled{background-color:rgba(50,182,122,0.4);border:none; border-radius: 4px;}";
    moveBtnPressString = "QPushButton{border:none; height:36px; background-color: #109358; color: #ffffff; border-radius: 4px;}"\
                         "QPushButton:pressed{ background-color:#109358; border:none; border-radius: 4px;}"\
                         "QPushButton:disabled{background-color:rgba(50,182,122,0.4);border:none; border-radius: 4px;}";
    connect(&timer, &QTimer::timeout, this, &TouchLongPressFilter::handleLongPress_slot);
}

bool TouchLongPressFilter::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type())
    {
    case QEvent::TouchBegin:
    {
        senderPushButton = static_cast<QPushButton *>(obj);
        if(!senderPushButton->isEnabled())
        {
            event->accept();
            return true;
        }
        if(senderPushButton->objectName() == "move")
        {
            senderPushButton->setStyleSheet(moveBtnPressString+QString("QPushButton{width:100px;}"));
            emit senderPushButton->pressed();
        }
        else
        {
            senderPushButton->setStyleSheet(QString("QPushButton{border:none;background:transparent;border-image: url(:/resource/image/%1);}").arg(pixmapNamePress.at(senderPushButton->objectName().toInt())));
            handleLongPress_slot();
            timer.start(200); // 设置长按时间阈值
        }
        event->accept();
        return true;
    }
    case QEvent::TouchEnd:
    {
        senderPushButton = static_cast<QPushButton *>(obj);
        if(senderPushButton->objectName() == "move")
        {
            senderPushButton->setStyleSheet(moveBtnString+QString("QPushButton{width:100px;}"));
            emit senderPushButton->released();
        }
        else
        {
            senderPushButton->setStyleSheet(QString("QPushButton{border:none;background:transparent;border-image: url(:/resource/image/%1);}").arg(pixmapName.at(senderPushButton->objectName().toInt())));
            timer.stop();
            touchEndEvent();
        }
        event->accept();
        return true;
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if(mouseEvent->source() == Qt::MouseEventSynthesizedBySystem || mouseEvent->source() == Qt::MouseEventSynthesizedByQt)
        {
            event->accept();
            return true;
        }
        break;
    }
    case QEvent::MouseButtonDblClick:
    {
        static_cast<QPushButton *>(obj)->click();
        event->accept();
        return true;
    }
    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

void TouchLongPressFilter::handleLongPress_slot()
{
    //处理长按时的逻辑
    if(senderPushButton != NULL)
    {
        senderPushButton->click();
        jogKeepCount++;
    }
}

void TouchLongPressFilter::touchEndEvent()
{
    if(CommDataController::GetInstance()->curRobotModes <11 || CommDataController::GetInstance()->curRobotModes ==CR__DATA__ROBOT_MODES__CloseBrake)
    {
        TipMsgDialog::GetInstance()->ShowThisDialog(WarningLevel::LV_WARNING, tr("警告"), tr("机械臂未上电使能完成！"));
        return;
    }
    if(senderPushButton != NULL && senderPushButton->isDown()== false)
    {
        if(this->jogKeepCount>2)
        {
            CommDataController::GetInstance()->movePara.movetype = CRService__MoveType::CR__SERVICE__MOVE_TYPE__DecStop;
            CommDataController::GetInstance()->moveControlSignal = true;
        }
        jogKeepCount = 0;
    }
}
