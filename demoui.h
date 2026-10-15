#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <memory>
#include "robot3d/presentation/IRobot3dView.h"
class RobotViewport;
class QSlider;
class QCheckBox;
namespace robot3d { namespace composition { class Robot3dComposition; } }
class DemoUi : public QWidget, public robot3d::presentation::IRobot3dView {
    Q_OBJECT
public:
    explicit DemoUi(QWidget* parent=nullptr);
    ~DemoUi();
    bool prepareModel(robot3d::domain::RobotConfigurationPtr config,std::string& error) override;
    void showState(const robot3d::presentation::Robot3dDisplayModel& model) override;
    void submitScene(const robot3d::scene::RobotSceneSnapshot& scene) override;
    void showError(const std::string& error) override;
    void appendLog(const std::string& message) override;
private slots:
    void OnRobotTypeChanged(int index);
    void OnJointSliderChanged();
    void OnSimToggled(bool checked);
    void OnCoordSysChanged(int index);
    void OnResetJoint();
    void OnDisplayFlagChanged();
    void OnFramePresented(qulonglong revision);
private:
    QWidget* BuildControlPanel();
    void OnCheckDump();
    std::unique_ptr<robot3d::composition::Robot3dComposition> composition_;
    robot3d::presentation::Robot3dDisplayModel model_;
    RobotViewport* m_viewport=nullptr;
    QComboBox* m_backendCb=nullptr;
    QComboBox *m_robotTypeCb=nullptr,*m_coordSysSelectCb=nullptr;
    QSlider* m_jointSlider[6]={};
    QLabel* m_jointLabel[6]={};
    QLabel *m_poseLabel=nullptr,*m_speedLabel=nullptr;
    QCheckBox *m_simCb=nullptr,*m_coordSysCb=nullptr,*m_waypointCb=nullptr,*m_pathCb=nullptr,*m_toolCb=nullptr;
    QPlainTextEdit* m_logEdit=nullptr;
    bool m_updatingSliders=false,m_dumpShotDone=false,m_dumpInProgress=false;
    int dumpStage_=0;
};
