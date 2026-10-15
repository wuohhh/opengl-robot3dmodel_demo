#include "robot3dcontrolwidget.h"
#include "robot3d/presentation/Robot3dPresenter.h"
#include <QDebug>
#include <QTouchEvent>
QPointer<Robot3dControlWidget> Robot3dControlWidget::instance_;
Robot3dControlWidget::Robot3dControlWidget(QWidget* parent) : QWidget(parent) {
    viewport_=new Robot3dGLWidget(this);
    composition_.reset(new robot3d::composition::Robot3dComposition(*this,*this,*viewport_,"robotType"));
}
Robot3dControlWidget::~Robot3dControlWidget() { composition_.reset(); }
void Robot3dControlWidget::InitInstance(QWidget* parent) { if(!instance_) instance_=new Robot3dControlWidget(parent); }
void Robot3dControlWidget::UnInitInstance() { delete instance_.data(); instance_=nullptr; }
Robot3dControlWidget* Robot3dControlWidget::GetInstance() { return instance_.data(); }
Robot3dGLWidget* Robot3dControlWidget::GetRobot3dGLWidget(Robot3DShowTypes type,bool followRealtime) { Q_UNUSED(followRealtime) if(viewport_) viewport_->SetShowType(type); return viewport_.data(); }
void Robot3dControlWidget::UpdateRobot3dJointAngle(JointPose j) { composition_->presenter().onJointPoseRequested(j); }
void Robot3dControlWidget::UpdateRobot3dToolPosition(Pose p) { composition_->presenter().onObservedTcp(p); }
void Robot3dControlWidget::AddRobotWaypointPath(Pose p) { composition_->presenter().onTrajectoryPoint(p,true); }
void Robot3dControlWidget::UpdateRobot3dSafetyToolData(QVector<SubstanceMsg> tools) {
    std::vector<robot3d::scene::ToolShape> result;
    for(const auto& t:tools) { robot3d::scene::ToolShape s; s.type=t.substanceType;
        s.pose={t.centerPose[0],t.centerPose[1],t.centerPose[2],t.centerPose[3],t.centerPose[4],t.centerPose[5]};
        s.radius=t.radius; s.length=t.length; s.width=t.width; s.height=t.height; s.rgb={{t.color.red(),t.color.green(),t.color.blue()}}; result.push_back(s); }
    composition_->presenter().onToolsRequested(result);
}
void Robot3dControlWidget::ResizeRobot3dMode() { if(viewport_) viewport_->update(); }
bool Robot3dControlWidget::prepareModel(robot3d::domain::RobotConfigurationPtr c,std::string& e) { if(!viewport_) { e="Viewport destroyed"; return false; } return viewport_->prepareModel(c,e); }
void Robot3dControlWidget::showState(const robot3d::presentation::Robot3dDisplayModel& m) { if(viewport_) viewport_->showState(m); }
void Robot3dControlWidget::submitScene(const robot3d::scene::RobotSceneSnapshot& s) { if(viewport_) viewport_->submitScene(s); }
void Robot3dControlWidget::showError(const std::string& e) { qWarning()<<QString::fromUtf8(e.c_str()); }
void Robot3dControlWidget::appendLog(const std::string& s) { qDebug()<<QString::fromUtf8(s.c_str()); }
TouchLongPressFilter::TouchLongPressFilter(QObject* parent) : QObject(parent) { timer.setInterval(100); connect(&timer,&QTimer::timeout,this,&TouchLongPressFilter::handleLongPress_slot); }
bool TouchLongPressFilter::eventFilter(QObject* obj,QEvent* event) {
    if(event->type()==QEvent::TouchBegin || event->type()==QEvent::MouseButtonPress) { senderPushButton=qobject_cast<QPushButton*>(obj); jogKeepCount=0; timer.start(); }
    if(event->type()==QEvent::TouchEnd || event->type()==QEvent::TouchCancel || event->type()==QEvent::MouseButtonRelease || event->type()==QEvent::Hide) touchEndEvent();
    return QObject::eventFilter(obj,event);
}
void TouchLongPressFilter::handleLongPress_slot() { if(senderPushButton) { senderPushButton->click(); ++jogKeepCount; } }
void TouchLongPressFilter::touchEndEvent() { timer.stop(); if(jogKeepCount>2) emit stopRequested(); jogKeepCount=0; senderPushButton=nullptr; }
