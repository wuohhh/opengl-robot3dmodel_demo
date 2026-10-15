#include "robot3d/application/Robot3dService.h"
#include "robot3d/domain/Kinematics.h"
#include <cmath>
namespace robot3d { namespace application {
using namespace domain;
Robot3dService::Robot3dService(ports::IRobotConfigRepository& r) : repository_(r) {
    state_.coordinates.push_back({"b_0", "基坐标系", {0,0,0,0,0,0}});
    state_.coordinates.push_back({"t_0", "工具坐标系", {0,0,0,0,0,0}});
    state_.coordinates.push_back({"p_0", "用户坐标系1", {400,0,200,0,0,0}});
    state_.coordinates.push_back({"wcs_0", "工件坐标系1", {200,300,0,0,0,0}});
}
core::Result<RobotConfigurationPtr> Robot3dService::prepareRobotType(const std::string& id) {
    auto result=repository_.load(id); if(!result.ok || !result.value) return result.ok?core::Result<RobotConfigurationPtr>::failure("机型配置为空"):result;
    for(const auto& dh:result.value->dh) for(double v:{dh.alpha,dh.a,dh.d,dh.theta,dh.beta}) if(!std::isfinite(v)) return core::Result<RobotConfigurationPtr>::failure("DH 参数无效");
    EulerPoseDeg pose={}; forwardKinematics(state_.joints,pose,result.value->dh.data());
    for(double v:{pose.X,pose.Y,pose.Z,pose.Rx,pose.Ry,pose.Rz}) if(!std::isfinite(v)) return core::Result<RobotConfigurationPtr>::failure("正解结果无效");
    return result;
}
core::Result<RobotStateSnapshot> Robot3dService::commitRobotType(RobotConfigurationPtr c) {
    if(!c) return core::Result<RobotStateSnapshot>::failure("机型配置为空");
    for(const auto& dh:c->dh) {
        if(!std::isfinite(dh.alpha)||!std::isfinite(dh.a)||!std::isfinite(dh.d)||!std::isfinite(dh.theta)||!std::isfinite(dh.beta))
            return core::Result<RobotStateSnapshot>::failure("DH 参数无效");
    }
    EulerPoseDeg tcp={}; forwardKinematics(state_.joints,tcp,c->dh.data());
    const double values[]={tcp.X,tcp.Y,tcp.Z,tcp.Rx,tcp.Ry,tcp.Rz};
    for(double v:values) if(!std::isfinite(v)) return core::Result<RobotStateSnapshot>::failure("正解结果无效");
    state_.configuration=c; state_.tcp=tcp; ++state_.revision; refreshCoordinate();
    return core::Result<RobotStateSnapshot>::success(state_);
}
core::Result<RobotStateSnapshot> Robot3dService::applyJointPose(const JointPoseDeg& j) {
    if(!state_.configuration) return core::Result<RobotStateSnapshot>::failure("尚未载入机型");
    auto values=jointValues(j);
    for(int i=0;i<6;++i) if(!std::isfinite(values[i]) || std::fabs(values[i])>demoJointLimit(i))
        return core::Result<RobotStateSnapshot>::failure("关节角超出演示范围或包含无效数值");
    EulerPoseDeg tcp={}; forwardKinematics(j,tcp,state_.configuration->dh.data());
    const double pose[]={tcp.X,tcp.Y,tcp.Z,tcp.Rx,tcp.Ry,tcp.Rz};
    for(double v:pose) if(!std::isfinite(v)) return core::Result<RobotStateSnapshot>::failure("正解结果无效");
    state_.joints=j; state_.tcp=tcp; ++state_.revision; refreshCoordinate();
    return core::Result<RobotStateSnapshot>::success(state_);
}
core::Result<RobotStateSnapshot> Robot3dService::applySample(const JointPoseDeg& j,double speed) {
    if(!std::isfinite(speed)||speed<0) return core::Result<RobotStateSnapshot>::failure("模拟速度无效");
    auto result=applyJointPose(j); if(result.ok) { state_.simulatedTcpSpeed=speed; result.value=state_; } return result;
}
core::Result<RobotStateSnapshot> Robot3dService::selectCoordinate(const std::string& id) {
    for(const auto& cs:state_.coordinates) if(cs.id==id) {
        state_.coordinateId=id; refreshCoordinate(); ++state_.revision;
        return core::Result<RobotStateSnapshot>::success(state_);
    }
    return core::Result<RobotStateSnapshot>::failure("未知坐标系");
}
void Robot3dService::refreshCoordinate() {
    if(state_.coordinateId=="t_0") { state_.coordinatePose=state_.tcp; return; }
    for(const auto& cs:state_.coordinates) if(cs.id==state_.coordinateId) {
        state_.coordinatePose=cs.pose;
        if(cs.id=="wcs_0") { EulerPoseDeg inverse={}; inversePose(inverse,state_.baseInWorld); poseRelativeTo(state_.coordinatePose,inverse,cs.pose); }
        return;
    }
}
void Robot3dService::addTrajectoryPoint(const EulerPoseDeg& p,bool running) {
    if(running && sampler_.append(p,state_.trajectory)) ++state_.revision;
}
void Robot3dService::addWaypoint() { if(state_.configuration && state_.waypoints.size()<500) { state_.waypoints.push_back(state_.tcp); ++state_.revision; } }

core::Result<RobotStateSnapshot> Robot3dService::applyObservedTcp(const EulerPoseDeg& p) {
    if(!state_.configuration) return core::Result<RobotStateSnapshot>::failure("尚未载入机型");
    for(double v:{p.X,p.Y,p.Z,p.Rx,p.Ry,p.Rz}) if(!std::isfinite(v)) return core::Result<RobotStateSnapshot>::failure("位姿包含无效数值");
    state_.tcp=p; refreshCoordinate(); ++state_.revision; return core::Result<RobotStateSnapshot>::success(state_);
}
}}
