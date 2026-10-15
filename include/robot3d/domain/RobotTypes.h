#pragma once
#include <array>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
namespace robot3d { namespace domain {
// Physical units stay in the domain. Rendering alone converts mm to model units.
struct JointPoseDeg { double L1,L2,L3,L4,L5,L6; };
struct EulerPoseDeg { double X,Y,Z,Rx,Ry,Rz; }; // ZYX Euler angles, degrees; XYZ in mm.
struct AxisAngleRad { double X,Y,Z,Rx,Ry,Rz; }; // Rotation vector, radians.
struct DHParameter { double alpha,a,d,theta,beta; }; // Angles in radians; a/d in mm.
struct CoordinateSystem { std::string id,name; EulerPoseDeg pose; };
struct RobotConfiguration {
    std::string id, modelDirectory;
    std::array<DHParameter,6> dh;
};
typedef std::shared_ptr<const RobotConfiguration> RobotConfigurationPtr;
inline std::array<double,6> jointValues(const JointPoseDeg& j) { return {{j.L1,j.L2,j.L3,j.L4,j.L5,j.L6}}; }
inline JointPoseDeg jointPose(const std::array<double,6>& a) { JointPoseDeg j={a[0],a[1],a[2],a[3],a[4],a[5]}; return j; }
struct RobotStateSnapshot {
    RobotConfigurationPtr configuration;
    JointPoseDeg joints = {};
    EulerPoseDeg tcp = {}, baseInWorld = {}, coordinatePose = {};
    std::string coordinateId = "b_0";
    std::vector<CoordinateSystem> coordinates;
    std::vector<EulerPoseDeg> waypoints, trajectory;
    double simulatedTcpSpeed = 0;
    std::uint64_t revision = 0;
};
// The original UI uses demonstration limits. These are not hardware configuration.
inline double demoJointLimit(int axis) { return axis==2 ? 165.0 : 360.0; }
class TrajectorySampler {
public:
    explicit TrajectorySampler(std::size_t capacity=1000) : capacity_(capacity) {}
    bool append(const EulerPoseDeg& pose, std::vector<EulerPoseDeg>& points) const;
private: std::size_t capacity_;
};
}}
