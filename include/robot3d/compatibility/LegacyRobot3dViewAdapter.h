#pragma once
#include <QWidget>
#include <QPointer>
#include <memory>
#include "robot3d/presentation/IRobot3dView.h"
#include "robot3d/shell/opengl/OpenGlRobotViewport.h"
#include "robot3d/compatibility/LegacyRobotTypes.h"
#include "robot3d/composition/Robot3dComposition.h"
// Deprecated host facade. Owns an MVP composition; no global robot state.
class LegacyRobot3dViewAdapter : public QWidget, public robot3d::presentation::IRobot3dView
{
    Q_OBJECT
public:
    explicit LegacyRobot3dViewAdapter(QWidget *parent = nullptr);
    ~LegacyRobot3dViewAdapter();
    static void InitInstance(QWidget *parent);
    static void UnInitInstance();
    static LegacyRobot3dViewAdapter *GetInstance();
    OpenGlRobotViewport *openGlViewport(LegacyRobotDisplayFlags type, bool followRealtime);
    void UpdateRobot3dJointAngle(LegacyJointPose joints);
    void UpdateRobot3dToolPosition(LegacyEulerPose pose);
    void AddRobotWaypointPath(LegacyEulerPose pose);
    void UpdateRobot3dSafetyToolData(QVector<LegacyToolShape> tools);
    void ResizeRobot3dMode();
    bool prepareModel(robot3d::domain::RobotConfigurationPtr config, std::string &error) override;
    void showState(const robot3d::presentation::Robot3dDisplayModel &model) override;
    void submitScene(const robot3d::scene::RobotSceneSnapshot &scene) override;
    void showError(const std::string &error) override;
    void appendLog(const std::string &message) override;

private:
    robot3d::scene::RobotDisplayOptions options_;
    static QPointer<LegacyRobot3dViewAdapter> instance_;
    QPointer<OpenGlRobotViewport> viewport_;
    std::unique_ptr<robot3d::composition::Robot3dComposition> composition_;
};
