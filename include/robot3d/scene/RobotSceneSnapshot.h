#pragma once
#include "robot3d/domain/RobotTypes.h"
namespace robot3d { namespace scene {
struct RobotDisplayOptions {
    bool coordinatePanel=true, operatePanel=true, zoomPanel=true, posePanel=false, waypointPanel=true, speedPanel=true;
    bool coordinateAxes=true, waypoints=false, trajectory=false, tool=false, endWorkpiece=false, targetRobot=false, safePlanes=false, install=false;
};
// The old viewport interprets the three rotation components as an axis-angle vector
// in degrees. Keep that display convention explicit until a separately verified correction.
struct LegacyAxisAngleDeg { double X,Y,Z,Rx,Ry,Rz; };
inline LegacyAxisAngleDeg legacyDisplayPose(const domain::EulerPoseDeg& p) { LegacyAxisAngleDeg r={p.X,p.Y,p.Z,p.Rx,p.Ry,p.Rz}; return r; }
struct ToolShape { int type=0; LegacyAxisAngleDeg pose={}; float radius=1,length=1,width=1,height=1; std::array<int,3> rgb={{50,85,182}}; };
struct SafePlane { std::array<std::array<double,3>,4> corners; float alpha=0; std::array<int,3> rgb={{50,182,122}}; };
struct RobotSceneSnapshot {
    domain::RobotStateSnapshot state;
    RobotDisplayOptions options;
    LegacyAxisAngleDeg coordinatePose={};
    std::vector<ToolShape> tools;
    std::vector<SafePlane> planes;
    domain::JointPoseDeg targetJoints={};
    double installationTiltDeg=0,installationBaseDeg=0;
};
}}
