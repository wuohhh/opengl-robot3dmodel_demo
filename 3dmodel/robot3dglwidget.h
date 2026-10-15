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
#include "robot3d/rendering/RobotSceneRenderer.h"
#include "robot3d/presentation/IRobot3dView.h"
// Kept for old hosts; application code uses RobotDisplayOptions instead.
enum Robot3DShowType { show_Null=0,show_CoordSys=1,show_RobotOperate=2,show_Zoom=4,show_PoseOperate=8,
 show_WaypointOperate=16,show_TargetRobot=32,show_SafePlane=64,show_TcpSpeed=128,show_Install=256,show_Tool=512 };
Q_DECLARE_FLAGS(Robot3DShowTypes,Robot3DShowType)
class Robot3dGLWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit Robot3dGLWidget(QWidget* parent=nullptr);
    ~Robot3dGLWidget();
    bool prepareModel(robot3d::domain::RobotConfigurationPtr config,std::string& error);
    void showState(const robot3d::presentation::Robot3dDisplayModel& model);
    void submitScene(const robot3d::scene::RobotSceneSnapshot& scene);
    void SetShowType(Robot3DShowTypes type);
    bool isRendererReady() const { return bool(renderer_); }
    void cleanupGL();
    robot3d::rendering::CameraState cameraState() const { return camera_; }
    robot3d::rendering::CameraState savedCameraState() const { return savedCamera_; }
    void setSavedCameraState(const robot3d::rendering::CameraState& state) { savedCamera_=state; }
    void setCameraState(const robot3d::rendering::CameraState& state) {
        camera_=state; const bool blocked=zoomSlider->blockSignals(true);
        zoomSlider->setValue(qRound((camera_.scale-.4)/.015)); zoomSlider->blockSignals(blocked); update();
    }
signals:
    void rendererReady();
    void renderFailed(const QString& error);
    void framePresented(qulonglong revision);
    void coordinateRequested(const QString& id);
    void displayOptionsRequested(const robot3d::scene::RobotDisplayOptions& options);
    void poseConfirmed(const robot3d::domain::JointPoseDeg& joints,const robot3d::domain::EulerPoseDeg& tcp);
    void poseCancelled();
    void EndEditPose_signals();
protected:
    void initializeGL() override;
    void resizeGL(int w,int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
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
    std::unique_ptr<robot3d::rendering::RobotSceneRenderer> renderer_;
    robot3d::rendering::CameraState camera_,savedCamera_;
    robot3d::scene::RobotSceneSnapshot scene_;
    robot3d::scene::RobotDisplayOptions options_;
    QMatrix4x4 projection;
    QVector2D curPos;
    bool dirty_=false,dragging_=false,install_=false;
    int rotateTranslateStatus=0;
    QSlider* zoomSlider=nullptr;
    QPushButton* endWorkpieceButton_=nullptr;
    QWidget *coordSysWidget=nullptr,*robotOperateWidget=nullptr,*zoomWidget=nullptr,*poseOperateWidget=nullptr,*waypointOperateWidget=nullptr,*tcpSpeedWidget=nullptr;
    QList<QPushButton*> waypointOpPb;
    QComboBox* selCoordSysCb=nullptr;
    QLabel *curTCPNameLabel=nullptr,*curTcpSpeedLabel=nullptr;
};
