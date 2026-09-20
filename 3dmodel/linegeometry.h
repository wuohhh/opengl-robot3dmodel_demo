#ifndef LINEGEOMETRY_H
#define LINEGEOMETRY_H

#include"basegeometry.h"

class LineGeometry:public BaseGeometry
{
public:
    LineGeometry(LineMsg lineMsgIn);
    LineGeometry(QVector<QVector3D> points, QColor color,int maxNum=1000);
    ~LineGeometry();
    void SetLineData(QVector3D startPt, QVector3D endPt);
    void SetLineData(QVector<QVector3D> points);         //绘制线的顶点数不再增删，只修改线段组成的顶点
    void AddDeleteLineData(QVector<QVector3D> points);   //绘制线段的顶点数有增删

    virtual bool initShaders();
    virtual void drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);

    LineMsg lineMsg;
    QVector<QVector3D> pointList;
    int maxNum=1000;
protected:
   void GenerateLineData(QVector3D pointStart,QVector3D pointEnd);
   void GenerateLinesData(QVector<QVector3D> points);

  // int numVertex=1;  //点的个数
};

#endif // LINEGEOMETRY_H
