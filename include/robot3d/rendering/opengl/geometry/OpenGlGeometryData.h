#pragma once
#include <QVector3D>
#include <QMatrix4x4>
#include <QColor>
#include <QOpenGLBuffer>
#include "robot3d/rendering/SceneUnits.h"
// Descriptions and buffers owned by the OpenGL geometry implementation.
/** @brief 仅包含位置的 OpenGL 顶点，用于线、平面和基础实体。 */
struct OpenGlPositionVertex
{
    QVector3D position; ///< 顶点位置，已由毫米换算为渲染模型单位。
};

/** @brief 用于生成球体、长方体、圆柱和圆锥顶点的工具实体描述。 */
typedef struct OpenGlSolidData
{
    int substanceType;    ///< 几何类型：0 为球体，1 为长方体，2 为圆柱，3 为圆锥。
    double centerPose[6]; ///< 中心位姿 [X, Y, Z, Rx, Ry, Rz]；位置为毫米，轴角旋转向量为度。
    float radius;         ///< 球体半径或圆柱、圆锥底面半径，单位为毫米。
    float length;         ///< 长方体长度，单位为毫米。
    float width;          ///< 长方体宽度，单位为毫米。
    float height;         ///< 长方体、圆柱或圆锥高度，单位为毫米。
    QColor color;         ///< 实体绘制颜色。
} OpenGlSolidData;

/** @brief 球体标记的几何与颜色描述。 */
typedef struct OpenGlSphereData
{
    QVector3D center; ///< 传给球体生成器的中心坐标，生成顶点时按毫米到模型单位的比例换算。
    float radius;     ///< 球体半径，生成顶点前按毫米解释。
    QColor color;     ///< 球体绘制颜色。
} OpenGlSphereData;

/** @brief 以两个端点描述的彩色线段。 */
typedef struct OpenGlLineData
{
    QVector3D startPt; ///< 线段起点，单位为毫米。
    QVector3D endPt;   ///< 线段终点，单位为毫米。
    QColor color;      ///< 线段绘制颜色。
} OpenGlLineData;

/** @brief 单个点标记的位置与颜色描述。 */
typedef struct OpenGlPointData
{
    QVector3D Pt; ///< 点标记的位置坐标。
    QColor color; ///< 点标记绘制颜色。
} OpenGlPointData;

/** @brief 按四个角点生成两个三角形的半透明平面描述。 */
typedef struct OpenGlPlaneData
{
    QVector3D firstPt;  ///< 第一个角点，单位为毫米。
    QVector3D secondPt; ///< 第二个角点，单位为毫米。
    QVector3D thirdPt;  ///< 第三个角点，单位为毫米。
    QVector3D fourPt;   ///< 第四个角点，单位为毫米；与前三点共同决定三角形连接顺序。
    QColor color;       ///< 平面 RGB 颜色，透明度由 alpha 单独传入着色器。
    float alpha;        ///< 片元透明度分量，0 为完全透明，1 为完全不透明。
} OpenGlPlaneData;

/** @brief 机械臂网格上传到 OpenGL 顶点缓冲区的数据布局。 */
struct OpenGlMeshVertex
{
    QVector3D position; ///< 网格顶点位置，使用渲染模型单位。
    QVector3D normal;   ///< 模型顶点法向量，用于光照计算。
    QVector3D material; ///< 顶点所属网格材质的漫反射 RGB 分量。
};

/** @brief 记录关节旋转轴及沿机械臂链累计的变换。 */
struct OpenGlJointTransform
{
    QVector3D center;     ///< 关节旋转轴经过的位置，使用渲染模型单位。
    QVector3D normal;     ///< 关节旋转轴的单位方向向量。
    QMatrix4x4 transform; ///< 当前关节旋转与上游关节变换相乘得到的累计矩阵。
};

/** @brief 由机械臂几何对象管理的 CPU 顶点数组及对应 GPU 缓冲区。 */
struct OpenGlMeshBuffer
{
    OpenGlMeshVertex *verticesData;       ///< 当前姿态下的 CPU 顶点数组，由几何对象分配并释放。
    OpenGlMeshVertex *verticesDataBackup; ///< 初始网格顶点备份，每次关节变换前用于还原当前数组。
    unsigned int numVertices = 0;         ///< 顶点数组长度，也是绘制时传入的顶点数量，初始为 0。
    QOpenGLBuffer VB;                     ///< 与 CPU 顶点数组对应的 OpenGL 顶点缓冲区。
};
