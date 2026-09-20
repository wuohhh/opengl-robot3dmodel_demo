#ifndef  ROBOT3DGLWIDGET_H
#define ROBOT3DGLWIDGET_H

#include <QOpenGLWidget>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QBasicTimer>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include<QLayout>
#include<QPushButton>
#include<QSlider>
#include<QLabel>
#include<QComboBox>
#include<QStyledItemDelegate>
#include<QAbstractItemView>

#include "demo_support.h"
#include"robotgeometry.h"
#include"linegeometry.h"
#include"planegeometry.h"
#include"spheregeometry.h"
#include"substancegeometry.h"

enum Robot3DShowType
{
    show_Null = 0x0,
    show_CoordSys = 0x1,
    show_RobotOperate = 0x2,
    show_Zoom = 0x4,
    show_PoseOperate = 0x8,
    show_WaypointOperate = 0x10,
    show_TargetRobot = 0x20,
    show_SafePlane = 0x40,
    show_TcpSpeed = 0x80,
    show_Install = 0x100,
    show_Tool = 0x200
};

typedef struct Robot3DShowData
{
    QMatrix4x4 ViewChangeMatrix;
    QQuaternion rotation_model;
    double scalePara;
    QVector3D translate_model;
    QVector3D rotate_center;
}Robot3DShowData;

Q_DECLARE_FLAGS(Robot3DShowTypes, Robot3DShowType)

class Robot3dGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit Robot3dGLWidget(QWidget *parent = nullptr);
    ~Robot3dGLWidget();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;

    void InitUI();
    void GetOpenGLVersion();

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

public:
    QMatrix4x4 projection;

    Robot3DShowData curRobot3DShowData;
    Robot3DShowData preRobot3DShowData;
    bool isInWorldTransSignal = false;

    //QVector3D rotate_center = QVector3D(0,0,0); //QVector3D(0.1731,-3.1034,23.8948);
    bool mousePressSignal = false;

    QVector2D curPos;
    //QVector3D translate_model=QVector3D(0,0,0);

    //QMatrix4x4 ViewChangeMatrix;  //用户坐标系切换变换矩阵

    //QQuaternion rotation_model;
    QQuaternion rotation_light;

    //double scalePara;
    QSlider*  zoomSlider;
    int rotateTranslateStatus; //鼠标当前关联操作的状态，0-旋转，1-移动,初始为0
    UserPushButton* editPushButton = NULL;

    RobotGeometry *robotGeometry;
    QList<LineGeometry*> coordSysGeometry;
    RobotGeometry *secondRobotGeometry;
    bool secondRobotShowSignal = false;
    bool waypointShowSignal = false;
    bool waypointPathShowSignal = false;
    bool safePlaneShowSignal = false;
    bool toolShowSignal = false;
    bool coordsysValueChangeSignal = false;   //坐标系的值改变
    //QMatrix4x4 installMatrix;  //安装变换阵

    SphereGeometry* TCP_Geometry=NULL;

    SphereGeometry* waypointSphereGeometry=NULL;

    LineGeometry*  waypointPathlineGeometry=NULL;

    QList<PlaneGeometry*> safePlaneGeometrys;

    SubstanceGeometry* tool_Geometry;
    QList<LineGeometry*> toolCoordSysGeometry;

    void SetShowType(Robot3DShowTypes showType);

    QWidget* coordSysWidget;
    QWidget* robotOperateWidget;
    QWidget* zoomWidget;
    QWidget* poseOperateWidget;
    QWidget* waypointOperateWidget;
    QWidget* tcpSpeedWidget;

    QList<QPushButton*> waypointOpPb;

    QComboBox* selCoordSysCb;
    QLabel* curTCPNameLabel;
    QLabel* curTcpSpeedLabel;

    Robot3DShowTypes curShowType;

    QMatrix4x4 ViewChangeMatrixFunction(Pose pose);   //坐标系切换
    QMatrix4x4 Euler2RotateMatrix(Pose pose);         //欧拉角转矩阵，其中增加了缩放系数
    QMatrix4x4 AxisAngle2RotateMatrix(Pose pose);  //轴角转旋转矩阵，其中增加了缩放系数

    //planePose是面坐标  deltZ是沿着面坐标系的平移量  alpha是透明度  length表示平面的边长 目前默认是正方形
    PlaneMsg PlanePose2Points(Pose planePose, float deltZ, float alpha=1.0,QColor color=QColor(255,0,0), float length=200,float width=200);

    void IsIntoWorldTransformWidget(bool isIn);

    int maxW = 0, maxH = 0; // 视窗最大的size

public slots:
    void zoomChange_slot();
    void ZoomSlider_valueChanged(int value);

    void SetRobotJointAngle_slot(JointPose jointPose);
    void SetRobotToolPosition_slot(Pose pose);
    void SetRobotSafetyToolData(QVector<SubstanceMsg> substanceMsgsIn);

    void EditRobotPoseBtn_slot();
    void Recovery3D_slot();
    void RotateTranslatChange_slot();
    void showEndTool_slot();
    void WaypointOperateChange_slot();

    void updateUseCoordSysSlot();
    void UpdateCoordSysCombo();
    void UpdateCoordSysVal();
    //void JogCoordSysID_slot();
    void UpdateCurrentTcp_slot();

    void UpdateCoordSysChange_slot(int itemIndex);   //坐标系切换的槽函数
    void UpdateTCPspeed_slot(double tcpSpeed);

    void ChangeRobotType_slot();
    // void ChangeCtrlEvent_slot();

    void resizeCustomedWidget(int w, int h);

public:signals:
    void EndEditPose_signals();
    /**
     * @brief widgetSizeChanged 视窗大小改变，用来控制控件的大小
     */
    void widgetSizeChanged(int w, int h);

    void CurPoseChanged_signal(UserPushButton *userPb,JointPose jointPose,Pose pose);
};

#endif // ROBOT3DGLWIDGET_H
