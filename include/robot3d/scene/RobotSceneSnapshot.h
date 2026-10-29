#pragma once
#include "robot3d/domain/RobotTypes.h"
#include "robot3d/scene/RobotDisplayOptions.h"
namespace robot3d
{
namespace scene
{
/** @brief 兼容旧视口的显示位姿：位置为 mm，姿态按以度表示的轴角旋转向量解释。 */
struct LegacyAxisAngleDeg
{
    double X;  ///< X 方向平移，单位为 mm。
    double Y;  ///< Y 方向平移，单位为 mm。
    double Z;  ///< Z 方向平移，单位为 mm。
    double Rx; ///< 轴角旋转向量的 X 分量，单位为度。
    double Ry; ///< 轴角旋转向量的 Y 分量，单位为度。
    double Rz; ///< 轴角旋转向量的 Z 分量，单位为度。
};
inline LegacyAxisAngleDeg legacyDisplayPose(const domain::EulerPoseDeg &p)
{
    LegacyAxisAngleDeg r = {p.X, p.Y, p.Z, p.Rx, p.Ry, p.Rz};
    return r;
}
/** @brief 供各渲染后端绘制的工具几何体描述，长度参数使用 mm。 */
struct ToolShape
{
    int type = 0;                             ///< 几何类型：0 球体、1 长方体、2 圆柱体、3 圆锥体。
    LegacyAxisAngleDeg pose = {};             ///< 工具显示位姿，位置为 mm，旋转沿用旧视口的度制轴角约定。
    float radius = 1;                         ///< 球体、圆柱体或圆锥体的半径，单位为 mm。
    float length = 1;                         ///< 长方体的 X 方向长度，单位为 mm。
    float width = 1;                          ///< 长方体的 Y 方向宽度，单位为 mm。
    float height = 1;                         ///< 长方体、圆柱体或圆锥体的高度，单位为 mm。
    std::array<int, 3> rgb = {{50, 85, 182}}; ///< RGB 三通道颜色，按 0～255 表示。
};
/** @brief 由四个角点描述的安全边界平面及其显示样式。 */
struct SafePlane
{
    std::array<std::array<double, 3>, 4> corners; ///< 按边界连接顺序排列的四个 XYZ 角点，单位为 mm。
    float alpha = 0;                              ///< 不透明度，0 表示完全透明，1 表示完全不透明。
    std::array<int, 3> rgb = {{50, 182, 122}};    ///< RGB 三通道颜色，按 0～255 表示。
};
/** @brief 交给渲染后端的完整场景快照，合并机器人状态、显示选项和附加几何体。 */
struct RobotSceneSnapshot
{
    domain::RobotStateSnapshot state;       ///< 机器人领域状态，包含当前关节、TCP、路点和轨迹。
    RobotDisplayOptions options;            ///< 面板、场景对象和安装姿态模式的显示开关。
    LegacyAxisAngleDeg coordinatePose = {}; ///< 所选坐标系的显示位姿，沿用旧视口的度制轴角约定。
    std::vector<ToolShape> tools;           ///< 工具几何体列表；为空时渲染器使用当前 TCP 处的默认工具。
    std::vector<SafePlane> planes;          ///< 安全边界平面列表。
    domain::JointPoseDeg targetJoints = {}; ///< 目标机器人模型的六轴关节角，单位为度。
    double installationTiltDeg = 0;         ///< 安装姿态的倾斜角，单位为度。
    double installationBaseDeg = 0;         ///< 安装姿态的基座旋转角，单位为度。
};
}
}
