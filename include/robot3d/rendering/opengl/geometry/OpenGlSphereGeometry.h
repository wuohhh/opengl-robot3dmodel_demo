#ifndef ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLSPHEREGEOMETRY_H
#define ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLSPHEREGEOMETRY_H

#include "robot3d/rendering/opengl/geometry/OpenGlGeometryBase.h"

class OpenGlSphereGeometry : public OpenGlGeometryBase
{
public:
    OpenGlSphereGeometry(OpenGlSphereData sphereMsgIn);
    ~OpenGlSphereGeometry();

    OpenGlSphereGeometry(QVector<OpenGlSphereData> sphereMsgsIn, int maxNum = 100);
    void SetSphereData(QVector<OpenGlSphereData> sphereMsgsIn); //绘制球的个数不再增删，只修改生成球的数据
    void SetSphereData(QVector3D center, float radius = 20.0);
    void AddDeleteSphereData(QVector<OpenGlSphereData> sphereMsgsIn); //绘制球的个数有增删

    virtual bool initShaders();
    virtual void drawGeometry(QMatrix4x4 model, QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);

    OpenGlSphereData sphereMsg = {QVector3D(0, 0, 0), 1, QColor(255, 0, 0)};
    int maxNum = 100;

protected:
    void GenerateSphereData(QVector3D center, float radius);
    void GenerateSpheresData(QVector<OpenGlSphereData> sphereMsgsIn);

    int numVertex = 432; //每个球包含的顶点数
                         //QVector<OpenGlSphereData> sphereMsgs;
};

#endif // ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLSPHEREGEOMETRY_H
