#pragma once
#include <memory>
#include <vector>
#include <QOpenGLFunctions>
#include "robot3d/scene/RobotSceneSnapshot.h"
#include "robot3d/rendering/CameraState.h"





class RobotGeometry; class LineGeometry; class SphereGeometry; class PlaneGeometry; class SubstanceGeometry;
namespace robot3d { namespace rendering {
class RobotSceneRenderer : protected QOpenGLFunctions {
public:
    RobotSceneRenderer();
    ~RobotSceneRenderer();
    bool prepareModel(domain::RobotConfigurationPtr config,QString& error);
    void apply(const scene::RobotSceneSnapshot& scene);
    void draw(const QMatrix4x4& projection,const CameraState& camera);
    QVector3D rotationCenter() const;
    QMatrix4x4 coordinateMatrix() const;
private:
    std::unique_ptr<RobotGeometry> robot_,target_;
    std::unique_ptr<SphereGeometry> tcp_,waypoints_;
    std::unique_ptr<LineGeometry> trajectory_;
    std::unique_ptr<SubstanceGeometry> tool_;
    std::vector<std::unique_ptr<LineGeometry>> axes_,toolAxes_;
    std::vector<std::unique_ptr<PlaneGeometry>> planes_;
    scene::RobotSceneSnapshot scene_;
    QMatrix4x4 coordinate_;
};
QMatrix4x4 legacyAxisAngleMatrix(const scene::LegacyAxisAngleDeg& pose);
}}
