#ifndef SUBSTANCEGEOMETRY_H
#define SUBSTANCEGEOMETRY_H

#include"basegeometry.h"

class SubstanceGeometry:public BaseGeometry
{
public:
   SubstanceGeometry(SubstanceMsg SubstanceMsgIn);
   ~SubstanceGeometry();

   SubstanceGeometry(QVector<SubstanceMsg> substanceMsgsIn,int maxNum=100);
   void SetSubstanceData(QVector<SubstanceMsg> substanceMsgsIn);   //绘制物体的个数不再增删，只修改生成物体的数据
   void SetSubstanceData(SubstanceMsg substanceMsgsIn);
   void AddDeleteSubstanceData(QVector<SubstanceMsg> substanceMsgsIn);   //绘制物体的个数有增删

   virtual bool initShaders();
   virtual void drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);

   SubstanceMsg substanceMsg={ 0,{0,0,0,0,0,0}, 1, 1, 1, 1, QColor(255,0,0)};
   int maxNum=100;
protected:
   void GenerateSubstanceData(SubstanceMsg substanceMsgsIn);
   void GenerateSubstancesData(QVector<SubstanceMsg> substanceMsgsIn);

   void GenerateSphereData(double* centerPose,float radius);//球体
   void GenerateConeData(double* centerPose,float radius,float height);//圆锥体
   void GenerateCylinderData(double* centerPose,float radius,float height);//圆柱体
   void GenerateCubeData(double* centerPose,float length, float width, float height);//立方体

   QMatrix4x4 AxisAngle2RotateMatrix(double* pose);

   FeatureVertexData GetRotatePoint(FeatureVertexData pointData, QMatrix4x4 RotateMatrix, QVector3D center);

   int numVertex=432;  //每个球包含的顶点数
};


#endif // SUBSTANCEGEOMETRY_H
