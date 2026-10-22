#ifndef ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLLINEGEOMETRY_H
#define ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLLINEGEOMETRY_H

#include"robot3d/rendering/opengl/geometry/OpenGlGeometryBase.h"

class OpenGlLineGeometry:public OpenGlGeometryBase
{
public:
    OpenGlLineGeometry(OpenGlLineData lineMsgIn);
    OpenGlLineGeometry(QVector<QVector3D> points, QColor color,int maxNum=1000);
    ~OpenGlLineGeometry();
    void SetLineData(QVector3D startPt, QVector3D endPt);
    void SetLineData(QVector<QVector3D> points);         //绘制线的顶点数不再增删，只修改线段组成的顶点
    void AddDeleteLineData(QVector<QVector3D> points);   //绘制线段的顶点数有增删

    virtual bool initShaders();
    virtual void drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);

    OpenGlLineData lineMsg;
    QVector<QVector3D> pointList;
    int maxNum=1000;
protected:
   void GenerateLineData(QVector3D pointStart,QVector3D pointEnd);
   void GenerateLinesData(QVector<QVector3D> points);

  // int numVertex=1;  //点的个数
};

#endif // ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLLINEGEOMETRY_H
