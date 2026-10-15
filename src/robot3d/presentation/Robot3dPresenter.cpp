#include "robot3d/presentation/Robot3dPresenter.h"
#include <cmath>
namespace robot3d { namespace presentation {
Robot3dPresenter::Robot3dPresenter(IRobot3dView& v,QObject& lifetime,application::Robot3dService& s,application::SimulationService& sim)
    : view_(v),lifetime_(&lifetime),service_(s),simulation_(sim) {
    timer_.setInterval(40);
    connect(&timer_,&QTimer::timeout,this,[this] { if(alive()) advanceSimulation(1,timer_.interval()/1000.0); else timer_.stop(); });
}
void Robot3dPresenter::start(const std::string& type) { onRobotTypeRequested(type); }
bool Robot3dPresenter::onRobotTypeRequested(const std::string& type) {
    if(!alive()) return false;
    auto config=service_.prepareRobotType(type);
    if(!config.ok) { view_.showError(config.error); refresh(); return false; }
    std::string error;
    if(!view_.prepareModel(config.value,error)) { view_.showError(error); refresh(); return false; }
    auto result=service_.commitRobotType(config.value);
    if(!result.ok) { view_.showError(result.error); refresh(); return false; }
    robotTypes_=service_.robotTypes();
    refresh(); view_.appendLog("切换机型: "+type); return true;
}
void Robot3dPresenter::report(const core::Result<domain::RobotStateSnapshot>& r) {
    if(!alive()) return;
    if(!r.ok) view_.showError(r.error);
    refresh();
}
void Robot3dPresenter::onJointPoseRequested(const domain::JointPoseDeg& j) { if(!alive()) return; report(service_.applyJointPose(j)); }
void Robot3dPresenter::onCoordinateRequested(const std::string& id) { if(!alive()) return; report(service_.selectCoordinate(id)); }
void Robot3dPresenter::onSimulationRequested(bool running) {
    if(!alive()) return;
    simulation_.setRunning(running); if(running) timer_.start(); else timer_.stop();
    view_.appendLog(running?"开始运动模拟":"停止运动模拟"); refresh();
}
void Robot3dPresenter::advanceSimulation(int steps,double dt) {
    if(!alive()) return;
    if(steps<0||steps>10000||!std::isfinite(dt)||dt<0) { view_.showError("模拟步数超出范围"); return; }
    for(int i=0;i<steps;++i) {
        auto sample=simulation_.step(dt); auto result=service_.applySample(sample.joints,sample.tcpSpeed);
        if(!result.ok) { view_.showError(result.error); onSimulationRequested(false); return; }
    }
    refresh();
}
void Robot3dPresenter::onDisplayOptionsRequested(const scene::RobotDisplayOptions& o) { if(!alive()) return; scene_.options=o; refresh(); }
void Robot3dPresenter::onTrajectoryPoint(const domain::EulerPoseDeg& p,bool running) { if(!alive()) return; service_.addTrajectoryPoint(p,running); refresh(); }
void Robot3dPresenter::onToolsRequested(const std::vector<scene::ToolShape>& tools) { if(!alive()) return; scene_.tools=tools; refresh(); }
void Robot3dPresenter::onObservedTcp(const domain::EulerPoseDeg& p) { if(!alive()) return; report(service_.applyObservedTcp(p)); }
void Robot3dPresenter::addWaypoint() { if(!alive()) return; service_.addWaypoint(); refresh(); }
void Robot3dPresenter::refresh() {
    if(!alive() || !service_.snapshot().configuration) return;
    scene_.state=service_.snapshot();
    scene_.coordinatePose=scene::legacyDisplayPose(scene_.state.coordinatePose);
    Robot3dDisplayModel model; model.state=scene_.state; model.options=scene_.options;
    model.robotTypes=robotTypes_; model.simulationRunning=simulation_.running();
    view_.showState(model); view_.submitScene(scene_);
}
}}
