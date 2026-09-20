#ifndef BASEGEOMETRY_H
#define BASEGEOMETRY_H

#include <QOpenGLFunctions>
#include<QVector2D>
#include<QVector3D>
#include<QVector>
#include<QOpenGLBuffer>
#include<QOpenGLShaderProgram>
#include<qmath.h>
#include<QColor>
#include<QDebug>


#define G_SCALE 0.04

extern int g_openglVersion;

struct FeatureVertexData
{
    QVector3D position;
};

typedef struct SubstanceMsg
{
    int substanceType;//0-球体，1-圆锥体
    double centerPose[6];
    float radius;
    float length;
    float width;
    float height;
    QColor color;
}SubstanceMsg;

typedef struct SphereMsg
{
    QVector3D center;
    float radius;
    QColor color;
} SphereMsg;

typedef struct LineMsg
{
    QVector3D startPt;
    QVector3D endPt;
    QColor color;
} LineMsg;

typedef struct PointMsg
{
    QVector3D Pt;
    QColor color;
} PointMsg;

typedef struct PlaneMsg
{
    QVector3D firstPt;
    QVector3D secondPt;
    QVector3D thirdPt;
    QVector3D fourPt;
    QColor color;
    float alpha;
} PlaneMsg;

struct VertexData
{
    QVector3D position;
    QVector3D normal;
    QVector3D material;
};

struct multiJointData
{
    QVector3D center;
    QVector3D normal;
    QMatrix4x4 transform;
};

struct MeshEntry
{
    VertexData *verticesData;
    VertexData *verticesDataBackup;   //顶点数据备份
    ushort numVertices=0;
    QOpenGLBuffer VB;
};


class BaseGeometry : public QOpenGLFunctions
{
 public:
    BaseGeometry();
    virtual ~BaseGeometry();
    virtual void drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);
    virtual bool initShaders();

    QVector<FeatureVertexData> vertex_data;     //存放顶点数据
    QVector<GLuint> vertex_index;      //存放索引数据
    QOpenGLBuffer vertex_data_Buf;             //顶点buffer
    QOpenGLBuffer vertex_index_Buf;              //索引buffer
    int num_vertex_data;                         //顶点数据的数量
    int num_vertex_index;                       //索引的数量

    QOpenGLShaderProgram program;


};

#endif // BASEGEOMETRY_H
