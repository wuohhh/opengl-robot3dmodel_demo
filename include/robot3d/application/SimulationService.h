#pragma once
#include "robot3d/domain/RobotTypes.h"
namespace robot3d
{
namespace application
{
/** @brief 一次仿真步进产生的六轴关节位姿与演示 TCP 速度。 */
struct SimulationSample
{
    domain::JointPoseDeg joints; ///< 六轴关节角采样值，单位为度。
    double tcpSpeed;             ///< 演示用 TCP 线速度，单位为 mm/s，并非设备实测速度。
};
class SimulationService
{
public:
    SimulationSample step(double dtSeconds);
    void setRunning(bool running)
    {
        running_ = running;
    }
    bool running() const
    {
        return running_;
    }
    void reset()
    {
        time_ = 0;
        running_ = false;
    }
    double time() const
    {
        return time_;
    }

private:
    double time_ = 0;
    bool running_ = false;
};
}
}
