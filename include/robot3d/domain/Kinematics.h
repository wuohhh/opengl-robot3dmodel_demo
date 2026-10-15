#pragma once
#include "robot3d/domain/RobotTypes.h"
namespace robot3d { namespace domain {
void forwardKinematics(JointPoseDeg joints, EulerPoseDeg& pose, const DHParameter* dh);
void poseRelativeTo(EulerPoseDeg& result, const EulerPoseDeg& pose1, const EulerPoseDeg& pose2);
void inversePose(EulerPoseDeg& result, const EulerPoseDeg& pose);
void eulerToAxisAngle(EulerPoseDeg source, AxisAngleRad& result);
void axisAngleToEuler(AxisAngleRad source, EulerPoseDeg& result);
}}
