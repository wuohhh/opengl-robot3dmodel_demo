#ifndef ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLGEOMETRYBASE_H
#define ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLGEOMETRYBASE_H

#include <QOpenGLFunctions>
#include <QVector2D>
#include <QVector3D>
#include <QVector>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <qmath.h>
#include <QColor>
#include <QDebug>

#include "robot3d/rendering/opengl/geometry/OpenGlGeometryData.h"

class OpenGlGeometryBase : public QOpenGLFunctions
{
protected:
    int m_glMajor = 2;
    void uploadVertices();

public:
    OpenGlGeometryBase();
    virtual ~OpenGlGeometryBase();
    virtual void drawGeometry(QMatrix4x4 model, QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);
    virtual bool initShaders();

    QVector<OpenGlPositionVertex> vertex_data; //存放顶点数据
    QVector<GLuint> vertex_index;              //存放索引数据
    QOpenGLBuffer vertex_data_Buf;             //顶点buffer
    QOpenGLBuffer vertex_index_Buf;            //索引buffer
    int num_vertex_data;                       //顶点数据的数量
    int num_vertex_index;                      //索引的数量

    QOpenGLShaderProgram program;
};

#endif // ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLGEOMETRYBASE_H
