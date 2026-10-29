#pragma once
#include <memory>
#include <vector>
#include <QOpenGLFunctions>
#include "robot3d/scene/RobotSceneSnapshot.h"
#include "robot3d/rendering/CameraState.h"

class OpenGlRobotGeometry;
class OpenGlLineGeometry;
class OpenGlSphereGeometry;
class OpenGlPlaneGeometry;
class OpenGlSolidGeometry;
namespace robot3d
{
namespace rendering
{
class OpenGlRobotSceneRenderer : protected QOpenGLFunctions
{
public:
    OpenGlRobotSceneRenderer();
    ~OpenGlRobotSceneRenderer();
    bool prepareModel(domain::RobotConfigurationPtr config, QString &error);
    void apply(const scene::RobotSceneSnapshot &scene);
    void draw(const QMatrix4x4 &projection, const CameraState &camera);
    QVector3D rotationCenter() const;
    QMatrix4x4 coordinateMatrix() const;

private:
    std::unique_ptr<OpenGlRobotGeometry> robot_, target_;
    std::unique_ptr<OpenGlSphereGeometry> tcp_, waypoints_;
    std::unique_ptr<OpenGlLineGeometry> trajectory_;
    std::unique_ptr<OpenGlSolidGeometry> tool_;
    std::vector<std::unique_ptr<OpenGlLineGeometry>> axes_, toolAxes_;
    std::vector<std::unique_ptr<OpenGlPlaneGeometry>> planes_;
    scene::RobotSceneSnapshot scene_;
    QMatrix4x4 coordinate_;
};
QMatrix4x4 legacyAxisAngleMatrix(const scene::LegacyAxisAngleDeg &pose);
}
}
