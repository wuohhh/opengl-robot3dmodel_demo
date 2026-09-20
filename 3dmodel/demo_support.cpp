#include "demo_support.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QXmlStreamReader>
#include <QDateTime>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * 1. 运动学算法实现
 *
 * 原项目中 cr_PoseTrans / cr_PoseInv / cr_ForwardKineFull 只是对预编译库
 * tpAlgApp(MSVC .lib) 的封装，跨编译器无法复用，这里按同样的接口与语义
 * 用标准 C++ 重新实现。
 * ==========================================================================*/

namespace {

/** 位姿 -> 齐次矩阵（行主序 R[9] + T[3]）
 *  旋转顺序与 robot3dglwidget::Euler2RotateMatrix 保持一致：R = Rz * Ry * Rx
 */
void PoseToMatrix(const Pose &p, double R[9], double T[3])
{
    const double rx = p.Rx * M_PI / 180.0;
    const double ry = p.Ry * M_PI / 180.0;
    const double rz = p.Rz * M_PI / 180.0;

    const double cx = cos(rx), sx = sin(rx);
    const double cy = cos(ry), sy = sin(ry);
    const double cz = cos(rz), sz = sin(rz);

    // Rx
    const double Rx[9] = {1, 0, 0, 0, cx, -sx, 0, sx, cx};
    // Ry
    const double Ry[9] = {cy, 0, sy, 0, 1, 0, -sy, 0, cy};
    // Rz
    const double Rz[9] = {cz, -sz, 0, sz, cz, 0, 0, 0, 1};

    // tmp = Ry * Rx
    double tmp[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += Ry[i * 3 + k] * Rx[k * 3 + j];
            tmp[i * 3 + j] = s;
        }
    }
    // R = Rz * tmp
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += Rz[i * 3 + k] * tmp[k * 3 + j];
            R[i * 3 + j] = s;
        }
    }

    T[0] = p.X;
    T[1] = p.Y;
    T[2] = p.Z;
}

/** 齐次矩阵 -> 位姿（旋转矩阵按 ZYX 欧拉角反解） */
void MatrixToPose(const double R[9], const double T[3], Pose &p)
{
    p.X = T[0];
    p.Y = T[1];
    p.Z = T[2];

    // R = Rz*Ry*Rx 反解
    const double sy = -R[2 * 3 + 0];
    double rx, ry, rz;
    if (fabs(sy) < 1.0 - 1e-9) {
        ry = asin(sy);
        rx = atan2(R[2 * 3 + 1], R[2 * 3 + 2]);
        rz = atan2(R[1 * 3 + 0], R[0 * 3 + 0]);
    } else {
        // 万向锁
        ry = (sy > 0) ? M_PI / 2 : -M_PI / 2;
        rx = atan2(-R[1 * 3 + 2], R[1 * 3 + 1]);
        rz = 0.0;
    }

    p.Rx = rx * 180.0 / M_PI;
    p.Ry = ry * 180.0 / M_PI;
    p.Rz = rz * 180.0 / M_PI;
}

void MatMul3(const double A[9], const double B[9], double C[9])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += A[i * 3 + k] * B[k * 3 + j];
            C[i * 3 + j] = s;
        }
}

void MatMulVec3(const double A[9], const double v[3], double out[3])
{
    for (int i = 0; i < 3; ++i)
        out[i] = A[i * 3 + 0] * v[0] + A[i * 3 + 1] * v[1] + A[i * 3 + 2] * v[2];
}

void Transpose3(const double A[9], double AT[9])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) AT[i * 3 + j] = A[j * 3 + i];
}

} // namespace

void cr_PoseTrans(Pose &result, const Pose &Pose1, const Pose &Pose2)
{
    double R1[9], T1[3], R2[9], T2[3];
    PoseToMatrix(Pose1, R1, T1);
    PoseToMatrix(Pose2, R2, T2);

    // result = Pose2^-1 * Pose1
    double R2T[9];
    Transpose3(R2, R2T);

    double d[3] = {T1[0] - T2[0], T1[1] - T2[1], T1[2] - T2[2]};
    double R[9], T[3];
    MatMul3(R2T, R1, R);
    MatMulVec3(R2T, d, T);

    MatrixToPose(R, T, result);
}

void cr_PoseInv(Pose &result, const Pose &pose)
{
    double R[9], T[3];
    PoseToMatrix(pose, R, T);

    double RT[9];
    Transpose3(R, RT);

    double negT[3] = {-T[0], -T[1], -T[2]};
    double Tout[3];
    MatMulVec3(RT, negT, Tout);

    MatrixToPose(RT, Tout, result);
}

void cr_ForwardKineFull(JointPose joint_pos, Pose &pose, WholeDH *DH)
{
    double q[ARM_DOF] = {joint_pos.L1, joint_pos.L2, joint_pos.L3,
                         joint_pos.L4, joint_pos.L5, joint_pos.L6};

    // 累积变换，初始为单位矩阵
    double Racc[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    double Tacc[3] = {0, 0, 0};

    for (int i = 0; i < ARM_DOF; ++i) {
        const double alpha = DH[i].alpha;
        const double a     = DH[i].a;
        const double d     = DH[i].d;
        const double theta = DH[i].theta + q[i] * M_PI / 180.0;   // 关节角为度

        const double ct = cos(theta), st = sin(theta);
        const double ca = cos(alpha), sa = sin(alpha);

        // 标准 DH：T = RotZ(theta) * TransZ(d) * TransX(a) * RotX(alpha)
        const double Ri[9] = {
            ct, -st * ca,  st * sa,
            st,  ct * ca, -ct * sa,
            0,        sa,       ca
        };
        const double Ti[3] = {a * ct, a * st, d};

        // Racc_new = Racc * Ri ;  Tacc_new = Racc * Ti + Tacc
        double Rnew[9], Tadd[3];
        MatMul3(Racc, Ri, Rnew);
        MatMulVec3(Racc, Ti, Tadd);

        for (int k = 0; k < 9; ++k) Racc[k] = Rnew[k];
        for (int k = 0; k < 3; ++k) Tacc[k] += Tadd[k];
    }

    MatrixToPose(Racc, Tacc, pose);
}

void cr_Eule2AxisAngle(Pose srcPose, Pose &dstPose)
{
    double R[9], T[3];
    PoseToMatrix(srcPose, R, T);

    // 旋转矩阵 -> 等效轴角
    const double trace = R[0] + R[4] + R[8];
    double theta = acos(qBound(-1.0, (trace - 1.0) / 2.0, 1.0));

    dstPose = srcPose;
    if (fabs(theta) < 1e-10) {
        dstPose.Rx = dstPose.Ry = dstPose.Rz = 0.0;
        return;
    }
    if (fabs(theta - M_PI) < 1e-6) {
        // 180 度特殊处理
        dstPose.Rx = theta * sqrt(qMax(0.0, (R[0] + 1.0) / 2.0));
        dstPose.Ry = theta * sqrt(qMax(0.0, (R[4] + 1.0) / 2.0));
        dstPose.Rz = theta * sqrt(qMax(0.0, (R[8] + 1.0) / 2.0));
        return;
    }
    const double k = theta / (2.0 * sin(theta));
    dstPose.Rx = k * (R[7] - R[5]);
    dstPose.Ry = k * (R[2] - R[6]);
    dstPose.Rz = k * (R[3] - R[1]);
}

void cr_AxisAngle2Eule(Pose srcPose, Pose &dstPose)
{
    const double rx = srcPose.Rx, ry = srcPose.Ry, rz = srcPose.Rz;
    const double theta = sqrt(rx * rx + ry * ry + rz * rz);

    dstPose = srcPose;
    if (theta < 1e-10) {
        dstPose.Rx = dstPose.Ry = dstPose.Rz = 0.0;
        return;
    }
    const double u = rx / theta, v = ry / theta, w = rz / theta;
    const double c = cos(theta), s = sin(theta), t = 1.0 - c;

    const double R[9] = {
        u * u * t + c,     u * v * t - w * s, u * w * t + v * s,
        u * v * t + w * s, v * v * t + c,     v * w * t - u * s,
        u * w * t - v * s, v * w * t + u * s, w * w * t + c
    };
    double T[3] = {srcPose.X, srcPose.Y, srcPose.Z};
    MatrixToPose(R, T, dstPose);
}

/* ============================================================================
 * 2. 单例与配置实现
 * ==========================================================================*/

DemoDataHub *DemoDataHub::GetInstance()
{
    static DemoDataHub inst;
    return &inst;
}

void DemoDataHub::UserOperateLogPrint(UserOperateType type, const QString &name, const QString &value)
{
    Q_UNUSED(type)
    qDebug() << "[UserOperate]" << name << value;
}

bool DemoDataHub::LoadRobotType(const QString &robotTypeName, const QString &robotTypeRoot)
{
    const QString cfgPath = QString("%1/%2/config.xml").arg(robotTypeRoot, robotTypeName);
    QFile file(cfgPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[DemoDataHub] 无法打开机型配置:" << QFileInfo(cfgPath).absoluteFilePath();
        return false;
    }

    robotType.robotTypeParaStr = robotTypeName;

    // 默认 DH（理论上都会被 config.xml 覆盖）
    for (int i = 0; i < ROB_AXIS_NUM; ++i) {
        stdRobotWholeDH[i].alpha = 0.0;
        stdRobotWholeDH[i].a     = 0.0;
        stdRobotWholeDH[i].d     = 0.0;
        stdRobotWholeDH[i].theta = 0.0;
        stdRobotWholeDH[i].beta  = 0.0;
    }

    QXmlStreamReader xml(&file);
    int idx = 0;
    while (!xml.atEnd() && idx < ROB_AXIS_NUM) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("DHpara")) {
            // DHpara 的子元素为 j0..j5，每项形如 "alpha,a,d,theta,beta"（alpha/theta 为弧度）
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name() == QLatin1String("DHpara")) break;
                if (xml.isStartElement() && idx < ROB_AXIS_NUM) {
                    const QString text = xml.readElementText().trimmed();
                    const QStringList parts = text.split(',');
                    if (parts.count() >= 5) {
                        stdRobotWholeDH[idx].alpha = parts.at(0).toDouble();
                        stdRobotWholeDH[idx].a     = parts.at(1).toDouble();
                        stdRobotWholeDH[idx].d     = parts.at(2).toDouble();
                        stdRobotWholeDH[idx].theta = parts.at(3).toDouble();
                        stdRobotWholeDH[idx].beta  = parts.at(4).toDouble();
                    }
                    ++idx;
                }
            }
            break;
        }
    }
    file.close();

    qDebug() << "[DemoDataHub] 载入机型" << robotTypeName << "DH 段数:" << idx;
    return true;
}

void DemoDataHub::ApplyJointPose(const JointPose &jointPose)
{
    currentJointPose = jointPose;
    cr_ForwardKineFull(currentJointPose, currentPose, stdRobotWholeDH);
}

void DemoDataHub::StepSimulation(double dtSeconds)
{
    // 演示运动：让大臂/小臂按正弦规律摆动，并联动计算正解位姿
    static double t = 0.0;
    t += dtSeconds;

    JointPose jp;
    jp.L1 = 30.0 * sin(t * 0.8);
    jp.L2 = 25.0 * sin(t * 0.5) - 10.0;
    jp.L3 = 30.0 * sin(t * 0.65 + 1.0) + 20.0;
    jp.L4 = 45.0 * sin(t * 0.9 + 0.3);
    jp.L5 = 35.0 * sin(t * 1.1 + 2.0);
    jp.L6 = 60.0 * sin(t * 0.7 + 1.5);

    ApplyJointPose(jp);
}

/* ------------------------------- RobotConfig ------------------------------ */

RobotConfig *RobotConfig::GetInstance()
{
    static RobotConfig inst;
    return &inst;
}

Pose RobotConfig::GetCoordSysPose(const QString &coordSysID)
{
    Pose ret = {0, 0, 0, 0, 0, 0};
    if (coordSysID == QLatin1String("b_0") || coordSysID == QLatin1String("t_0")) {
        return ret;
    }
    for (int i = 0; i < coordSysList.count(); ++i) {
        if (coordSysList.at(i).coordSysID == coordSysID) {
            return coordSysList.at(i).coordSysPose;
        }
    }
    for (int i = 0; i < wcsList.count(); ++i) {
        if (wcsList.at(i).wcsID == coordSysID) {
            return wcsList.at(i).root2Ref;
        }
    }
    return ret;
}

CoordSysType RobotConfig::GetCoordSysType(const QString &coordSysID)
{
    if (coordSysID.isEmpty()) return base;
    const QChar c = coordSysID.at(0);
    if (c == 'b') return base;
    if (c == 't') return tool;
    if (c == 'p') return point;
    if (c == 'l') return line;
    if (c == 'f') return plane;
    if (coordSysID.startsWith("wcs_")) return work;
    if (c == 'j') return joint;
    return base;
}

int RobotConfig::GetIDNumber(const QString &coordSysID)
{
    const int pos = coordSysID.lastIndexOf('_');
    if (pos < 0 || pos + 1 >= coordSysID.length()) return 0;
    return coordSysID.mid(pos + 1).toInt();
}

QString RobotConfig::GetTCPnameFromID(const QString &tcpID)
{
    if (tcpID.isEmpty()) return QStringLiteral("tcp");
    return tcpID;
}

QString RobotConfig::GetTCPnameFromPose(Pose tcpPose)
{
    return QStringLiteral("tcp(%1,%2,%3)")
            .arg(tcpPose.X, 0, 'f', 1)
            .arg(tcpPose.Y, 0, 'f', 1)
            .arg(tcpPose.Z, 0, 'f', 1);
}

Pose RobotConfig::getWorkSysInWorldPose(const QString &wcsID)
{
    for (int i = 0; i < wcsList.count(); ++i) {
        if (wcsList.at(i).wcsID == wcsID) {
            return wcsList.at(i).root2Ref;
        }
    }
    Pose ret = {0, 0, 0, 0, 0, 0};
    return ret;
}

/* --------------------------- SoftwareUI_Controller ------------------------ */

SoftwareUI_Controller *SoftwareUI_Controller::GetInstance()
{
    static SoftwareUI_Controller inst;
    return &inst;
}

/* ------------------------------ TipMsgDialog ------------------------------ */

TipMsgDialog *TipMsgDialog::GetInstance()
{
    static TipMsgDialog inst;
    return &inst;
}

void TipMsgDialog::ShowThisDialog(WarningLevel level, const QString &title, const QString &text)
{
    const char *lv = (level == WarningLevel::LV_WARNING) ? "WARNING"
                   : (level == WarningLevel::LV_ERROR)   ? "ERROR" : "INFO";
    qWarning() << "[TipMsgDialog]" << lv << title << text;
}
