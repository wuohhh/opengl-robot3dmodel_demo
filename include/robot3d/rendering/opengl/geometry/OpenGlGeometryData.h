#pragma once
#include <QVector3D>
#include <QMatrix4x4>
#include <QColor>
#include <QOpenGLBuffer>
#include "robot3d/rendering/SceneUnits.h"
// Descriptions and buffers owned by the OpenGL geometry implementation.
struct OpenGlPositionVertex
{
    QVector3D position;
};

typedef struct OpenGlSolidData
{
    int substanceType;//0-球体，1-圆锥体
    double centerPose[6];
    float radius;
    float length;
    float width;
    float height;
    QColor color;
}OpenGlSolidData;

typedef struct OpenGlSphereData
{
    QVector3D center;
    float radius;
    QColor color;
} OpenGlSphereData;

typedef struct OpenGlLineData
{
    QVector3D startPt;
    QVector3D endPt;
    QColor color;
} OpenGlLineData;

typedef struct OpenGlPointData
{
    QVector3D Pt;
    QColor color;
} OpenGlPointData;

typedef struct OpenGlPlaneData
{
    QVector3D firstPt;
    QVector3D secondPt;
    QVector3D thirdPt;
    QVector3D fourPt;
    QColor color;
    float alpha;
} OpenGlPlaneData;

struct OpenGlMeshVertex
{
    QVector3D position;
    QVector3D normal;
    QVector3D material;
};

struct OpenGlJointTransform
{
    QVector3D center;
    QVector3D normal;
    QMatrix4x4 transform;
};

struct OpenGlMeshBuffer
{
    OpenGlMeshVertex *verticesData;
    OpenGlMeshVertex *verticesDataBackup;   //顶点数据备份
    unsigned int numVertices=0;
    QOpenGLBuffer VB;
};
