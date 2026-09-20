#ifndef DEMO_SUPPORT_H
#define DEMO_SUPPORT_H
/**
 * @file demo_support.h
 * @brief 3D 示教器模块移植适配层（shim）
 *
 * 原项目中 move3D/model3D/*.{h,cpp} 依赖以下 4 个外部模块：
 *   1. base/basedefine.h            -> 基础类型 Pose/JointPose/WholeDH/CopyPose 等
 *   2. commu/commdatacontroller.h   -> CommDataController 单例（机器人数据中枢）
 *   3. robotconfig/robotconfig.h    -> RobotConfig 单例（坐标系/机型/TCP 配置）
 *   4. base/softwareui_controller.h -> SoftwareUI_Controller 单例（机型切换通知）
 *   5. robotAlgorithm/robotalginterface.h -> cr_PoseTrans/cr_PoseInv/cr_ForwardKineFull
 *                                           （原实现调用 MSVC 预编译库 tpAlgApp，MinGW 无法链接，
 *                                             故在 demo_support.cpp 中用纯 C++ 重新实现）
 *
 * 本文件把这 5 项一次性提供，使 3dmodel/ 下的源文件仅需把上述 include 改为
 * #include "demo_support.h" 即可编译，业务代码零改动。
 */

#include <QString>
#include <QStringList>
#include <QVector>
#include <QObject>
#include <QColor>
#include <QDebug>
#include <QPushButton>
#include <cmath>

/* ============================================================================
 * 1. 基础类型（对应原 base/basedefine.h + robotAlgorithm/tpAlgApp/include/alg_type.h）
 * ==========================================================================*/

#define ROB_AXIS_NUM   6
#define ARM_DOF        6
#define PI_MATH        3.14159265358979

/** 安全限制边界平面数量（对应 base/basedefine.h） */
#define CR6_SAFETY_LIMITS_BOUNDARY_PLANE_NUM 8


/** 关节角（度） */
typedef struct JointPose
{
    double L1;
    double L2;
    double L3;
    double L4;
    double L5;
    double L6;
} JointPose;

/** 空间位姿：XYZ 单位 mm，Rx/Ry/Rz 为轴角（弧度制向量），与原项目一致 */
typedef struct Pose
{
    double X;
    double Y;
    double Z;
    double Rx;
    double Ry;
    double Rz;
} Pose;

/** DH 参数（对应 alg_type.h 的 WholeDH） */
typedef struct WholeDH
{
    double alpha;
    double a;
    double d;
    double theta;
    double beta;
} WholeDH;

/** 坐标系类型（对应 plugin_sdk cgxibasedefine.h） */
enum CoordSysType
{
    base = 0,
    joint = 1,
    point = 2,
    line = 3,
    plane = 4,
    tool = 5,
    vision = 6,
    path = 7,
    world = 8,
    work = 9
};

/** 下发坐标系消息用的类型（对应 proto 的 CRData__CoordinateType，取 demo 用到的项） */
enum CRData__CoordinateType
{
    CR__DATA__COORDINATE_TYPE__baseCoordinate       = 0,
    CR__DATA__COORDINATE_TYPE__ToolBaseCoordinate   = 1,
    CR__DATA__COORDINATE_TYPE__PointCoordinate      = 2,
    CR__DATA__COORDINATE_TYPE__LineCoordinate       = 3,
    CR__DATA__COORDINATE_TYPE__PlaneCoordinate      = 4
};

/** 机器人模式（对应 robotData.pb-c.h） */
enum CRData__RobotModes
{
    CR__DATA__ROBOT_MODES__CloseBrake = 12
};

/** Lua 脚本状态（对应 robotData.pb-c.h） */
enum CRData__LuaScriptStatus
{
    CR__DATA__LUA__SCRIPT_STATUS__lua_Script_pause = 2,
    CR__DATA__LUA__SCRIPT_STATUS__lua_Script_run   = 3
};

/** 运动类型（对应 robotService.pb-c.h） */
enum class CRService__MoveType
{
    CR__SERVICE__MOVE_TYPE__DecStop = 0
};

/** 用户操作日志类型（对应 base/basedefine.h） */
enum class UserOperateType
{
    click = 0,
    combox,
    button,
    touch
};

/** 警告等级（对应 log/LogBaseDefine.h）
 *  注意：不能直接叫 ERROR/INFO，windows.h 会把 ERROR 定义成宏（经 qopengl.h 间接引入） */
enum class WarningLevel
{
    LV_INFO = 0,
    LV_WARNING,
    LV_ERROR
};

/* --------------------------- 位姿/关节拷贝工具 --------------------------- */

inline void CopyPose(Pose srcPose, Pose *dstPose)
{
    if (dstPose == nullptr) return;
    *dstPose = srcPose;
}

inline void CopyJointPose(JointPose srcJointPose, JointPose *dstJointPose)
{
    if (dstJointPose == nullptr) return;
    *dstJointPose = srcJointPose;
}

/** Pose -> double[6] */
inline void CopyPoseMsg(Pose coordSysPose, double *toolposition)
{
    if (toolposition == nullptr) return;
    toolposition[0] = coordSysPose.X;
    toolposition[1] = coordSysPose.Y;
    toolposition[2] = coordSysPose.Z;
    toolposition[3] = coordSysPose.Rx;
    toolposition[4] = coordSysPose.Ry;
    toolposition[5] = coordSysPose.Rz;
}

/** JointPose -> double[6] */
inline void CopyJointAngleMsg(JointPose coordSysJointAngle, double *toolaxisangle)
{
    if (toolaxisangle == nullptr) return;
    toolaxisangle[0] = coordSysJointAngle.L1;
    toolaxisangle[1] = coordSysJointAngle.L2;
    toolaxisangle[2] = coordSysJointAngle.L3;
    toolaxisangle[3] = coordSysJointAngle.L4;
    toolaxisangle[4] = coordSysJointAngle.L5;
    toolaxisangle[5] = coordSysJointAngle.L6;
}

/** double[6] -> Pose */
inline void SetPoseMsg(double *toolposition, Pose *coordSysPose)
{
    if (toolposition == nullptr || coordSysPose == nullptr) return;
    coordSysPose->X  = toolposition[0];
    coordSysPose->Y  = toolposition[1];
    coordSysPose->Z  = toolposition[2];
    coordSysPose->Rx = toolposition[3];
    coordSysPose->Ry = toolposition[4];
    coordSysPose->Rz = toolposition[5];
}

/** double[6] -> JointPose */
inline void SetJointAngleMsg(double *toolaxisangle, JointPose *coordSysJointAngle)
{
    if (toolaxisangle == nullptr || coordSysJointAngle == nullptr) return;
    coordSysJointAngle->L1 = toolaxisangle[0];
    coordSysJointAngle->L2 = toolaxisangle[1];
    coordSysJointAngle->L3 = toolaxisangle[2];
    coordSysJointAngle->L4 = toolaxisangle[3];
    coordSysJointAngle->L5 = toolaxisangle[4];
    coordSysJointAngle->L6 = toolaxisangle[5];
}

/* ============================================================================
 * 2. 运动学算法（重新实现，替代 robotAlgorithm/tpAlgApp 预编译库）
 * ==========================================================================*/

/** 位姿1 在 位姿2 坐标系下的表示：result = Pose2^-1 * Pose1 */
void cr_PoseTrans(Pose &result, const Pose &Pose1, const Pose &Pose2);

/** 位姿求逆 */
void cr_PoseInv(Pose &result, const Pose &pose);

/** DH 正运动学：关节角(度) -> 末端位姿 */
void cr_ForwardKineFull(JointPose joint_pos, Pose &pose, WholeDH *DH);

/** 欧拉角(ZYX) -> 位姿的轴角表示 */
void cr_Eule2AxisAngle(Pose srcPose, Pose &dstPose);

/** 位姿的轴角表示 -> 欧拉角(ZYX) */
void cr_AxisAngle2Eule(Pose srcPose, Pose &dstPose);

/* ============================================================================
 * 3. 对话框桩（对应 popup/tipmsgdialog.h，demo 中降级为控制台输出）
 * ==========================================================================*/

class TipMsgDialog : public QObject
{
    Q_OBJECT
public:
    static TipMsgDialog *GetInstance();
    void ShowThisDialog(WarningLevel level, const QString &title, const QString &text);
};

/* ============================================================================
 * 3.1 扩展轴位姿与自定义按钮
 *
 *     原 base/userpushbutton.h 依赖庞大的 basedefine.h，而 3D 模块只用到
 *     editPushButton 指针与几个 SetPose_* 槽，这里按用到的接口重建。
 * ==========================================================================*/

/** 扩展轴位姿（原项目 EXJointPose 含 4 个扩展轴）
 *  [demo 适配] 追加 coordSysID：原项目 curExjJogPara 是带该字段的结构体，
 *  3D 模块会写 curExjJogPara.coordSysID，这里把字段并入类型以保持代码零改动。 */
typedef struct EXJointPose
{
    double  E1;
    double  E2;
    double  E3;
    double  E4;
    QString coordSysID;
} EXJointPose;

class UserPushButton : public QPushButton
{
    Q_OBJECT
public:
    explicit UserPushButton(const QString &text, QWidget *parent = Q_NULLPTR)
        : QPushButton(text, parent) {}
    ~UserPushButton() {}

public slots:
    void SetPose_JointAngleMsg(JointPose jointPose, Pose pose)
    {
        emit PoseChanged_signals(jointPose, pose);
    }
    void SetPose_ExjMsg(JointPose jointPose, Pose pose, EXJointPose exjJointPose)
    {
        Q_UNUSED(exjJointPose)
        emit PoseChanged_signals(jointPose, pose);
    }
    void SetPose_ExJointMsg(EXJointPose exjJointPose)
    {
        Q_UNUSED(exjJointPose)
    }

signals:
    void PoseChanged_signals(JointPose jointPose, Pose pose);
};

/* ============================================================================
 * 3.2 内存监控桩（对应 commu/rammonitorthread.h）
 *     demo 中恒返回 false，不触发内存保护中断
 * ==========================================================================*/

class RamMonotorThread : public QObject
{
    Q_OBJECT
public:
    static RamMonotorThread *GetInstance()
    {
        static RamMonotorThread inst;
        return &inst;
    }
    bool MemoryCheck(const char *function, const char *file)
    {
        Q_UNUSED(function)
        Q_UNUSED(file)
        return false;
    }
};

/* ============================================================================
 * 4. 机器人配置（对应 robotconfig/robotconfig.h 中被 3D 模块用到的部分）
 * ==========================================================================*/

/** 用户坐标系条目 */
typedef struct CoordSysMsg
{
    QString coordSysID;     // 如 p_0 / l_0 / f_0 / t_0 / b_0
    QString coordSysName;
    Pose    coordSysPose = {0, 0, 0, 0, 0, 0};
    bool    coordSysValid = true;
} CoordSysMsg;

/** 工件坐标系条目 */
typedef struct WcsMsg
{
    QString wcsID;          // 如 wcs_0
    QString wcsName;
    Pose    root2Ref = {0, 0, 0, 0, 0, 0};   // 相对世界坐标系的位姿
} WcsMsg;

/** 世界坐标/安装姿态（对应 worldTransform_configwidget 用到的字段） */
typedef struct WorldTransformMsg
{
    double tiltAngle = 0.0;
    double baseAngle = 0.0;
} WorldTransformMsg;

class RobotConfig : public QObject
{
    Q_OBJECT
public:
    static RobotConfig *GetInstance();

    QVector<CoordSysMsg> coordSysList;      // 用户坐标系列表
    QVector<WcsMsg>      wcsList;           // 工件坐标系列表
    WorldTransformMsg    worldTransform;    // 安装姿态

    /** 按 ID 取坐标系位姿 */
    Pose GetCoordSysPose(const QString &coordSysID);
    /** 按 ID 取坐标系类型（与原项目一致，返回 CoordSysType） */
    CoordSysType GetCoordSysType(const QString &coordSysID);
    /** 从形如 "p_12" 的 ID 中取数字编号 */
    int  GetIDNumber(const QString &coordSysID);
    /** 按 TCP ID 取名称 */
    QString GetTCPnameFromID(const QString &tcpID);
    /** 按 TCP 位姿取名称 */
    QString GetTCPnameFromPose(Pose tcpPose);
    /** 取工件坐标系在世界坐标系下的位姿 */
    Pose getWorkSysInWorldPose(const QString &wcsID);

signals:
    void CoordSysChange_signal();
    void CoordSysValChange_signal();
};

/* ============================================================================
 * 5. UI 控制器（对应 base/softwareui_controller.h）
 * ==========================================================================*/

class SoftwareUI_Controller : public QObject
{
    Q_OBJECT
public:
    static SoftwareUI_Controller *GetInstance();

signals:
    void ChangeRobotType_signal();
    void ChangeLanguage_signal();
};

/* ============================================================================
 * 6. 数据中枢（替代 commu/commdatacontroller.h 的 CommDataController）
 *
 *    类名保持为 DemoDataHub，但通过文件末尾的宏使 3dmodel/ 下的源码可以继续
 *    书写 CommDataController::GetInstance()，从而做到业务代码零改动。
 * ==========================================================================*/

class DemoDataHub : public QObject
{
    Q_OBJECT
public:
    static DemoDataHub *GetInstance();

    /** 从 robotType/<机型>/config.xml 载入机型名与 DH 参数 */
    bool LoadRobotType(const QString &robotTypeName, const QString &robotTypeRoot = QString("robotType"));

    /** 演示用：应用一组关节角，联动更新正解位姿 */
    void ApplyJointPose(const JointPose &jointPose);
    /** 演示用：正弦摆动一个关节（运动模拟） */
    void StepSimulation(double dtSeconds);

    /* ------- 与 3D 模块直接交互的数据成员（名称与原项目保持一致） ------- */
    struct RobotTypeHolder
    {
        QString robotTypeParaStr;               // 机型名，用作 robotType/<name>/model3d 路径
    } robotType;

    WholeDH  stdRobotWholeDH[ROB_AXIS_NUM];     // 标准 DH 参数表
    JointPose currentJointPose = {0, 0, 0, 0, 0, 0};
    Pose      currentPose      = {0, 0, 0, 0, 0, 0};
    Pose      baseInWorldPose  = {0, 0, 0, 0, 0, 0};

    EXJointPose currentEXJointPose;
    EXJointPose curExjJogPara;

    struct JogParaHolder
    {
        QString coordSysID = "b_0";
        int     tcpID      = -1;
        Pose    tcpOffset  = {0, 0, 0, 0, 0, 0};
    } curJogPara;

    struct CoordSysTcpParaHolder
    {
        CoordSysType coordSysType = base;
        int          coordinateId = 0;
    };
    struct TcpInCoordSysMsgHolder
    {
        CoordSysTcpParaHolder coordSysTcpPara;
    } curTcpInCoordSysMsg;

    struct SetCoordinateTcpMessageHolder
    {
        CRData__CoordinateType coordinatetype = CR__DATA__COORDINATE_TYPE__baseCoordinate;
        int tcpid        = -1;
        int coordinateid = 0;
    } setCoordinateTcpMessage, movePara_as_placeholder_unused;

    /** 对应原项目的 movePara（运动指令参数） */
    struct MoveParaHolder
    {
        CRService__MoveType movetype = CRService__MoveType::CR__SERVICE__MOVE_TYPE__DecStop;
    } movePara;

    int  curRobotModes = CR__DATA__ROBOT_MODES__CloseBrake;                 // 机器人状态
    int  curLuaScriptStatus = CR__DATA__LUA__SCRIPT_STATUS__lua_Script_run; // 脚本状态
    bool moveControlSignal = false;                                         // 运动控制信号
    bool setTCPInCoordSysSignal = false;                                    // 下发坐标系信号
    bool setSystemVariableSignal = false;
    /** 3D 控件是否跟随实时数据刷新（由 GetRobot3dGLWidget 的第二个参数控制） */
    bool updateRealtimeRobot3dMsgSignal = false;
    /** 当前 TCP 速度（mm/s），演示用 */
    double curTcpSpeed = 0.0;

    /** 供外部（如工具栏）调用的日志桩 */
    void UserOperateLogPrint(UserOperateType type, const QString &name, const QString &value);

signals:
    void UpdateCurrentTcp_signal();
    void UpdateTCPspeed_signal(double tcpSpeed);
    void updateUseCoordSysSignal();
    void JogCoordSysID_signal();
};

/* ============================================================================
 * 7. 宏映射：让 3dmodel/ 下的源码无需改动类名
 * ==========================================================================*/

#define CommDataController DemoDataHub

#endif // DEMO_SUPPORT_H
