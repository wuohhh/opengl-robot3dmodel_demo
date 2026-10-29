#ifndef ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLPLANEGEOMETRY_H
#define ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLPLANEGEOMETRY_H

#include "robot3d/rendering/opengl/geometry/OpenGlGeometryBase.h"
class OpenGlPlaneGeometry : public OpenGlGeometryBase
{
public:
    OpenGlPlaneGeometry(OpenGlPlaneData planeMsgIn);
    OpenGlPlaneGeometry(QVector<OpenGlPlaneData> planeMsgsIn, int numMax = 100);
    ~OpenGlPlaneGeometry();

    void SetPlaneData(OpenGlPlaneData planeMsgIn); //按照逆时针顺序输入平面的四个顶点
    void SetPlaneData(QVector<OpenGlPlaneData>
                          planeMsgsIn); //平面数量不变 可修改组成平面的顶点数据    按照逆时针顺序输入平面的四个顶点
    void AddDeletePlaneData(QVector<OpenGlPlaneData> planeMsgsIn); //平面数量有增删  按照逆时针顺序输入平面的四个顶点

    virtual bool initShaders();
    virtual void drawGeometry(QMatrix4x4 model, QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);

    OpenGlPlaneData planeMsg;
    QVector<OpenGlPlaneData> planeMsgs;

    int maxNum = 100;

protected:
    void GeneratePlaneData(OpenGlPlaneData planeMsgIn);
    void GeneratePlanesData(QVector<OpenGlPlaneData> planeMsgsIn);

    int numVertex = 4; //每个平面包含的顶点数
};

#endif // ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLPLANEGEOMETRY_H
