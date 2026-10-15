#pragma once
#include <QWidget>
#include <QPointer>
#include <QTimer>
#include <QPushButton>
#include <memory>
#include "robot3dglwidget.h"
#include "demo_support.h"
#include "basegeometry.h"
#include "robot3d/composition/Robot3dComposition.h"
// Deprecated host facade. Owns an MVP composition; no global robot state.
class Robot3dControlWidget : public QWidget, public robot3d::presentation::IRobot3dView {
    Q_OBJECT
public:
    explicit Robot3dControlWidget(QWidget* parent=nullptr);
    ~Robot3dControlWidget();
    static void InitInstance(QWidget* parent);
    static void UnInitInstance();
    static Robot3dControlWidget* GetInstance();
    Robot3dGLWidget* GetRobot3dGLWidget(Robot3DShowTypes type,bool followRealtime);
    void UpdateRobot3dJointAngle(JointPose joints);
    void UpdateRobot3dToolPosition(Pose pose);
    void AddRobotWaypointPath(Pose pose);
    void UpdateRobot3dSafetyToolData(QVector<SubstanceMsg> tools);
    void ResizeRobot3dMode();
    bool prepareModel(robot3d::domain::RobotConfigurationPtr config,std::string& error) override;
    void showState(const robot3d::presentation::Robot3dDisplayModel& model) override;
    void submitScene(const robot3d::scene::RobotSceneSnapshot& scene) override;
    void showError(const std::string& error) override;
    void appendLog(const std::string& message) override;
private:
    static QPointer<Robot3dControlWidget> instance_;
    QPointer<Robot3dGLWidget> viewport_;
    std::unique_ptr<robot3d::composition::Robot3dComposition> composition_;
};
// Input recognizer only. The host connects stopRequested to its motion application service.
class TouchLongPressFilter : public QObject {
    Q_OBJECT
public:
    explicit TouchLongPressFilter(QObject* parent=nullptr);
    bool eventFilter(QObject* obj,QEvent* event) override;
    void touchEndEvent();
    QTimer timer;
    QPointer<QPushButton> senderPushButton;
    int jogKeepCount=0;
public slots:
    void handleLongPress_slot();
signals:
    void stopRequested();
};
