#pragma once
#include "robot3d/scene/RobotSceneSnapshot.h"
namespace robot3d { namespace presentation {
struct Robot3dDisplayModel {
    domain::RobotStateSnapshot state;
    scene::RobotDisplayOptions options;
    std::vector<std::string> robotTypes;
    bool simulationRunning=false;
};
class IRobot3dView {
public:
    virtual ~IRobot3dView() {}
    virtual bool prepareModel(domain::RobotConfigurationPtr config,std::string& error)=0;
    virtual void showState(const Robot3dDisplayModel& model)=0;
    virtual void submitScene(const scene::RobotSceneSnapshot& scene)=0;
    virtual void showError(const std::string& error)=0;
    virtual void appendLog(const std::string& message)=0;
};
}}
