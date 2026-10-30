#include "robot3d/shell/RobotViewport.h"
#include "robot3d/shell/opengl/OpenGlRobotViewport.h"
#include <QStackedLayout>
#include <QTimer>
#ifdef ROBOT3D_WITH_VTK
#include "robot3d/shell/vtk/VtkRobotViewport.h"
#endif
RobotViewport::RobotViewport(QWidget *parent)
    : QWidget(parent), gl_(new OpenGlRobotViewport(this)), stack_(new QStackedLayout(this))
{
    setObjectName("robotViewport");
    stack_->setMargin(0);
    stack_->addWidget(gl_);
    connect(gl_, &OpenGlRobotViewport::workpieceSelected, this, [this](const QString &id){if(active_=="opengl"&&pending_.isEmpty())emit workpieceSelected(id);});
    connect(gl_, &OpenGlRobotViewport::rendererReady, this,
            [this]
            {
                ready("opengl");
            });
    connect(gl_, &OpenGlRobotViewport::renderFailed, this,
            [this](const QString &e)
            {
                if (pending_ == "opengl")
                    failSwitch(e);
                else if (active_ == "opengl")
                    emit renderFailed(e);
            });
    connect(gl_, &OpenGlRobotViewport::framePresented, this,
            [this](qulonglong r)
            {
                presented("opengl", r);
            });
    connect(gl_, &OpenGlRobotViewport::coordinateRequested, this,
            [this](const QString &id)
            {
                if (active_ == "opengl")
                    emit coordinateRequested(id);
            });
    connect(gl_, &OpenGlRobotViewport::displayOptionsRequested, this,
            [this](const robot3d::scene::RobotDisplayOptions &o)
            {
                if (active_ == "opengl")
                    emit displayOptionsRequested(o);
            });
    connect(gl_, &OpenGlRobotViewport::poseConfirmed, this,
            [this](const robot3d::domain::JointPoseDeg &j, const robot3d::domain::EulerPoseDeg &p)
            {
                if (active_ == "opengl")
                    emit poseConfirmed(j, p);
            });
    connect(gl_, &OpenGlRobotViewport::poseCancelled, this,
            [this]
            {
                if (active_ == "opengl")
                    emit poseCancelled();
            });
    const auto requested = QString::fromUtf8(qgetenv("DEMO_RENDERER")).toLower();
    if (requested == "vtk" && vtkAvailable())
    {
        active_ = "vtk";
        ensureVtk();
#ifdef ROBOT3D_WITH_VTK
        stack_->setCurrentWidget(vtk_);
#endif
    }
    else if (!requested.isEmpty() && requested != "opengl")
    {
        QTimer::singleShot(0, this,
                           [this]
                           {
                               emit renderFailed("请求的渲染后端不可用，已使用 OpenGL");
                           });
    }
}
RobotViewport::~RobotViewport()
{
}
bool RobotViewport::vtkAvailable()
{
#ifdef ROBOT3D_WITH_VTK
    return true;
#else
    return false;
#endif
}
void RobotViewport::ensureVtk()
{
#ifdef ROBOT3D_WITH_VTK
    if (vtk_)
        return;
    vtk_ = new VtkRobotViewport(this);
    // Apply visibility before the stacked layout measures the new viewport.
    vtk_->showState(model_);
    stack_->addWidget(vtk_);
    connect(vtk_, &VtkRobotViewport::workpieceSelected, this, [this](const QString &id){if(active_=="vtk"&&pending_.isEmpty())emit workpieceSelected(id);});
    connect(vtk_, &VtkRobotViewport::rendererReady, this,
            [this]
            {
                ready("vtk");
            });
    connect(vtk_, &VtkRobotViewport::renderFailed, this,
            [this](const QString &e)
            {
                if (pending_ == "vtk")
                    failSwitch(e);
                else if (active_ == "vtk")
                    emit renderFailed(e);
            });
    connect(vtk_, &VtkRobotViewport::framePresented, this,
            [this](qulonglong r)
            {
                presented("vtk", r);
            });
    connect(vtk_, &VtkRobotViewport::poseConfirmed, this,
            [this](const robot3d::domain::JointPoseDeg &j, const robot3d::domain::EulerPoseDeg &p)
            {
                if (active_ == "vtk")
                    emit poseConfirmed(j, p);
            });
    connect(vtk_, &VtkRobotViewport::poseCancelled, this,
            [this]
            {
                if (active_ == "vtk")
                    emit poseCancelled();
            });
    connect(vtk_, &VtkRobotViewport::coordinateRequested, this,
            [this](const QString &id)
            {
                if (active_ == "vtk")
                    emit coordinateRequested(id);
            });
    connect(vtk_, &VtkRobotViewport::displayOptionsRequested, this,
            [this](const robot3d::scene::RobotDisplayOptions &o)
            {
                if (active_ == "vtk")
                    emit displayOptionsRequested(o);
            });
#endif
}
bool RobotViewport::prepareModel(robot3d::domain::RobotConfigurationPtr config, std::string &error)
{
#ifdef ROBOT3D_WITH_VTK
    if (active_ == "vtk")
        return vtk_->prepareModel(config, error);
#endif
    return gl_->prepareModel(config, error);
}
void RobotViewport::showState(const robot3d::presentation::Robot3dDisplayModel &model)
{
    model_ = model;
    gl_->showState(model);
#ifdef ROBOT3D_WITH_VTK
    if (vtk_)
        vtk_->showState(model);
#endif
}
void RobotViewport::submitScene(const robot3d::scene::RobotSceneSnapshot &scene)
{
    scene_ = scene;
#ifdef ROBOT3D_WITH_VTK
    if (active_ == "vtk")
        vtk_->submitScene(scene);
    else
#endif
        gl_->submitScene(scene);
}

bool RobotViewport::prepareWorkpieces(const robot3d::scene::RobotSceneSnapshot &scene,std::string &error)
{
    if(!pending_.isEmpty()){error="Renderer is switching; please retry";return false;}
#ifdef ROBOT3D_WITH_VTK
    if(active_=="vtk")return vtk_->prepareWorkpieces(scene,error);
#endif
    return gl_->prepareWorkpieces(scene,error);
}
void RobotViewport::focusWorkpiece(const QString &id)
{
#ifdef ROBOT3D_WITH_VTK
    if(active_=="vtk"){vtk_->focusWorkpiece(id);return;}
#endif
    gl_->focusWorkpiece(id);
}
void RobotViewport::fitAllWorkpieces()
{
#ifdef ROBOT3D_WITH_VTK
    if(active_=="vtk"){vtk_->fitAllWorkpieces();return;}
#endif
    gl_->fitAllWorkpieces();
}

QImage RobotViewport::capture()
{
#ifdef ROBOT3D_WITH_VTK
    if (active_ == "vtk")
        return vtk_->capture();
#endif
    return gl_->grabFramebuffer();
}
bool RobotViewport::requestBackend(const QString &name)
{
    if (name != "opengl" && name != "vtk")
        return false;
    if (name == "vtk" && !vtkAvailable())
    {
        emit renderFailed("当前构建未启用 VTK");
        return false;
    }
    if (name == active_)
    {
        if (!pending_.isEmpty())
        {
            pending_.clear();
            candidatePrepared_ = false;
        }
        stack_->setCurrentWidget(gl_);
#ifdef ROBOT3D_WITH_VTK
        if (name == "vtk")
            stack_->setCurrentWidget(vtk_);
#endif
        emit backendChanged(active_);
        return true;
    }
    pending_ = name;
    candidatePrepared_ = false;
#ifdef ROBOT3D_WITH_VTK
    if (name == "vtk")
    {
        ensureVtk();
        stack_->setCurrentWidget(vtk_);
        if (vtk_->isRendererReady())
            finishSwitch();
    }
    else
#endif
    {
        stack_->setCurrentWidget(gl_);
        if (gl_->isRendererReady())
            finishSwitch();
    }
    return true;
}
void RobotViewport::ready(const QString &name)
{
    if (name == pending_)
    {
        finishSwitch();
        return;
    }
    if (name != active_)
        return;
    if (scene_.state.configuration)
    {
        std::string error;
        if (!prepareModel(scene_.state.configuration, error))
        {
            emit renderFailed(QString::fromStdString(error));
            return;
        }
        submitScene(scene_);
    }
    else
        emit rendererReady();
}
void RobotViewport::finishSwitch()
{
    if (pending_.isEmpty())
        return;
    if (!scene_.state.configuration)
    {
        active_ = pending_;
        pending_.clear();
        emit backendChanged(active_);
        emit rendererReady();
        return;
    }
    const QString next = pending_;
    auto camera = gl_->cameraState();
    auto savedCamera = gl_->savedCameraState();
#ifdef ROBOT3D_WITH_VTK
    if (active_ == "vtk")
    {
        camera = vtk_->cameraState();
        savedCamera = vtk_->savedCameraState();
    }
#endif
    std::string error;
    bool ok = false;
#ifdef ROBOT3D_WITH_VTK
    if (next == "vtk")
        ok = vtk_->prepareModel(scene_.state.configuration, error);
    else
#endif
        ok = gl_->prepareModel(scene_.state.configuration, error);
    if (!ok)
    {
        failSwitch(QString::fromStdString(error));
        return;
    }
    bool workpiecesOk=false;
#ifdef ROBOT3D_WITH_VTK
    if(next=="vtk")workpiecesOk=vtk_->prepareWorkpieces(scene_,error);else
#endif
        workpiecesOk=gl_->prepareWorkpieces(scene_,error);
    if(!workpiecesOk){failSwitch(QString::fromStdString(error));return;}
    candidatePrepared_ = true;
#ifdef ROBOT3D_WITH_VTK
    if (next == "vtk")
    {
        vtk_->showState(model_);
        vtk_->submitScene(scene_);
        vtk_->setCameraState(camera);
        vtk_->setSavedCameraState(savedCamera);
        vtk_->capture();
    }
    else
#endif
    {
        gl_->showState(model_);
        gl_->submitScene(scene_);
        gl_->setCameraState(camera);
        gl_->setSavedCameraState(savedCamera);
        gl_->grabFramebuffer();
    }
}
void RobotViewport::presented(const QString &name, qulonglong revision)
{
    if (name == pending_ && candidatePrepared_ && revision == scene_.state.revision)
    {
        active_ = pending_;
        pending_.clear();
        candidatePrepared_ = false;
        emit backendChanged(active_);
    }
    if (name == active_ && pending_.isEmpty())
        emit framePresented(revision);
}
void RobotViewport::failSwitch(const QString &message)
{
    pending_.clear();
    candidatePrepared_ = false;
    stack_->setCurrentWidget(gl_);
#ifdef ROBOT3D_WITH_VTK
    if (active_ == "vtk")
        stack_->setCurrentWidget(vtk_);
#endif
    emit backendChanged(active_);
    emit renderFailed(message);
}
