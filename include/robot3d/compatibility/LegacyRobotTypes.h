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
inline void CopyPose(LegacyEulerPose p,LegacyEulerPose* out) { if(out) *out=p; }
inline void CopyJointPose(LegacyJointPose p,LegacyJointPose* out) { if(out) *out=p; }
inline void CopyPoseMsg(LegacyEulerPose p,double* out) { const double a[]={p.X,p.Y,p.Z,p.Rx,p.Ry,p.Rz}; if(out) for(int i=0;i<6;++i) out[i]=a[i]; }
inline void cr_ForwardKineFull(LegacyJointPose j,LegacyEulerPose& p,LegacyDhParameter* d) { robot3d::domain::forwardKinematics(j,p,d); }
inline void cr_PoseInv(LegacyEulerPose& r,const LegacyEulerPose& p) { robot3d::domain::inversePose(r,p); }
inline void cr_PoseTrans(LegacyEulerPose& r,const LegacyEulerPose& p,const LegacyEulerPose& ref) { robot3d::domain::poseRelativeTo(r,p,ref); }

enum LegacyRobotDisplayFlag { show_Null=0,show_CoordSys=1,show_RobotOperate=2,show_Zoom=4,show_PoseOperate=8,
 show_WaypointOperate=16,show_TargetRobot=32,show_SafePlane=64,show_TcpSpeed=128,show_Install=256,show_Tool=512 };
Q_DECLARE_FLAGS(LegacyRobotDisplayFlags,LegacyRobotDisplayFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(LegacyRobotDisplayFlags)

typedef struct LegacyToolShape
{
    int substanceType;//0-球体，1-圆锥体
    double centerPose[6];
    float radius;
    float length;
    float width;
    float height;
    QColor color;
}LegacyToolShape;
