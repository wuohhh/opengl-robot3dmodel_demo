#pragma once
#include <array>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
namespace robot3d
{
namespace domain
{
// Physical units stay in the domain. Rendering alone converts mm to model units.
/** @brief 六轴关节角，按第 1～6 轴排列，单位均为度。 */
struct JointPoseDeg
{
    double L1; ///< 第 1 轴关节角，单位为度。
    double L2; ///< 第 2 轴关节角，单位为度。
    double L3; ///< 第 3 轴关节角，单位为度。
    double L4; ///< 第 4 轴关节角，单位为度。
    double L5; ///< 第 5 轴关节角，单位为度。
    double L6; ///< 第 6 轴关节角，单位为度。
};
/** @brief 笛卡尔位姿：位置单位为 mm，姿态采用 ZYX 顺序的欧拉角，单位为度。 */
struct EulerPoseDeg
{
    double X;  ///< X 方向平移，单位为 mm。
    double Y;  ///< Y 方向平移，单位为 mm。
    double Z;  ///< Z 方向平移，单位为 mm。
    double Rx; ///< 绕 X 轴的欧拉角，单位为度。
    double Ry; ///< 绕 Y 轴的欧拉角，单位为度。
    double Rz; ///< 绕 Z 轴的欧拉角，单位为度。
};
/** @brief 笛卡尔位置与轴角旋转向量；位置单位为 mm，旋转向量模长为转角（弧度）。 */
struct AxisAngleRad
{
    double X;  ///< X 方向平移，单位为 mm。
    double Y;  ///< Y 方向平移，单位为 mm。
    double Z;  ///< Z 方向平移，单位为 mm。
    double Rx; ///< 轴角旋转向量的 X 分量，单位为弧度。
    double Ry; ///< 轴角旋转向量的 Y 分量，单位为弧度。
    double Rz; ///< 轴角旋转向量的 Z 分量，单位为弧度。
};
/** @brief 单轴 DH 几何参数；角度使用弧度，距离使用 mm。 */
struct DHParameter
{
    double alpha; ///< 连杆扭转角，单位为弧度。
    double a;     ///< 连杆长度，单位为 mm。
    double d;     ///< 沿关节轴的连杆偏距，单位为 mm。
    double theta; ///< 关节零位角偏置，正解时叠加关节角，单位为弧度。
    double beta;  ///< 配置中保留的附加角参数，单位为弧度；当前标准 DH 正解未使用。
};
/** @brief 供界面选择的命名坐标系及其定义位姿。 */
struct CoordinateSystem
{
    std::string id;    ///< 坐标系标识，如 b_0、t_0、p_0 或 wcs_0。
    std::string name;  ///< 用于界面显示的坐标系名称。
    EulerPoseDeg pose; ///< 坐标系定义位姿，平移为 mm，ZYX 欧拉角为度。
};
/** @brief 机型标识、模型资源路径和六轴运动学参数的配置。 */
struct RobotConfiguration
{
    std::string id;                ///< 机型标识，对应机型配置目录名。
    std::string modelDirectory;    ///< 当前机型的三维模型资源目录。
    std::array<DHParameter, 6> dh; ///< 按第 1～6 轴顺序保存的 DH 参数。
};
typedef std::shared_ptr<const RobotConfiguration> RobotConfigurationPtr;
inline std::array<double, 6> jointValues(const JointPoseDeg &j)
{
    return {{j.L1, j.L2, j.L3, j.L4, j.L5, j.L6}};
}
inline JointPoseDeg jointPose(const std::array<double, 6> &a)
{
    JointPoseDeg j = {a[0], a[1], a[2], a[3], a[4], a[5]};
    return j;
}
/** @brief 应用层发布的机器人状态快照，使用领域层的 mm/deg 物理单位。 */
struct RobotStateSnapshot
{
    RobotConfigurationPtr configuration;       ///< 当前机型的只读共享配置；尚未载入机型时为空。
    JointPoseDeg joints = {};                  ///< 当前六轴关节角，单位为度，初始为零。
    EulerPoseDeg tcp = {};                     ///< TCP 在机器人基坐标系中的位姿，单位为 mm/deg。
    EulerPoseDeg baseInWorld = {};             ///< 机器人基坐标系在世界坐标系中的位姿，单位为 mm/deg。
    EulerPoseDeg coordinatePose = {};          ///< 当前所选坐标系用于显示的位姿，单位为 mm/deg。
    std::string coordinateId = "b_0";          ///< 当前选择的坐标系标识，默认选择基坐标系。
    std::vector<CoordinateSystem> coordinates; ///< 可供选择的坐标系定义列表。
    std::vector<EulerPoseDeg> waypoints;       ///< 已记录的 TCP 路点位姿列表，单位为 mm/deg。
    std::vector<EulerPoseDeg> trajectory;      ///< 仿真采样形成的 TCP 轨迹位姿列表，单位为 mm/deg。
    double simulatedTcpSpeed = 0;              ///< 演示用 TCP 线速度，单位为 mm/s，并非设备实测速度。
    std::uint64_t revision = 0;                ///< 状态修订号，用于标识应用层发布的状态变更。
};
// The original UI uses demonstration limits. These are not hardware configuration.
inline double demoJointLimit(int axis)
{
    return axis == 2 ? 165.0 : 360.0;
}
}
}
