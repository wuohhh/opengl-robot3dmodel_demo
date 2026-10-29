#pragma once
#include <QObject>
#include <QTimer>
#include <QPointer>
#include "robot3d/presentation/IRobot3dView.h"
#include "robot3d/application/Robot3dService.h"
#include "robot3d/application/SimulationService.h"
namespace robot3d
{
namespace presentation
{
class Robot3dPresenter : public QObject
{
    Q_OBJECT
public:
    Robot3dPresenter(IRobot3dView &view, QObject &lifetime, application::Robot3dService &service,
                     application::SimulationService &simulation);
    void start(const std::string &type);
    bool onRobotTypeRequested(const std::string &type);
    void onJointPoseRequested(const domain::JointPoseDeg &joints);
    void onCoordinateRequested(const std::string &id);
    void onSimulationRequested(bool running);
    void advanceSimulation(int steps, double dt = .04);
    void onDisplayOptionsRequested(const scene::RobotDisplayOptions &options);
    void onTrajectoryPoint(const domain::EulerPoseDeg &pose, bool running);
    void onToolsRequested(const std::vector<scene::ToolShape> &tools);
    void addWaypoint();
    void onObservedTcp(const domain::EulerPoseDeg &pose);
    const domain::RobotStateSnapshot &state() const
    {
        return service_.snapshot();
    }
    void refresh();

private:
    bool alive() const
    {
        return !lifetime_.isNull();
    }
    void report(const core::Result<domain::RobotStateSnapshot> &result);
    IRobot3dView &view_;
    QPointer<QObject> lifetime_;
    application::Robot3dService &service_;
    application::SimulationService &simulation_;
    QTimer timer_;
    scene::RobotSceneSnapshot scene_;
    std::vector<std::string> robotTypes_;
};
}
}
