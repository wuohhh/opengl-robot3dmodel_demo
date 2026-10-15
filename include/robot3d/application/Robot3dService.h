#pragma once
#include "robot3d/ports/IRobotConfigRepository.h"
namespace robot3d { namespace application {
class Robot3dService {
public:
    explicit Robot3dService(ports::IRobotConfigRepository& repository);
    core::Result<domain::RobotConfigurationPtr> prepareRobotType(const std::string& id);
    core::Result<domain::RobotStateSnapshot> commitRobotType(domain::RobotConfigurationPtr config);
    core::Result<domain::RobotStateSnapshot> applyJointPose(const domain::JointPoseDeg& joints);
    core::Result<domain::RobotStateSnapshot> selectCoordinate(const std::string& id);
    core::Result<domain::RobotStateSnapshot> applySample(const domain::JointPoseDeg& joints, double simulatedSpeed);
    void addTrajectoryPoint(const domain::EulerPoseDeg& pose, bool scriptRunning);
    void addWaypoint();
    core::Result<domain::RobotStateSnapshot> applyObservedTcp(const domain::EulerPoseDeg& pose);
    const domain::RobotStateSnapshot& snapshot() const { return state_; }
    std::vector<std::string> robotTypes() const { return repository_.robotTypes(); }
private:
    void refreshCoordinate();
    ports::IRobotConfigRepository& repository_;
    domain::RobotStateSnapshot state_;
    domain::TrajectorySampler sampler_;
};
}}
