#include "robot3d/compatibility/LegacyRobot3dViewAdapter.h"
#include "robot3d/presentation/Robot3dPresenter.h"
#include <QDebug>
QPointer<LegacyRobot3dViewAdapter> LegacyRobot3dViewAdapter::instance_;
LegacyRobot3dViewAdapter::LegacyRobot3dViewAdapter(QWidget* parent) : QWidget(parent) {
    viewport_=new OpenGlRobotViewport(this);
    composition_.reset(new robot3d::composition::Robot3dComposition(*this,*this,*viewport_,"robotType"));
}
LegacyRobot3dViewAdapter::~LegacyRobot3dViewAdapter() { composition_.reset(); }
void LegacyRobot3dViewAdapter::InitInstance(QWidget* parent) { if(!instance_) instance_=new LegacyRobot3dViewAdapter(parent); }
void LegacyRobot3dViewAdapter::UnInitInstance() { delete instance_.data(); instance_=nullptr; }
LegacyRobot3dViewAdapter* LegacyRobot3dViewAdapter::GetInstance() { return instance_.data(); }
OpenGlRobotViewport* LegacyRobot3dViewAdapter::openGlViewport(LegacyRobotDisplayFlags type,bool followRealtime) {
    Q_UNUSED(followRealtime)
    if(!viewport_) return nullptr;
    auto o=options_;
    o.coordinatePanel=bool(type&show_CoordSys); o.coordinateAxes=o.coordinatePanel;
    o.operatePanel=bool(type&show_RobotOperate); o.zoomPanel=bool(type&show_Zoom); o.posePanel=bool(type&show_PoseOperate);
    o.waypointPanel=bool(type&show_WaypointOperate); o.speedPanel=bool(type&show_TcpSpeed); o.targetRobot=bool(type&show_TargetRobot);
    o.safePlanes=bool(type&show_SafePlane); o.install=bool(type&show_Install); o.tool=bool(type&show_Tool);
    composition_->presenter().onDisplayOptionsRequested(o);
    return viewport_.data();
}
void LegacyRobot3dViewAdapter::UpdateRobot3dJointAngle(LegacyJointPose j) { composition_->presenter().onJointPoseRequested(j); }
void LegacyRobot3dViewAdapter::UpdateRobot3dToolPosition(LegacyEulerPose p) { composition_->presenter().onObservedTcp(p); }
void LegacyRobot3dViewAdapter::AddRobotWaypointPath(LegacyEulerPose p) { composition_->presenter().onTrajectoryPoint(p,true); }
void LegacyRobot3dViewAdapter::UpdateRobot3dSafetyToolData(QVector<LegacyToolShape> tools) {
    std::vector<robot3d::scene::ToolShape> result;
    for(const auto& t:tools) { robot3d::scene::ToolShape s; s.type=t.substanceType;
        s.pose={t.centerPose[0],t.centerPose[1],t.centerPose[2],t.centerPose[3],t.centerPose[4],t.centerPose[5]};
        s.radius=t.radius; s.length=t.length; s.width=t.width; s.height=t.height; s.rgb={{t.color.red(),t.color.green(),t.color.blue()}}; result.push_back(s); }
    composition_->presenter().onToolsRequested(result);
}
void LegacyRobot3dViewAdapter::ResizeRobot3dMode() { if(viewport_) viewport_->update(); }
bool LegacyRobot3dViewAdapter::prepareModel(robot3d::domain::RobotConfigurationPtr c,std::string& e) { if(!viewport_) { e="Viewport destroyed"; return false; } return viewport_->prepareModel(c,e); }
void LegacyRobot3dViewAdapter::showState(const robot3d::presentation::Robot3dDisplayModel& m) { options_=m.options; if(viewport_) viewport_->showState(m); }
void LegacyRobot3dViewAdapter::submitScene(const robot3d::scene::RobotSceneSnapshot& s) { if(viewport_) viewport_->submitScene(s); }
void LegacyRobot3dViewAdapter::showError(const std::string& e) { qWarning()<<QString::fromUtf8(e.c_str()); }
void LegacyRobot3dViewAdapter::appendLog(const std::string& s) { qDebug()<<QString::fromUtf8(s.c_str()); }
