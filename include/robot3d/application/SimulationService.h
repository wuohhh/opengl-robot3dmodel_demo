#pragma once
#include "robot3d/domain/RobotTypes.h"
namespace robot3d { namespace application {
struct SimulationSample { domain::JointPoseDeg joints; double tcpSpeed; };
class SimulationService {
public:
    SimulationSample step(double dtSeconds);
    void setRunning(bool running) { running_=running; }
    bool running() const { return running_; }
    void reset() { time_=0; running_=false; }
    double time() const { return time_; }
private: double time_=0; bool running_=false;
};
}}
