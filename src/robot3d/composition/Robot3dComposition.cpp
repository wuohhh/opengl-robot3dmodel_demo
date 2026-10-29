#include "robot3d/composition/Robot3dComposition.h"
#include "robot3d/infrastructure/XmlRobotConfigRepository.h"
#include "robot3d/presentation/Robot3dPresenter.h"
#include "robot3d/shell/opengl/OpenGlRobotViewport.h"
#include "robot3d/shell/RobotViewport.h"
namespace robot3d
{
namespace composition
{
/** @brief 装配并持有仓储、应用服务和 Presenter，管理它们之间的依赖与销毁顺序。 */
struct Robot3dComposition::Implementation
{
    // Reverse member destruction releases the Presenter/timer before its services.
    infrastructure::XmlRobotConfigRepository repository_;       ///< 从指定机型目录读取 XML 配置的仓储。
    application::Robot3dService service_;                       ///< 使用配置仓储管理机器人领域状态的应用服务。
    application::SimulationService simulation_;                 ///< 生成关节运动与 TCP 采样的仿真服务。
    std::unique_ptr<presentation::Robot3dPresenter> presenter_; ///< 连接视图与服务的 Presenter，先于依赖服务销毁。
    Implementation(presentation::IRobot3dView &v, QObject &life, const std::string &root)
        : repository_(root), service_(repository_),
          presenter_(new presentation::Robot3dPresenter(v, life, service_, simulation_))
    {
    }
    template <class Viewport> void bind(Viewport &viewport)
    {
        auto *p = presenter_.get();
        QObject::connect(&viewport, &Viewport::rendererReady, p,
                         [p]
                         {
                             auto type = qgetenv("DEMO_ROBOT_TYPE");
                             p->start(p->state().configuration ? p->state().configuration->id
                                                               : (type.isEmpty() ? "G6L-3-20" : type.toStdString()));
                         });
        QObject::connect(&viewport, &Viewport::coordinateRequested, p,
                         [p](const QString &id)
                         {
                             p->onCoordinateRequested(id.toStdString());
                         });
        QObject::connect(&viewport, &Viewport::displayOptionsRequested, p,
                         &presentation::Robot3dPresenter::onDisplayOptionsRequested);
    }
};
Robot3dComposition::Robot3dComposition(presentation::IRobot3dView &v, QObject &life, OpenGlRobotViewport &viewport,
                                       const std::string &root)
    : implementation_(new Implementation(v, life, root))
{
    implementation_->bind(viewport);
}
Robot3dComposition::Robot3dComposition(presentation::IRobot3dView &v, QObject &life, RobotViewport &viewport,
                                       const std::string &root)
    : implementation_(new Implementation(v, life, root))
{
    implementation_->bind(viewport);
}
Robot3dComposition::~Robot3dComposition()
{
}
presentation::Robot3dPresenter &Robot3dComposition::presenter()
{
    return *implementation_->presenter_;
}
}
}
