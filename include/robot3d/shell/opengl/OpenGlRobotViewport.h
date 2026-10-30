#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QVector2D>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <memory>
#include "robot3d/rendering/opengl/OpenGlRobotSceneRenderer.h"
#include "robot3d/presentation/Robot3dDisplayModel.h"
class OpenGlRobotViewport : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit OpenGlRobotViewport(QWidget *parent = nullptr);
    ~OpenGlRobotViewport();
    bool prepareModel(robot3d::domain::RobotConfigurationPtr config, std::string &error);
    void showState(const robot3d::presentation::Robot3dDisplayModel &model);
    void submitScene(const robot3d::scene::RobotSceneSnapshot &scene);
    bool isRendererReady() const
    {
        return bool(renderer_);
    }
    bool prepareWorkpieces(const robot3d::scene::RobotSceneSnapshot &scene, std::string &error);
    void focusWorkpiece(const QString &id);
    void fitAllWorkpieces();
    void cleanupGL();
    robot3d::rendering::CameraState cameraState() const
    {
        return camera_;
    }
    robot3d::rendering::CameraState savedCameraState() const
    {
        return savedCamera_;
    }
    void setSavedCameraState(const robot3d::rendering::CameraState &state)
    {
        savedCamera_ = state;
    }
    void setCameraState(const robot3d::rendering::CameraState &state)
    {
        camera_ = state;
        const bool blocked = zoomSlider->blockSignals(true);
        zoomSlider->setValue(qRound((camera_.scale - .4) / .015));
        zoomSlider->blockSignals(blocked);
        update();
    }
signals:
    void workpieceSelected(const QString &id);
    void rendererReady();
    void renderFailed(const QString &error);
    void framePresented(qulonglong revision);
    void coordinateRequested(const QString &id);
    void displayOptionsRequested(const robot3d::scene::RobotDisplayOptions &options);
    void poseConfirmed(const robot3d::domain::JointPoseDeg &joints, const robot3d::domain::EulerPoseDeg &tcp);
    void poseCancelled();
    void EndEditPose_signals();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
private slots:
    void zoomChange_slot();
    void ZoomSlider_valueChanged(int value);
    void Recovery3D_slot();
    void RotateTranslatChange_slot();
    void showEndTool_slot();
    void WaypointOperateChange_slot();
    void UpdateCoordSysChange_slot(int index);
    void EditRobotPoseBtn_slot();

private:
    void InitUI();
    std::unique_ptr<robot3d::rendering::OpenGlRobotSceneRenderer> renderer_;
    robot3d::rendering::CameraState camera_, savedCamera_;
    robot3d::scene::RobotSceneSnapshot scene_;
    robot3d::scene::RobotDisplayOptions options_;
    QMatrix4x4 projection;
    QVector2D curPos;
    QPointF workpiecePressPoint_;
    bool workpieceDragged_ = false;
    bool dirty_ = false, dragging_ = false, install_ = false;
    int rotateTranslateStatus = 0;
    QSlider *zoomSlider = nullptr;
    QPushButton *endWorkpieceButton_ = nullptr;
    QWidget *coordSysWidget = nullptr, *robotOperateWidget = nullptr, *zoomWidget = nullptr,
            *poseOperateWidget = nullptr, *waypointOperateWidget = nullptr, *tcpSpeedWidget = nullptr;
    QList<QPushButton *> waypointOpPb;
    QComboBox *selCoordSysCb = nullptr;
    QLabel *curTCPNameLabel = nullptr, *curTcpSpeedLabel = nullptr;
};
