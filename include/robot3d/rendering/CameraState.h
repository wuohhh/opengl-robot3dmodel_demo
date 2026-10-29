#pragma once
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
namespace robot3d
{
namespace rendering
{
/** @brief 保存视图的坐标基、交互旋转、平移和缩放状态。 */
struct CameraState
{
    QMatrix4x4 coordinateMatrix; ///< 当前显示坐标系的变换矩阵，用于换算交互旋转轴和平移方向。
    QQuaternion rotation;        ///< 用户交互累积的视图旋转四元数。
    QVector3D translation;       ///< 视图平移量，绘制时按当前显示坐标基进行变换。
    QVector3D rotationCenter;    ///< 交互旋转的枢轴位置，使用渲染模型单位。
    double scale = 1;            ///< 无量纲视图缩放倍率，默认保持原始比例。
};
}
}
