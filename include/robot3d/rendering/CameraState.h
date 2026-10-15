#pragma once
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
namespace robot3d { namespace rendering {
struct CameraState {
    QMatrix4x4 coordinateMatrix;
    QQuaternion rotation;
    QVector3D translation, rotationCenter;
    double scale=1;
};
}}
