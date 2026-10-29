#pragma once
// Legacy value names for host adapters. New modules use explicit domain types.
#include "robot3d/domain/RobotTypes.h"
#include "robot3d/domain/Kinematics.h"
#include <QColor>
#include <QFlags>
typedef robot3d::domain::JointPoseDeg LegacyJointPose;
typedef robot3d::domain::EulerPoseDeg LegacyEulerPose;
typedef robot3d::domain::DHParameter LegacyDhParameter;
#define ROB_AXIS_NUM 6
#define ARM_DOF 6
#define CR6_SAFETY_LIMITS_BOUNDARY_PLANE_NUM 8
inline void CopyPose(LegacyEulerPose p, LegacyEulerPose *out)
{
    if (out)
        *out = p;
}
inline void CopyJointPose(LegacyJointPose p, LegacyJointPose *out)
{
    if (out)
        *out = p;
}
inline void CopyPoseMsg(LegacyEulerPose p, double *out)
{
    const double a[] = {p.X, p.Y, p.Z, p.Rx, p.Ry, p.Rz};
    if (out)
        for (int i = 0; i < 6; ++i)
            out[i] = a[i];
}
inline void cr_ForwardKineFull(LegacyJointPose j, LegacyEulerPose &p, LegacyDhParameter *d)
{
    robot3d::domain::forwardKinematics(j, p, d);
}
inline void cr_PoseInv(LegacyEulerPose &r, const LegacyEulerPose &p)
{
    robot3d::domain::inversePose(r, p);
}
inline void cr_PoseTrans(LegacyEulerPose &r, const LegacyEulerPose &p, const LegacyEulerPose &ref)
{
    robot3d::domain::poseRelativeTo(r, p, ref);
}

/** @brief 旧宿主使用的显示位掩码，可通过 LegacyRobotDisplayFlags 组合多个选项。 */
enum LegacyRobotDisplayFlag
{
    show_Null = 0,             ///< 不启用任何显示标志。
    show_CoordSys = 1,         ///< 显示坐标系选择面板及坐标轴。
    show_RobotOperate = 2,     ///< 显示视图操作面板，含复位、旋转/平移切换和末端工件显隐。
    show_Zoom = 4,             ///< 显示视图缩放面板。
    show_PoseOperate = 8,      ///< 显示位姿操作面板。
    show_WaypointOperate = 16, ///< 显示路点和轨迹操作面板。
    show_TargetRobot = 32,     ///< 显示目标机器人模型。
    show_SafePlane = 64,       ///< 显示安全边界平面。
    show_TcpSpeed = 128,       ///< 显示 TCP 速度面板。
    show_Install = 256,        ///< 启用安装姿态显示模式。
    show_Tool = 512            ///< 显示工具几何体及工具坐标轴。
};
Q_DECLARE_FLAGS(LegacyRobotDisplayFlags, LegacyRobotDisplayFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(LegacyRobotDisplayFlags)

/** @brief 旧宿主传入的工具形状描述，转换为场景层 ToolShape 后交给渲染器。 */
typedef struct LegacyToolShape
{
    int substanceType;    ///< 几何类型：0 球体、1 长方体、2 圆柱体、3 圆锥体。
    double centerPose[6]; ///< 依次为 X/Y/Z（mm）与 Rx/Ry/Rz（度制轴角旋转向量）的显示位姿。
    float radius;         ///< 球体、圆柱体或圆锥体的半径，单位为 mm。
    float length;         ///< 长方体的 X 方向长度，单位为 mm。
    float width;          ///< 长方体的 Y 方向宽度，单位为 mm。
    float height;         ///< 长方体、圆柱体或圆锥体的高度，单位为 mm。
    QColor color;         ///< 工具几何体的显示颜色。
} LegacyToolShape;
