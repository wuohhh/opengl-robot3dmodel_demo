#pragma once
#include <QWidget>
#include <QImage>
#include "robot3d/presentation/Robot3dDisplayModel.h"
class OpenGlRobotViewport;
class VtkRobotViewport;
class QStackedLayout;
class RobotViewport : public QWidget
{
    Q_OBJECT
public:
    explicit RobotViewport(QWidget *parent = nullptr);
    ~RobotViewport();
    bool prepareModel(robot3d::domain::RobotConfigurationPtr config, std::string &error);
    void showState(const robot3d::presentation::Robot3dDisplayModel &model);
    void submitScene(const robot3d::scene::RobotSceneSnapshot &scene);
    QImage capture();
    bool prepareWorkpieces(const robot3d::scene::RobotSceneSnapshot &scene, std::string &error);
    void focusWorkpiece(const QString &id);
    void fitAllWorkpieces();
    QString backend() const
    {
        return active_;
    }
    bool switching() const
    {
        return !pending_.isEmpty();
    }
    bool requestBackend(const QString &name);
    static bool vtkAvailable();
signals:
    void workpieceSelected(const QString &id);
    void rendererReady();
    void renderFailed(const QString &error);
    void framePresented(qulonglong revision);
    void backendChanged(const QString &backend);
    void coordinateRequested(const QString &id);
    void displayOptionsRequested(const robot3d::scene::RobotDisplayOptions &options);
    void poseConfirmed(const robot3d::domain::JointPoseDeg &joints, const robot3d::domain::EulerPoseDeg &tcp);
    void poseCancelled();

private:
    void ensureVtk();
    void ready(const QString &backend);
    void finishSwitch();
    void presented(const QString &backend, qulonglong revision);
    void failSwitch(const QString &message);
    OpenGlRobotViewport *gl_;
    VtkRobotViewport *vtk_ = nullptr;
    QStackedLayout *stack_;
    QString active_ = "opengl", pending_;
    bool candidatePrepared_ = false;
    robot3d::presentation::Robot3dDisplayModel model_;
    robot3d::scene::RobotSceneSnapshot scene_;
};
