#pragma once
#include <QMatrix4x4>
#include "robot3d/scene/RobotSceneSnapshot.h"
#include "robot3d/rendering/CameraState.h"
#include "robot3d/rendering/SceneUnits.h"
namespace robot3d { namespace rendering { namespace vtkmath {
const double scale = kMillimetersToModelUnits;
inline QMatrix4x4 poseMatrix(const scene::LegacyAxisAngleDeg& p) {
    QMatrix4x4 m;
    const QVector3D v(p.Rx,p.Ry,p.Rz);
    if(v.length()>1e-8f) m.rotate(v.length(),v.normalized());
    m.setColumn(3,QVector4D(p.X*scale,p.Y*scale,p.Z*scale,1));
    return m;
}
inline std::array<QMatrix4x4,7> linkMatrices(const domain::RobotConfiguration& c,const domain::JointPoseDeg& pose) {
    const auto& d=c.dh;
    const QVector3D centers[6]={
        {0,0,0},{0,0,float(d[0].d)},{0,0,float(d[0].d+d[2].a)},
        {0,0,float(d[0].d+d[2].a+d[3].a)},{0,float(-d[3].d),0},
        {0,0,float(d[0].d+d[2].a+d[3].a+d[4].d)}};
    const QVector3D axes[6]={{0,0,1},{0,-1,0},{0,-1,0},{0,-1,0},{0,0,1},{0,-1,0}};
    const auto values=domain::jointValues(pose);
    std::array<QMatrix4x4,7> result;
    for(int i=0;i<6;++i) {
        QMatrix4x4 local;
        local.translate(centers[i]*scale); local.rotate(values[i],axes[i]); local.translate(-centers[i]*scale);
        result[i+1]=result[i]*local;
    }
    return result;
}
inline QMatrix4x4 sceneMatrix(const scene::RobotSceneSnapshot& scene,const CameraState& camera) {
    const auto coordinate=scene.options.install?QMatrix4x4():poseMatrix(scene.coordinatePose);
    QMatrix4x4 m; m.scale(camera.scale); m=coordinate.inverted()*m;
    m.translate(QVector3D(coordinate*QVector4D(camera.translation,0))/camera.scale);
    m.translate(camera.rotationCenter);
    auto rotation=camera.rotation;
    if(scene.options.install) rotation=rotation*QQuaternion::fromEulerAngles(0,-scene.installationTiltDeg,-scene.installationBaseDeg);
    m.rotate(rotation); m.translate(-camera.rotationCenter); return m;
}
}}}
