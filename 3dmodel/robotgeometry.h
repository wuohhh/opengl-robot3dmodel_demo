#ifndef ROBOTGEOMETRY_H
#define ROBOTGEOMETRY_H





#include"basegeometry.h"
#include "robot3d/domain/RobotTypes.h"
typedef robot3d::domain::DHParameter WholeDH;



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

    bool ChangeRobot3D(QString directory,const WholeDH stdRobotWholeDH[6]);
    void setEndWorkpieceVisible(bool visible) { endWorkpieceVisible_=visible; }


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
    void clearMesh();
    int m_glMajor=2;
    bool endWorkpieceVisible_=false;
//    QVector<QVector<MeshEntry>> m_Entries;  //每个模型文件可能有多个mesh
//    QVector<QVector<MeshEntry>> Entries;    //每个模型文件数据备份

    QVector<MeshEntry> entriesData;

    //QVector<MeshEntry> entriesDataBackup;

   QOpenGLShaderProgram program;
};

#endif // ROBOTGEOMETRY_H

