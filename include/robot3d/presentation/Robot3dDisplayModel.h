#pragma once
#include "robot3d/scene/RobotSceneSnapshot.h"
namespace robot3d { namespace presentation {
struct Robot3dDisplayModel {
    domain::RobotStateSnapshot state;
    scene::RobotDisplayOptions options;
    std::vector<std::string> robotTypes;
    bool simulationRunning=false;
};
}}
