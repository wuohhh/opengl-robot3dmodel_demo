#pragma once
// Legacy value names for host adapters. New modules use explicit domain types.
#include "robot3d/domain/RobotTypes.h"
#include "robot3d/domain/Kinematics.h"
typedef robot3d::domain::JointPoseDeg JointPose;
typedef robot3d::domain::EulerPoseDeg Pose;
typedef robot3d::domain::DHParameter WholeDH;
#define ROB_AXIS_NUM 6
#define ARM_DOF 6
#define CR6_SAFETY_LIMITS_BOUNDARY_PLANE_NUM 8
inline void CopyPose(Pose p,Pose* out) { if(out) *out=p; }
inline void CopyJointPose(JointPose p,JointPose* out) { if(out) *out=p; }
inline void CopyPoseMsg(Pose p,double* out) { const double a[]={p.X,p.Y,p.Z,p.Rx,p.Ry,p.Rz}; if(out) for(int i=0;i<6;++i) out[i]=a[i]; }
inline void cr_ForwardKineFull(JointPose j,Pose& p,WholeDH* d) { robot3d::domain::forwardKinematics(j,p,d); }
inline void cr_PoseInv(Pose& r,const Pose& p) { robot3d::domain::inversePose(r,p); }
inline void cr_PoseTrans(Pose& r,const Pose& p,const Pose& ref) { robot3d::domain::poseRelativeTo(r,p,ref); }
