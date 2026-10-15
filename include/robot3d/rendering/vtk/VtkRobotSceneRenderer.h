#pragma once
#include <memory>
#include <QString>
#include "robot3d/scene/RobotSceneSnapshot.h"
#include "robot3d/rendering/CameraState.h"
class vtkRenderer;
namespace robot3d { namespace rendering {
// VTK objects remain private; both backends consume the same scene contract.
class VtkRobotSceneRenderer {
public:
    explicit VtkRobotSceneRenderer(vtkRenderer* renderer);
    ~VtkRobotSceneRenderer();
    bool prepareModel(domain::RobotConfigurationPtr configuration, QString& error);
    void apply(const scene::RobotSceneSnapshot& scene);
    void updateCamera(const CameraState& camera);
    QMatrix4x4 coordinateMatrix() const;
    QVector3D rotationCenter() const;
    QMatrix4x4 linkMatrix(int index) const;
    int triangleCount() const;
private:
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
};
}}
