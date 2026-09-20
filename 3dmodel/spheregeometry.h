#ifndef SPHEREGEOMETRY_H
#define SPHEREGEOMETRY_H

#include"basegeometry.h"

class SphereGeometry:public BaseGeometry
{
public:
   SphereGeometry(SphereMsg sphereMsgIn);
   ~SphereGeometry();

   SphereGeometry(QVector<SphereMsg> sphereMsgsIn,int maxNum=100);
   void SetSphereData(QVector<SphereMsg> sphereMsgsIn);   //绘制球的个数不再增删，只修改生成球的数据
   void SetSphereData(QVector3D center,float radius=20.0);
   void AddDeleteSphereData(QVector<SphereMsg> sphereMsgsIn);   //绘制球的个数有增删

   virtual bool initShaders();
   virtual void drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);

   SphereMsg sphereMsg={ QVector3D(0,0,0), 1, QColor(255,0,0)};
   int maxNum=100;
protected:
   void GenerateSphereData(QVector3D center,float radius);
   void GenerateSpheresData(QVector<SphereMsg> sphereMsgsIn);

   int numVertex=432;  //每个球包含的顶点数
   //QVector<SphereMsg> sphereMsgs;
};

#endif // SPHEREGEOMETRY_H
