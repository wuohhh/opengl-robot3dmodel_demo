#pragma once
#include <QWidget>
#include <QImage>
#include <QVector2D>
#include <memory>
#include "robot3d/presentation/Robot3dDisplayModel.h"
#include "robot3d/rendering/CameraState.h"
class QSlider;
class QPushButton;
class QComboBox;
class QLabel;
namespace robot3d
{
namespace rendering
{
class VtkRobotSceneRenderer;
}
}
class VtkRobotViewport : public QWidget
{
    Q_OBJECT
public:
    explicit VtkRobotViewport(QWidget *parent = nullptr);
    ~VtkRobotViewport();
    bool prepareModel(robot3d::domain::RobotConfigurationPtr config, std::string &error);
    void showState(const robot3d::presentation::Robot3dDisplayModel &model);
    void submitScene(const robot3d::scene::RobotSceneSnapshot &scene);
    bool isRendererReady() const;
    QImage capture();
    bool prepareWorkpieces(const robot3d::scene::RobotSceneSnapshot &scene, std::string &error);
    void focusWorkpiece(const QString &id);
    void fitAllWorkpieces();
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
    void setCameraState(const robot3d::rendering::CameraState &state);
    const robot3d::rendering::VtkRobotSceneRenderer *renderer() const;
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
    bool eventFilter(QObject *target, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
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
    void requestRender();
    void renderNow();
    /** @brief 机器人视口的私有实现，封装 VTK 渲染资源与错误观察器。 */
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
    robot3d::rendering::CameraState camera_, savedCamera_;
    robot3d::scene::RobotSceneSnapshot scene_;
    robot3d::scene::RobotDisplayOptions options_;
    QVector2D curPos;
    QPointF workpiecePressPoint_;
    bool workpieceDragged_ = false;
    bool dirty_ = false, dragging_ = false, install_ = false, scheduled_ = false, rendering_ = false;
    int rotateTranslateStatus = 0;
    QSlider *zoomSlider = nullptr;
    QPushButton *endWorkpieceButton_ = nullptr;
    QWidget *coordSysWidget = nullptr, *robotOperateWidget = nullptr, *zoomWidget = nullptr,
            *poseOperateWidget = nullptr, *waypointOperateWidget = nullptr, *tcpSpeedWidget = nullptr;
    QList<QPushButton *> waypointOpPb;
    QComboBox *selCoordSysCb = nullptr;
    QLabel *curTCPNameLabel = nullptr, *curTcpSpeedLabel = nullptr;
};
