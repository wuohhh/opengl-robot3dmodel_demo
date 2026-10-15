#include "robot3d/composition/Robot3dComposition.h"
#include "robot3d/infrastructure/XmlRobotConfigRepository.h"
#include "robot3d/presentation/Robot3dPresenter.h"
#include "3dmodel/robot3dglwidget.h"
#include "robot3d/rendering/RobotViewport.h"
namespace robot3d { namespace composition {
struct Robot3dComposition::Implementation {
    // Reverse member destruction releases the Presenter/timer before its services.
    infrastructure::XmlRobotConfigRepository repository_;
    application::Robot3dService service_;
    application::SimulationService simulation_;
    std::unique_ptr<presentation::Robot3dPresenter> presenter_;
    Implementation(presentation::IRobot3dView& v,QObject& life,const std::string& root)
        : repository_(root),service_(repository_),presenter_(new presentation::Robot3dPresenter(v,life,service_,simulation_)) {}
    template<class Viewport> void bind(Viewport& viewport) {
    auto* p=presenter_.get();
    QObject::connect(&viewport,&Viewport::rendererReady,p,[p] {
        auto type=qgetenv("DEMO_ROBOT_TYPE"); p->start(p->state().configuration?p->state().configuration->id:(type.isEmpty()?"G6L-3-20":type.toStdString()));
    });
    QObject::connect(&viewport,&Viewport::coordinateRequested,p,[p](const QString& id){ p->onCoordinateRequested(id.toStdString()); });
    QObject::connect(&viewport,&Viewport::displayOptionsRequested,p,&presentation::Robot3dPresenter::onDisplayOptionsRequested);
    }
};
Robot3dComposition::Robot3dComposition(presentation::IRobot3dView& v,QObject& life,Robot3dGLWidget& viewport,const std::string& root)
    : implementation_(new Implementation(v,life,root)) { implementation_->bind(viewport); }
Robot3dComposition::Robot3dComposition(presentation::IRobot3dView& v,QObject& life,RobotViewport& viewport,const std::string& root)
    : implementation_(new Implementation(v,life,root)) { implementation_->bind(viewport); }
Robot3dComposition::~Robot3dComposition() {}
presentation::Robot3dPresenter& Robot3dComposition::presenter() { return *implementation_->presenter_; }
}}
