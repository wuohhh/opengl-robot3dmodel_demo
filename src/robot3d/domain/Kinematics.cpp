#include "robot3d/domain/Kinematics.h"
#include <cmath>
#include <algorithm>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
namespace robot3d { namespace domain {
namespace {

/** 位姿 -> 齐次矩阵（行主序 R[9] + T[3]）
 *  旋转顺序与 robot3dglwidget::Euler2RotateMatrix 保持一致：R = Rz * Ry * Rx
 */
void PoseToMatrix(const EulerPoseDeg &p, double R[9], double T[3])
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
void MatrixToPose(const double R[9], const double T[3], EulerPoseDeg &p)
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

void poseRelativeTo(EulerPoseDeg &result, const EulerPoseDeg &Pose1, const EulerPoseDeg &Pose2)
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

void inversePose(EulerPoseDeg &result, const EulerPoseDeg &pose)
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

void forwardKinematics(JointPoseDeg joint_pos, EulerPoseDeg &pose, const DHParameter *DH)
{
    double q[6] = {joint_pos.L1, joint_pos.L2, joint_pos.L3,
                         joint_pos.L4, joint_pos.L5, joint_pos.L6};

    // 累积变换，初始为单位矩阵
    double Racc[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    double Tacc[3] = {0, 0, 0};

    for (int i = 0; i < 6; ++i) {
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

void eulerToAxisAngle(EulerPoseDeg srcPose, AxisAngleRad &dstPose)
{
    double R[9], T[3];
    PoseToMatrix(srcPose, R, T);

    // 旋转矩阵 -> 等效轴角
    const double trace = R[0] + R[4] + R[8];
    double theta = acos(std::max(-1.0, std::min(1.0, (trace - 1.0) / 2.0)));

    dstPose.X=srcPose.X; dstPose.Y=srcPose.Y; dstPose.Z=srcPose.Z;
    if (fabs(theta) < 1e-10) {
        dstPose.Rx = dstPose.Ry = dstPose.Rz = 0.0;
        return;
    }
    if (fabs(theta - M_PI) < 1e-6) {
        // 180 度特殊处理
        dstPose.Rx = theta * sqrt(std::max(0.0, (R[0] + 1.0) / 2.0));
        dstPose.Ry = theta * sqrt(std::max(0.0, (R[4] + 1.0) / 2.0));
        dstPose.Rz = theta * sqrt(std::max(0.0, (R[8] + 1.0) / 2.0));
        return;
    }
    const double k = theta / (2.0 * sin(theta));
    dstPose.Rx = k * (R[7] - R[5]);
    dstPose.Ry = k * (R[2] - R[6]);
    dstPose.Rz = k * (R[3] - R[1]);
}

void axisAngleToEuler(AxisAngleRad srcPose, EulerPoseDeg &dstPose)
{
    const double rx = srcPose.Rx, ry = srcPose.Ry, rz = srcPose.Rz;
    const double theta = sqrt(rx * rx + ry * ry + rz * rz);

    dstPose.X=srcPose.X; dstPose.Y=srcPose.Y; dstPose.Z=srcPose.Z;
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


}}
