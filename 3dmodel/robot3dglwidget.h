/**
 * @file robot3dglwidget.h
 * @brief 机器人三维显示控件（示教器 3D 视图）的接口定义
 *
 * 主要职责：
 *   1. 用 OpenGL 绘制机器人本体、第二机器人、TCP 球、路点、安全平面、末端工具与坐标系轴线；
 *   2. 在本控件之上叠加操作面板（缩放、坐标系选择、TCP 速度、路点开关、置初始位等）；
 *   3. 维护视角状态（旋转 / 平移 / 缩放 / 坐标系变换）并响应鼠标拖拽交互。
 *
 * 依赖：demo_support.h 提供 Pose / JointPose / RobotConfig / CommDataController 等移植适配层；
 *       各 *geometry 类负责具体图元的顶点生成与绘制。
 */
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

/**
 * @brief 三维控件的显示项位标志
 * @note  各项按位组合后传入 SetShowType()，一次决定子面板的显隐与三维要素的绘制；
 *        show_Null 表示不显示任何面板与三维要素。
 */
enum Robot3DShowType
{
    show_Null = 0x0,             ///< 不显示任何面板与三维要素
    show_CoordSys = 0x1,         ///< 显示坐标系选择面板（下拉框 + 当前 TCP 名称）
    show_RobotOperate = 0x2,     ///< 显示机器人操作按钮（置初始位 / 平移旋转切换 / 末端工件显隐）
    show_Zoom = 0x4,             ///< 显示缩放按钮与缩放滑条
    show_PoseOperate = 0x8,      ///< 显示位姿编辑的“确定 / 取消”按钮
    show_WaypointOperate = 0x10, ///< 显示路点操作条（路点 / 轨迹勾选按钮）
    show_TargetRobot = 0x20,     ///< 绘制第二台机器人（目标位姿）
    show_SafePlane = 0x40,       ///< 绘制安全平面
    show_TcpSpeed = 0x80,        ///< 显示 TCP 速度标签
    show_Install = 0x100,        ///< 世界安装视角模式（叠加安装角；进入时重置视角，退出时恢复）
    show_Tool = 0x200            ///< 绘制末端工具实体及其坐标系
};

/**
 * @brief 视角显示状态：旋转 / 平移 / 缩放与用户坐标系变换
 * @note  curRobot3DShowData 为当前状态；preRobot3DShowData 用于世界安装模式进出时保存与恢复。
 */
typedef struct Robot3DShowData
{
    QMatrix4x4 ViewChangeMatrix; ///< 当前用户坐标系变换矩阵（由 ViewChangeMatrixFunction 生成）
    QQuaternion rotation_model;  ///< 鼠标拖拽累积的模型旋转四元数
    double scalePara;            ///< 缩放系数（滑条 0~100 映射为 0.4~1.9，默认 1.0）
    QVector3D translate_model;   ///< 模型平移量（在用户坐标系下表达）
    QVector3D rotate_center;     ///< 模型旋转中心（取机器人第 3 关节中心）
}Robot3DShowData;

/// 允许对枚举按位组合，作为 SetShowType() 的入参类型
Q_DECLARE_FLAGS(Robot3DShowTypes, Robot3DShowType)

/**
 * @brief 机器人三维显示控件
 *
 * 以 QOpenGLWidget 作为绘制载体，并通过内部布局叠加操作面板；
 * 数据由 CommDataController（本工程中映射为 DemoDataHub）单例的信号与数据成员驱动，
 * 具体图元交由 RobotGeometry / LineGeometry / SphereGeometry / PlaneGeometry / SubstanceGeometry 绘制。
 */
class Robot3dGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit Robot3dGLWidget(QWidget *parent = nullptr);
    ~Robot3dGLWidget();

protected:
    /// 鼠标按下：记录拖拽起点并进入拖拽状态
    void mousePressEvent(QMouseEvent *e) override;
    /// 鼠标抬起：结束拖拽状态
    void mouseReleaseEvent(QMouseEvent *e) override;
    /// 鼠标移动：按 rotateTranslateStatus 对模型做增量旋转或平移
    void mouseMoveEvent(QMouseEvent *e) override;

    /// 创建并布局叠加在 3D 视口之上的各类操作面板
    void InitUI();
    /// 读取 OpenGL 主版本号写入全局 g_openglVersion（供各几何类选择顶点缓冲更新方式）
    void GetOpenGLVersion();

    /// 初始化 GL 状态与全部几何对象（机器人、坐标系、TCP、路点、安全平面、工具）
    void initializeGL() override;
    /// 设置透视投影（视角 45°、近平面 30、远平面 400），并通知叠加控件自适应尺寸
    void resizeGL(int w, int h) override;
    /// 组装 model / view / projection 矩阵并依次绘制各类几何体
    void paintGL() override;

public:
    QMatrix4x4 projection; ///< 透视投影矩阵（resizeGL 中设置）

    Robot3DShowData curRobot3DShowData; ///< 当前视角状态
    Robot3DShowData preRobot3DShowData; ///< 世界安装模式下的视角备份
    bool isInWorldTransSignal = false;  ///< 是否处于世界安装视角模式

    //QVector3D rotate_center = QVector3D(0,0,0); //QVector3D(0.1731,-3.1034,23.8948);
    /// 鼠标按下状态（拖拽期间为 true）
    bool mousePressSignal = false;

    QVector2D curPos; ///< 上一次鼠标位置（用于计算增量位移）
    //QVector3D translate_model=QVector3D(0,0,0);

    //QMatrix4x4 ViewChangeMatrix;  //用户坐标系切换变换矩阵

    //QQuaternion rotation_model;
    QQuaternion rotation_light; ///< 预留的光照旋转量

    //double scalePara;
    QSlider*  zoomSlider;                  ///< 缩放滑条（0~100，默认 40）
    int rotateTranslateStatus;             ///< 鼠标拖拽行为：0-旋转，1-平移（在 InitUI 中初始化为 0）
    UserPushButton* editPushButton = NULL; ///< 发起“编辑位姿”的按钮（由外部模块设置，确定 / 取消时回调其槽）

    /// 机器人本体（含末端工具模型）
    RobotGeometry *robotGeometry;
    QList<LineGeometry*> coordSysGeometry;  ///< 当前坐标系的三条轴线（X 红 / Y 绿 / Z 蓝）
    RobotGeometry *secondRobotGeometry;     ///< 第二台机器人（目标位姿）
    bool secondRobotShowSignal = false;     ///< 是否绘制第二台机器人
    bool waypointShowSignal = false;        ///< 是否绘制路点球
    bool waypointPathShowSignal = false;    ///< 是否绘制路点轨迹线
    bool safePlaneShowSignal = false;       ///< 是否绘制安全平面
    bool toolShowSignal = false;            ///< 是否绘制末端工具及工具坐标系
    bool coordsysValueChangeSignal = false; ///< 坐标系数值发生变化（由 UpdateCoordSysVal 置位）
    //QMatrix4x4 installMatrix;  //安装变换阵

    SphereGeometry* TCP_Geometry=NULL; ///< TCP 位置球

    SphereGeometry* waypointSphereGeometry=NULL; ///< 路点球（批量绘制）

    LineGeometry*  waypointPathlineGeometry=NULL; ///< 路点轨迹折线

    QList<PlaneGeometry*> safePlaneGeometrys; ///< 安全平面（按边界平面数量预创建）

    SubstanceGeometry* tool_Geometry;          ///< 末端工具实体
    QList<LineGeometry*> toolCoordSysGeometry; ///< 末端工具的三条坐标轴线

    /// 按位标志设置显示项与显示模式（内部会按当前 Jog 坐标系重算变换并刷新坐标轴线）
    void SetShowType(Robot3DShowTypes showType);

    QWidget* coordSysWidget;        ///< 顶部：坐标系下拉框 + 当前 TCP 名称
    QWidget* robotOperateWidget;    ///< 中部左侧：置初始位 / 平移旋转切换 / 末端工件显隐
    QWidget* zoomWidget;            ///< 中部右侧：缩放按钮 + 缩放滑条
    QWidget* poseOperateWidget;     ///< 底部：位姿编辑“确定 / 取消”
    QWidget* waypointOperateWidget; ///< 顶部右侧：路点操作按钮条
    QWidget* tcpSpeedWidget;        ///< 底部：TCP 速度显示

    QList<QPushButton*> waypointOpPb; ///< 路点操作条上的按钮（下标 3、4 分别对应路点与轨迹）

    QComboBox* selCoordSysCb; ///< 坐标系下拉框（itemData 保存坐标系 ID）
    QLabel* curTCPNameLabel;  ///< 当前 TCP 名称标签
    QLabel* curTcpSpeedLabel; ///< TCP 速度数值标签

    Robot3DShowTypes curShowType; ///< 当前显示项位标志（paintGL 中判断 show_Install）

    /// 生成坐标系变换矩阵，并同步刷新三条坐标轴线（内部会请求一次重绘）
    QMatrix4x4 ViewChangeMatrixFunction(Pose pose);   //坐标系切换
    /// 欧拉角(ZYX)转旋转矩阵（平移列按 G_SCALE 换算到模型单位）
    QMatrix4x4 Euler2RotateMatrix(Pose pose);         //欧拉角转矩阵，其中增加了缩放系数
    /// 轴角转旋转矩阵（Rx/Ry/Rz 作为轴角向量，弧度制；平移列按 G_SCALE 换算到模型单位）
    QMatrix4x4 AxisAngle2RotateMatrix(Pose pose);  //轴角转旋转矩阵，其中增加了缩放系数

    //planePose是面坐标  deltZ是沿着面坐标系的平移量  alpha是透明度  length表示平面的边长 目前默认是正方形
    /// 由平面位姿、法向偏移、透明度、颜色与边长生成平面的四个角点（逆时针，单位 mm）
    /// @param deltZ 沿平面法向的平移量；length / width 为平面边长，默认 200mm
    PlaneMsg PlanePose2Points(Pose planePose, float deltZ, float alpha=1.0,QColor color=QColor(255,0,0), float length=200,float width=200);

    /// 进入 / 退出世界安装视角模式：进入时重置视角，退出时恢复进入前的视角
    void IsIntoWorldTransformWidget(bool isIn);

    int maxW = 0, maxH = 0; ///< 视窗历史最大尺寸（用于小屏时缩小操作按钮）

public slots:
    /// 缩放按钮：按滑条 pageStep 增减数值（按住自动重复）
    void zoomChange_slot();
    /// 缩放滑条变化：映射为 scalePara 并请求重绘
    void ZoomSlider_valueChanged(int value);

    /// 关节角刷新：驱动机器人本体各关节顶点重算
    void SetRobotJointAngle_slot(JointPose jointPose);
    /// 末端位姿刷新：更新 TCP 球位置（工具坐标系下同时刷新坐标轴线）
    void SetRobotToolPosition_slot(Pose pose);
    /// 末端工具数据刷新：更新工具实体与工具坐标系轴线
    void SetRobotSafetyToolData(QVector<SubstanceMsg> substanceMsgsIn);

    /// 位姿编辑的确定 / 取消：确定时回调发起编辑的按钮，随后发出 EndEditPose_signals
    void EditRobotPoseBtn_slot();
    /// 置初始位：恢复默认视角（安装模式下重放当前显示模式）
    void Recovery3D_slot();
    /// 旋转 / 平移拖拽行为切换
    void RotateTranslatChange_slot();
    /// 末端工件显隐切换（影响机器人绘制是否包含 tool 模型）
    void showEndTool_slot();
    /// 路点 / 轨迹显示开关
    void WaypointOperateChange_slot();

    /// 随实时数据回读当前 TCP 所在坐标系，并同步下拉框与视角变换
    void updateUseCoordSysSlot();
    /// 依据配置重建坐标系下拉框选项
    void UpdateCoordSysCombo();
    /// 坐标系数值变化后重新执行一次坐标系切换处理
    void UpdateCoordSysVal();
    //void JogCoordSysID_slot();
    /// 刷新当前 TCP 名称
    void UpdateCurrentTcp_slot();

    /// 下拉框选择变化：向通信层下发坐标系切换请求
    void UpdateCoordSysChange_slot(int itemIndex);   //坐标系切换的槽函数
    /// 刷新 TCP 速度显示
    void UpdateTCPspeed_slot(double tcpSpeed);

    /// 机型切换：按新机型与 DH 参数重建机器人模型
    void ChangeRobotType_slot();
    // void ChangeCtrlEvent_slot();

    /// 视窗尺寸变化时调整操作按钮尺寸（小屏适配）
    void resizeCustomedWidget(int w, int h);

public:signals:
    /// 位姿编辑结束（确定或取消）后发出
    void EndEditPose_signals();
    /**
     * @brief widgetSizeChanged 视窗大小改变，用来控制控件的大小
     */
    void widgetSizeChanged(int w, int h);

    /// 当前位姿变化通知（携带发起编辑的按钮与关节角 / 位姿）
    void CurPoseChanged_signal(UserPushButton *userPb,JointPose jointPose,Pose pose);
};

#endif // ROBOT3DGLWIDGET_H
