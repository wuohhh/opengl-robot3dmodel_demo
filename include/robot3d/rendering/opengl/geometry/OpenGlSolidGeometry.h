#ifndef ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLSOLIDGEOMETRY_H
#define ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLSOLIDGEOMETRY_H

#include"robot3d/rendering/opengl/geometry/OpenGlGeometryBase.h"

class OpenGlSolidGeometry:public OpenGlGeometryBase
{
public:
   OpenGlSolidGeometry(OpenGlSolidData SubstanceMsgIn);
   ~OpenGlSolidGeometry();

   OpenGlSolidGeometry(QVector<OpenGlSolidData> substanceMsgsIn,int maxNum=100);
   void SetSubstanceData(QVector<OpenGlSolidData> substanceMsgsIn);   //绘制物体的个数不再增删，只修改生成物体的数据
   void SetSubstanceData(OpenGlSolidData substanceMsgsIn);
   void AddDeleteSubstanceData(QVector<OpenGlSolidData> substanceMsgsIn);   //绘制物体的个数有增删

   virtual bool initShaders();
   virtual void drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);

   OpenGlSolidData substanceMsg={ 0,{0,0,0,0,0,0}, 1, 1, 1, 1, QColor(255,0,0)};
   int maxNum=100;
protected:
   void GenerateSubstanceData(OpenGlSolidData substanceMsgsIn);
   void GenerateSubstancesData(QVector<OpenGlSolidData> substanceMsgsIn);

   void GenerateSphereData(double* centerPose,float radius);//球体
   void GenerateConeData(double* centerPose,float radius,float height);//圆锥体
   void GenerateCylinderData(double* centerPose,float radius,float height);//圆柱体
   void GenerateCubeData(double* centerPose,float length, float width, float height);//立方体

   QMatrix4x4 AxisAngle2RotateMatrix(double* pose);

   OpenGlPositionVertex GetRotatePoint(OpenGlPositionVertex pointData, QMatrix4x4 RotateMatrix, QVector3D center);

   int numVertex=432;  //每个球包含的顶点数
};


#endif // ROBOT3D_RENDERING_OPENGL_GEOMETRY_OPENGLSOLIDGEOMETRY_H
