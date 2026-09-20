#ifndef ROBOTGEOMETRY_H
#define ROBOTGEOMETRY_H

#include"assimp/Importer.hpp"
#include"assimp/scene.h"
#include"assimp/postprocess.h"

#include"basegeometry.h"
#include "demo_support.h"

extern int showEndToolStatus; //是否显示末端工件，0-不显示，1-显示，默认0

class RobotGeometry :  public QOpenGLFunctions
{    
public:
    RobotGeometry();
    virtual ~RobotGeometry();

    virtual bool initShaders();
    virtual void drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);
    void drawSecondGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix);
    bool LoadMesh(const std::string& Filename, MeshEntry &mesh_model,int joint);
    void CalRobotViewTargetPos();
    void MatrixMultiVector3D(float *matrix4x4,float *vector3d, QVector3D &result);

    void ChangeRobot3D(QString name,WholeDH stdRobotWholeDH[6]);


    int numJoints;       //关节数
    int numTool;         //是否有末端工具，0/1

    QVector3D Target;    //相机看的位置
    QVector3D Pos;       //相机的位置

    void multjointTransform(double array[6]);      //围绕任意轴变换 多关节
    void multjointTransform2(double array[6]);      //围绕任意轴变换 多关节
    QMatrix4x4 setTransformMatrix(QVector3D center, QVector3D normal,float degree);
    QVector<multiJointData> jointData;
    void set_rotate_center(QVector3D &rotate_center);

private:
//    QVector<QVector<MeshEntry>> m_Entries;  //每个模型文件可能有多个mesh
//    QVector<QVector<MeshEntry>> Entries;    //每个模型文件数据备份

    QVector<MeshEntry> entriesData;

    //QVector<MeshEntry> entriesDataBackup;

   QOpenGLShaderProgram program;
};

#endif // ROBOTGEOMETRY_H

