#pragma once
#include "robot3d/scene/RobotSceneSnapshot.h"
namespace robot3d
{
namespace presentation
{
/** @brief 由 Presenter 发布给 View 的显示数据，包含状态、选项和仿真运行标记。 */
struct Robot3dDisplayModel
{
    domain::RobotStateSnapshot state;    ///< 当前机器人状态快照，供界面读取关节、TCP 和坐标系等数据。
    scene::RobotDisplayOptions options;  ///< 当前生效的界面与场景显示开关。
    std::vector<std::string> robotTypes; ///< 可供界面选择的机型标识列表。
    bool simulationRunning = false;      ///< 仿真是否正在运行，默认停止。
};
}
}
