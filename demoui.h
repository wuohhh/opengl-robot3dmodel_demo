#ifndef DEMOUI_H
#define DEMOUI_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVector>

#include "3dmodel/demo_support.h"

class Robot3dGLWidget;
class QSlider;
class QCheckBox;

/**
 * @brief 移植验证 demo 的主界面
 *
 * 左侧：直接嵌入原项目的 3D 控件 Robot3dGLWidget（经 Robot3dControlWidget 单例取出）
 * 右侧：演示控制面板 —— 机型切换 / 关节滑条 / 运动模拟 / 坐标系切换 / 显示项开关
 */
class DemoUi : public QWidget
{
    Q_OBJECT
public:
    explicit DemoUi(QWidget *parent = nullptr);
    ~DemoUi();

private slots:
    void OnRobotTypeChanged(int index);
    void OnJointSliderChanged();
    void OnSimTimer();
    void OnSimToggled(bool checked);
    void OnCoordSysChanged(int index);
    void OnResetJoint();
    void OnCheckDump();
    void OnDisplayFlagChanged();

private:
    QWidget *BuildControlPanel();
    void     AddLog(const QString &msg);
    void     RefreshPoseLabels();
    void     ApplyJointFromSliders();

    Robot3dGLWidget *m_glWidget = nullptr;

    QComboBox *m_robotTypeCb = nullptr;
    QSlider   *m_jointSlider[ROB_AXIS_NUM] = {nullptr};
    QLabel    *m_jointLabel[ROB_AXIS_NUM] = {nullptr};
    QLabel    *m_poseLabel = nullptr;
    QLabel    *m_speedLabel = nullptr;
    QCheckBox *m_simCb = nullptr;
    QCheckBox *m_coordSysCb = nullptr;
    QCheckBox *m_waypointCb = nullptr;
    QCheckBox *m_pathCb = nullptr;
    QCheckBox *m_toolCb = nullptr;
    QComboBox *m_coordSysSelectCb = nullptr;

    QTimer         *m_simTimer = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;

    bool m_updatingSliders = false;
    bool m_dumpShotDone = false;
};

#endif // DEMOUI_H
