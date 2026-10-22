#pragma once
namespace robot3d { namespace scene {
struct RobotDisplayOptions {
    bool coordinatePanel=true, operatePanel=true, zoomPanel=true, posePanel=false, waypointPanel=true, speedPanel=true;
    bool coordinateAxes=true, waypoints=false, trajectory=false, tool=false, endWorkpiece=false, targetRobot=false, safePlanes=false, install=false;
};
}}
