#include "robot3d/shell/vtk/VtkRobotViewport.h"
#include "robot3d/rendering/vtk/VtkRobotSceneRenderer.h"
#include <QVTKOpenGLNativeWidget.h>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QAbstractItemView>
#include <QStyledItemDelegate>
#include <QDebug>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <functional>
namespace {
class VtkRobotRenderSurface : public QVTKOpenGLNativeWidget {
public:
    std::function<void()> ready;
    explicit VtkRobotRenderSurface(QWidget* parent):QVTKOpenGLNativeWidget(parent) {
        auto format=QVTKOpenGLNativeWidget::defaultFormat();
        // Both widgets share a Qt top-level window; retain the legacy GLSL-compatible profile.
        format.setProfile(QSurfaceFormat::CompatibilityProfile); setFormat(format);
    }
protected:
    void initializeGL() override {
        QVTKOpenGLNativeWidget::initializeGL();
        const auto format=context()->format();
        qInfo()<<"VTK OpenGL context"<<format.majorVersion()<<format.minorVersion()<<format.profile()
               <<reinterpret_cast<const char*>(context()->functions()->glGetString(GL_RENDERER));
        QTimer::singleShot(0,this,[this] { if(ready) ready(); });
    }
};
}
struct VtkRobotViewport::Implementation {
    VtkRobotRenderSurface* surface;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> window;
    vtkSmartPointer<vtkRenderer> vtkRenderer_;
    vtkSmartPointer<vtkCallbackCommand> observer;
    std::unique_ptr<robot3d::rendering::VtkRobotSceneRenderer> renderer;
    QString error;
    bool ready=false;
    explicit Implementation(QWidget* owner):surface(new VtkRobotRenderSurface(owner)),
        window(vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New()),
        vtkRenderer_(vtkSmartPointer<vtkRenderer>::New()),observer(vtkSmartPointer<vtkCallbackCommand>::New()) {
        window->SetMultiSamples(0); window->AddRenderer(vtkRenderer_); surface->setRenderWindow(window);
        // Qt intent handlers own interaction, matching the original viewport.
        if(window->GetInteractor()) window->GetInteractor()->SetInteractorStyle(nullptr);
        renderer.reset(new robot3d::rendering::VtkRobotSceneRenderer(vtkRenderer_));
        observer->SetClientData(this);
        observer->SetCallback([](vtkObject*,unsigned long,void* client,void* message) {
            auto* self=static_cast<Implementation*>(client);
            self->error=message?QString::fromUtf8(static_cast<const char*>(message)):QString("VTK rendering failed");
        });
        window->AddObserver(vtkCommand::ErrorEvent,observer);
        vtkRenderer_->AddObserver(vtkCommand::ErrorEvent,observer);
    }
    ~Implementation() {
        window->RemoveObservers(vtkCommand::ErrorEvent,observer);
        vtkRenderer_->RemoveObservers(vtkCommand::ErrorEvent,observer);
    }
};
VtkRobotViewport::VtkRobotViewport(QWidget* parent):QWidget(parent),impl_(new Implementation(this)) {
    setObjectName("vtkRobotViewport");
    impl_->surface->setObjectName("vtkSurface");
    impl_->surface->installEventFilter(this);
    impl_->surface->ready=[this] {
        impl_->ready=true; emit rendererReady(); requestRender();
    };
    InitUI(); impl_->surface->lower();
}
VtkRobotViewport::~VtkRobotViewport() {
    impl_->surface->ready=nullptr;
    impl_->surface->makeCurrent();
    impl_->renderer.reset();
    impl_->surface->setRenderWindow(static_cast<vtkGenericOpenGLRenderWindow*>(nullptr));
    impl_->surface->doneCurrent();
}
bool VtkRobotViewport::isRendererReady() const { return impl_->ready&&impl_->surface->isValid(); }
const robot3d::rendering::VtkRobotSceneRenderer* VtkRobotViewport::renderer() const { return impl_->renderer.get(); }
bool VtkRobotViewport::prepareModel(robot3d::domain::RobotConfigurationPtr c,std::string& error) {
    if(!isRendererReady()) { error="VTK 视口尚未就绪"; return false; }
    QString message;
    impl_->surface->makeCurrent();
    const bool ok=impl_->renderer->prepareModel(c,message); impl_->surface->doneCurrent();
    error=message.toStdString(); return ok;
}
void VtkRobotViewport::requestRender() {
    if(scheduled_) return;
    scheduled_=true;
    QTimer::singleShot(0,this,[this] { scheduled_=false; renderNow(); });
}
void VtkRobotViewport::renderNow() {
    if(rendering_||!isRendererReady()||!scene_.state.configuration) return;
    rendering_=true; impl_->surface->makeCurrent();
    if(dirty_) {
        impl_->renderer->apply(scene_);
        camera_.coordinateMatrix=impl_->renderer->coordinateMatrix();
        camera_.rotationCenter=install_?QVector3D():impl_->renderer->rotationCenter();
        dirty_=false;
    }
    impl_->renderer->updateCamera(camera_);
    impl_->error.clear(); impl_->window->Render();
    impl_->surface->doneCurrent(); impl_->surface->update(); rendering_=false;
    if(!impl_->error.isEmpty()) { emit renderFailed(impl_->error); return; }
    emit framePresented(scene_.state.revision);
}
QImage VtkRobotViewport::capture() { renderNow(); return impl_->surface->grabFramebuffer(); }
void VtkRobotViewport::setCameraState(const robot3d::rendering::CameraState& state) {
    camera_=state;
    QSignalBlocker block(zoomSlider); zoomSlider->setValue(qRound((camera_.scale-.4)/.015));
    requestRender();
}
void VtkRobotViewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event); impl_->surface->setGeometry(rect()); requestRender();
}
bool VtkRobotViewport::eventFilter(QObject* target,QEvent* event) {
    if(target!=impl_->surface) return QWidget::eventFilter(target,event);
    if(event->type()==QEvent::MouseButtonPress) {
        auto* e=static_cast<QMouseEvent*>(event); curPos=QVector2D(e->localPos()); dragging_=true; return true;
    }
    if(event->type()==QEvent::MouseButtonRelease) { dragging_=false; return true; }
    if(event->type()==QEvent::MouseMove&&dragging_) {
        const auto pos=QVector2D(static_cast<QMouseEvent*>(event)->localPos()); const auto diff=pos-curPos; curPos=pos;
        if(rotateTranslateStatus==0) {
            const auto axis=QVector3D(0,diff.y(),diff.x()).normalized();
            const auto transformed=camera_.coordinateMatrix*QVector4D(axis,0);
            camera_.rotation=QQuaternion::fromAxisAndAngle(QVector3D(transformed),diff.length()/5)*camera_.rotation;
        } else camera_.translation+=QVector3D(0,diff.x()/5,-diff.y()/5);
        requestRender(); return true;
    }
    return QWidget::eventFilter(target,event);
}
void VtkRobotViewport::showState(const robot3d::presentation::Robot3dDisplayModel& m) {
    options_=m.options;
    { QSignalBlocker block(selCoordSysCb);
      bool changed=selCoordSysCb->count()!=int(m.state.coordinates.size());
      for(int i=0;!changed&&i<selCoordSysCb->count();++i) changed=selCoordSysCb->itemData(i).toString().toStdString()!=m.state.coordinates[i].id || selCoordSysCb->itemText(i).toStdString()!=m.state.coordinates[i].name;
      if(changed) { selCoordSysCb->clear(); for(const auto& cs:m.state.coordinates) selCoordSysCb->addItem(QString::fromUtf8(cs.name.c_str()),QString::fromStdString(cs.id)); }
      selCoordSysCb->setCurrentIndex(selCoordSysCb->findData(QString::fromStdString(m.state.coordinateId))); }
    endWorkpieceButton_->setIcon(QIcon(options_.endWorkpiece?":/resource/image/end_tool_show.png":":/resource/image/end_tool_hide.png"));
    curTCPNameLabel->setText(QString::fromUtf8("默认 TCP"));
    curTcpSpeedLabel->setText(QString::number(m.state.simulatedTcpSpeed,'f',3)+" mm/s");
    coordSysWidget->setVisible(options_.coordinatePanel); robotOperateWidget->setVisible(options_.operatePanel);
    zoomWidget->setVisible(options_.zoomPanel); poseOperateWidget->setVisible(options_.posePanel);
    waypointOperateWidget->setVisible(options_.waypointPanel); tcpSpeedWidget->setVisible(options_.speedPanel);
    for(int i=3;i<5;++i) { QSignalBlocker block(waypointOpPb[i]); waypointOpPb[i]->setChecked(i==3?options_.waypoints:options_.trajectory); }
}
void VtkRobotViewport::submitScene(const robot3d::scene::RobotSceneSnapshot& s) {
    if(s.options.install!=install_) {
        if(s.options.install) { savedCamera_=camera_; camera_=robot3d::rendering::CameraState(); camera_.translation=QVector3D(0,0,20); }
        else camera_=savedCamera_;
        install_=s.options.install;
    }
    scene_=s; dirty_=true; requestRender();
}
void VtkRobotViewport::ZoomSlider_valueChanged(int value) { camera_.scale=.4+value*.015; requestRender(); }
void VtkRobotViewport::zoomChange_slot() { auto* b=qobject_cast<QPushButton*>(sender()); if(b) zoomSlider->setValue(zoomSlider->value()+(b->objectName()=="zoomin"?1:-1)); }
void VtkRobotViewport::Recovery3D_slot() { camera_.rotation=QQuaternion(); camera_.translation=install_?QVector3D(0,0,20):QVector3D(); camera_.scale=1; zoomSlider->setValue(40); requestRender(); }
void VtkRobotViewport::RotateTranslatChange_slot() { rotateTranslateStatus=1-rotateTranslateStatus; auto* b=qobject_cast<QPushButton*>(sender()); if(b) b->setIcon(QIcon(rotateTranslateStatus?":/resource/image/T_R_change.png":":/resource/image/R_T_change.png")); }
void VtkRobotViewport::showEndTool_slot() { auto next=options_; next.endWorkpiece=!next.endWorkpiece; emit displayOptionsRequested(next); }
void VtkRobotViewport::WaypointOperateChange_slot() { auto next=options_; next.waypoints=waypointOpPb[3]->isChecked(); next.trajectory=waypointOpPb[4]->isChecked(); emit displayOptionsRequested(next); }
void VtkRobotViewport::UpdateCoordSysChange_slot(int index) { if(index>=0) emit coordinateRequested(selCoordSysCb->itemData(index).toString()); }
void VtkRobotViewport::EditRobotPoseBtn_slot() { auto* b=qobject_cast<QPushButton*>(sender()); if(b&&b->objectName()=="ok") emit poseConfirmed(scene_.state.joints,scene_.state.tcp); else emit poseCancelled(); emit EndEditPose_signals(); }
void VtkRobotViewport::InitUI()
{
    //放大缩小功能
    QStringList zoomNames;
    zoomNames << "zoomin" << "zoomout";

    // 缩放功能：zoomin / zoomout 按钮 + 垂直滑条
    zoomWidget = new QWidget(this);
    zoomWidget->setObjectName("zoomWidget");
    QVBoxLayout* zoomLayout = new QVBoxLayout();
    zoomLayout->setSpacing(10);
    zoomWidget->setLayout(zoomLayout);

//    zoomWidget->setProperty("helpPrompt","showHelpPrompt");
//    zoomWidget->setProperty("helpPromptExplain",tr("图形缩放"));
//    zoomWidget->setProperty("helpPromptExplainWidgetObjName","graphicsZoomExplainWidget");
//    zoomWidget->setProperty("helpPromptExplainPos",0);
//    zoomWidget->setProperty("helpPromptExplainFlag",0);
//    zoomWidget->setProperty("helpPromptExplainDirect",1);
//    zoomWidget->setProperty("helpPromptExplainArrowDirect",3);
//    zoomWidget->setProperty("helpPromptExplainMargin","0#80#0#80");

    zoomLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Fixed,QSizePolicy::Expanding));
    for(int index = 0; index < zoomNames.count(); index++)
    {
        QPushButton* zoomPB = new QPushButton(this);
        zoomPB->setObjectName(zoomNames.at(index));
        zoomPB->setStyleSheet(QString("QPushButton{border:none;background-color: #ffffff;qproperty-icon: url(:/resource/image/%1.png); qproperty-iconSize: 16px 16px;}"
                                      "QPushButton:pressed{border:1px solid #32b67a;}").arg(zoomNames.at(index)));
        // 长按不松开时自动重复发送 clicked 信号，便于连续缩放
        zoomPB->setAutoRepeat(true);
        zoomPB->setAutoRepeatDelay(100); //按下按钮不松开，500ms后开始自动发送信号
        zoomPB->setAutoRepeatInterval(100); //每隔100ms发送一次点击信号
        QObject::connect(zoomPB, SIGNAL(clicked()), this, SLOT(zoomChange_slot()));
        zoomLayout->addWidget(zoomPB);

        if(index == 0)
        {
            zoomSlider = new QSlider(Qt::Vertical, this);
            zoomSlider->setRange(0,100);
            zoomSlider->setPageStep(1);
            zoomSlider->setTickInterval(10);  // 设置刻度间隔
            zoomSlider->setTickPosition(QSlider::TicksBelow);  //刻度在上方
            // 默认值 40 → scalePara = 0.4 + 4*0.15 = 1.0（原始比例）
            zoomSlider->setValue(40);
            connect(zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(ZoomSlider_valueChanged(int)));
            zoomLayout->addWidget(zoomSlider);
        }
    }
    zoomLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Fixed,QSizePolicy::Expanding));

    //坐标系选择
    // 坐标系选择：下拉框 + 当前 TCP 名称
    coordSysWidget = new QWidget(this);
    QHBoxLayout* coordSysLayout = new QHBoxLayout();
    coordSysLayout->setSpacing(10);
    coordSysWidget->setLayout(coordSysLayout);

    QLabel* selCoordSysLabel = new QLabel(tr("坐标系:"),this);
    selCoordSysLabel->setStyleSheet("width:50px;");
    selCoordSysCb = new QComboBox(this);
    selCoordSysCb->setObjectName("selCoordSysCb");
    // 下拉项较多时显示滚动条
    selCoordSysCb->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);//设置滚动条
    selCoordSysCb->setStyleSheet("QComboBox{width:110px;}");
    connect(selCoordSysCb,SIGNAL(currentIndexChanged(int)),this,SLOT(UpdateCoordSysChange_slot(int)));
    QStyledItemDelegate *styledItemDelegate = new QStyledItemDelegate();//为设置下拉框item高度生效
    selCoordSysCb ->setItemDelegate(styledItemDelegate);

    QLabel* curTCPTitleLabel = new QLabel("TCP:",this);
    curTCPTitleLabel->setStyleSheet("width:50px;");
    curTCPNameLabel = new QLabel(this);
    curTCPNameLabel->setStyleSheet("QLabel{min-width:120px;border: 1px solid #efefef;}");

    coordSysLayout->addWidget(selCoordSysLabel);
    coordSysLayout->addWidget(selCoordSysCb);
    coordSysLayout->addItem(new QSpacerItem(30,1,QSizePolicy::Fixed,QSizePolicy::Fixed));
    coordSysLayout->addWidget(curTCPTitleLabel);
    coordSysLayout->addWidget(curTCPNameLabel);

    //
    // TCP 速度显示
    tcpSpeedWidget = new QWidget(this);
    QHBoxLayout* tcpSpeedLayout = new QHBoxLayout();
    tcpSpeedLayout->setContentsMargins(0,0,0,0);
    tcpSpeedWidget->setLayout(tcpSpeedLayout);

    curTcpSpeedLabel = new QLabel(QString("0.000 mm/s"),this);
    curTcpSpeedLabel->setStyleSheet("max-height:36px;min-height:36px");
    curTcpSpeedLabel->setAlignment(Qt::AlignCenter);

    tcpSpeedLayout->addWidget( new QLabel(tr("TCP速度:"),this));
    tcpSpeedLayout->addWidget(curTcpSpeedLabel);
    tcpSpeedLayout->addStretch(0);

    //
    // 位姿编辑的“确定 / 取消”（确定时回调发起编辑的按钮）
    poseOperateWidget = new QWidget(this);
    QHBoxLayout* poseOperateLayout = new QHBoxLayout();
    poseOperateWidget->setLayout(poseOperateLayout);

    QPushButton* btn_ok = new QPushButton(tr("确定"),this);
    btn_ok->setStyleSheet("QPushButton{width:60px;}");
    btn_ok->setObjectName("ok");
    QObject::connect(btn_ok, SIGNAL(clicked()), this, SLOT(EditRobotPoseBtn_slot()));

    QPushButton* btn_cancel = new QPushButton(tr("取消"), this);
    btn_cancel->setStyleSheet("QPushButton{width:65px;border-color: #e82828;background-color: #e82828;}"
                              "QPushButton:pressed{background-color: #9E0F0F;}");
    btn_cancel->setObjectName("cancel");
    QObject::connect(btn_cancel, SIGNAL(clicked()), this, SLOT(EditRobotPoseBtn_slot()));

    poseOperateLayout->setMargin(0);
    poseOperateLayout->addStretch(0);
    poseOperateLayout->addWidget(btn_ok);
    poseOperateLayout->addWidget(btn_cancel);

    //
    // 机器人操作按钮：置初始位 / 平移旋转切换 / 末端工件显隐
    robotOperateWidget = new QWidget(this);
    QVBoxLayout *robotOperateLayout = new QVBoxLayout();
    robotOperateLayout->setContentsMargins(5,5,0,5);
    robotOperateLayout->setSpacing(10);
    robotOperateWidget->setLayout(robotOperateLayout);

    QPushButton* recovery3dPb =new QPushButton(QIcon(":/resource/image/recovery3d.png"),"",this);
    recovery3dPb->setObjectName("recovery3dPb");
    recovery3dPb->setIconSize(QSize(36,36));
    recovery3dPb->setFixedSize(QSize(40,40));
    recovery3dPb->setStyleSheet("QPushButton{border:none;background:transparent;}"
                                  "QPushButton:pressed{border:1px solid #32b67a;}");
    QObject::connect(recovery3dPb, SIGNAL(clicked()), this, SLOT(Recovery3D_slot()));

//    recovery3dPb->setProperty("helpPrompt","showHelpPrompt");
//    recovery3dPb->setProperty("helpPromptExplain",tr("置初始位"));
//    recovery3dPb->setProperty("helpPromptExplainWidgetObjName","recovery3dExplainWidget");
//    recovery3dPb->setProperty("helpPromptExplainPos",2);
//    recovery3dPb->setProperty("helpPromptExplainFlag",0);
//    recovery3dPb->setProperty("helpPromptExplainDirect",0);
//    recovery3dPb->setProperty("helpPromptExplainArrowDirect",2);
//    recovery3dPb->setProperty("helpPromptExplainMargin","0#0#0#0");

    // 初始为旋转模式
    this->rotateTranslateStatus = 0;
     QPushButton* rotateTranslatePb =new QPushButton(QIcon(":/resource/image/R_T_change.png"),"",this);
     rotateTranslatePb->setObjectName("rotateTranslatePb");
     rotateTranslatePb->setIconSize(QSize(36,36));
     rotateTranslatePb->setFixedSize(QSize(40,40));
     rotateTranslatePb->setStyleSheet("QPushButton{border:none;background:transparent;}"
                                   "QPushButton:pressed{border:1px solid #32b67a;}");
     QObject::connect(rotateTranslatePb, SIGNAL(clicked()), this, SLOT(RotateTranslatChange_slot()));

    // 初始不显示末端工件，按钮图标为 end_tool_hide

    QPushButton *showEndToolPb = new QPushButton(QIcon(":/resource/image/end_tool_hide.png"),"",this);
    endWorkpieceButton_=showEndToolPb;
    showEndToolPb->setObjectName("showEndToolPb");
    showEndToolPb->setIconSize(QSize(36, 36));
    showEndToolPb->setFixedSize(QSize(40, 40));
    showEndToolPb->setStyleSheet("QPushButton{border:none;background:transparent;}"
                                 "QPushButton:pressed{border:1px solid #32b67a;}");

    QObject::connect(showEndToolPb, SIGNAL(clicked()), this, SLOT(showEndTool_slot()));

//     rotateTranslatePb->setProperty("helpPrompt","showHelpPrompt");
//     rotateTranslatePb->setProperty("helpPromptExplain",tr("平移旋转"));
//     rotateTranslatePb->setProperty("helpPromptExplainWidgetObjName","R_T_changeExplainWidget");
//     rotateTranslatePb->setProperty("helpPromptExplainPos",2);
//     rotateTranslatePb->setProperty("helpPromptExplainFlag",0);
//     rotateTranslatePb->setProperty("helpPromptExplainDirect",0);
//     rotateTranslatePb->setProperty("helpPromptExplainArrowDirect",2);
//     rotateTranslatePb->setProperty("helpPromptExplainMargin","0#0#0#0");

     robotOperateLayout->addWidget(recovery3dPb);
     robotOperateLayout->addWidget(rotateTranslatePb);
     robotOperateLayout->addWidget(showEndToolPb);
     robotOperateLayout->addStretch(0);

     //
     // 路点操作条：5 个可勾选按钮（前 3 个当前隐藏）
     waypointOperateWidget = new QWidget(this);
     QHBoxLayout *waypointOperateLayout = new QHBoxLayout();
     waypointOperateLayout->setSpacing(10);
     waypointOperateWidget->setLayout(waypointOperateLayout);

     // 按钮图标名：machineBase / targetPose / currentPose / waypointPose / pathCurve
     QStringList waypointOpStr;
     waypointOpStr<<"machineBase"<<"targetPose"<<"currentPose"<<"waypointPose"<<"pathCurve";

     for(int index = 0; index < waypointOpStr.count(); index++)
     {
         QPushButton* opPb = new QPushButton(this);
         opPb->setObjectName(waypointOpStr.at(index));
         opPb->setStyleSheet(QString("QPushButton{width:36px;height:36px;}"
                                     "QPushButton:checked{border-image: url(:/resource/image/%1_checked.png);}"
                                     "QPushButton:!checked{border-image: url(:/resource/image/%1_unchecked.png);}").arg(waypointOpStr.at(index)));
         opPb->setCheckable(true);

         if(index == 0)
             opPb->setChecked(false);
         else
             opPb->setChecked(true);

         QObject::connect(opPb, SIGNAL(clicked()), this, SLOT(WaypointOperateChange_slot()));
         waypointOperateLayout->addWidget(opPb);
         waypointOpPb.append(opPb);

         if(index<3)
             opPb->hide();//临时注释

//         if(waypointOpStr.at(index) == "waypointPose")
//         {
//             opPb->setProperty("helpPrompt","showHelpPrompt");
//             opPb->setProperty("helpPromptExplain",tr("路点显示"));
//             opPb->setProperty("helpPromptExplainWidgetObjName","wayPointShowExplainWidget");
//             opPb->setProperty("helpPromptExplainPos",0);
//             opPb->setProperty("helpPromptExplainFlag",0);
//             opPb->setProperty("helpPromptExplainDirect",0);
//             opPb->setProperty("helpPromptExplainArrowDirect",3);
//             opPb->setProperty("helpPromptExplainMargin","0#0#0#0");
//         }
//         else if(waypointOpStr.at(index) == "pathCurve")
//         {
//             opPb->setProperty("helpPrompt","showHelpPrompt");
//             opPb->setProperty("helpPromptExplain",tr("轨迹显示"));
//             opPb->setProperty("helpPromptExplainWidgetObjName","pathCurveShowExplainWidget");
//             opPb->setProperty("helpPromptExplainPos",3);
//             opPb->setProperty("helpPromptExplainFlag",0);
//             opPb->setProperty("helpPromptExplainDirect",0);
//             opPb->setProperty("helpPromptExplainArrowDirect",0);
//             opPb->setProperty("helpPromptExplainMargin","0#0#0#0");
//         }
     }

     // 顶部：坐标系面板 + 路点操作条
     QHBoxLayout* topLayout = new QHBoxLayout();
     topLayout->addWidget(coordSysWidget);
     topLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding,QSizePolicy::Fixed));
     topLayout->addWidget(waypointOperateWidget);

     // 中部：机器人操作按钮（左）与缩放控件（右）
     QHBoxLayout* midLayout = new QHBoxLayout();
     midLayout->setSpacing(0);
     midLayout->addWidget(robotOperateWidget);
     midLayout->addItem(new QSpacerItem(1,1,QSizePolicy::Expanding,QSizePolicy::Expanding));
     midLayout->addWidget(zoomWidget);

     // 底部：TCP 速度与位姿编辑按钮
     QHBoxLayout* bottomLayout = new QHBoxLayout();
     bottomLayout->addItem(new QSpacerItem(30,1,QSizePolicy::Fixed,QSizePolicy::Fixed));
     bottomLayout->addWidget(tcpSpeedWidget);
     bottomLayout->addWidget(poseOperateWidget);
     bottomLayout->addItem(new QSpacerItem(10,1,QSizePolicy::Fixed,QSizePolicy::Fixed));

     // 三段式纵向总布局；上述子控件浮在 GL 视口之上
     QVBoxLayout* mainLayout = new QVBoxLayout(this);
     mainLayout->setMargin(0);
     mainLayout->addLayout(topLayout );
     mainLayout->addLayout(midLayout );
     mainLayout->addLayout(bottomLayout );
     mainLayout->addItem(new QSpacerItem(1,5,QSizePolicy::Fixed,QSizePolicy::Fixed));
     this->setLayout(mainLayout);
}
