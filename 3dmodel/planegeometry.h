#ifndef PLANEGEOMETRY_H
#define PLANEGEOMETRY_H

#include"basegeometry.h"
class PlaneGeometry:public BaseGeometry
{
public:
    PlaneGeometry(PlaneMsg planeMsgIn);
    PlaneGeometry(QVector<PlaneMsg> planeMsgsIn,int numMax=100);
    ~PlaneGeometry();

    void SetPlaneData(PlaneMsg planeMsgIn);   //按照逆时针顺序输入平面的四个顶点
    void SetPlaneData(QVector<PlaneMsg> planeMsgsIn);  //平面数量不变 可修改组成平面的顶点数据    按照逆时针顺序输入平面的四个顶点
    void AddDeletePlaneData(QVector<PlaneMsg> planeMsgsIn);  //平面数量有增删  按照逆时针顺序输入平面的四个顶点

    virtual bool initShaders();
    virtual void drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);

    PlaneMsg planeMsg;
    QVector<PlaneMsg> planeMsgs;

    int maxNum=100;

protected:
   void GeneratePlaneData(PlaneMsg planeMsgIn);
   void GeneratePlanesData(QVector<PlaneMsg> planeMsgsIn);

   int numVertex=4;  //每个平面包含的顶点数
};

#endif // PLANEGEOMETRY_H
