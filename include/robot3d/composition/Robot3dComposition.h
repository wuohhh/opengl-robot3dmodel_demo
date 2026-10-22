#pragma once
#include <memory>
#include <string>
class QObject;
class OpenGlRobotViewport;
class RobotViewport;
namespace robot3d { namespace presentation { class IRobot3dView; class Robot3dPresenter; }
namespace composition {
class Robot3dComposition {
public:
    Robot3dComposition(presentation::IRobot3dView& view,QObject& lifetime,OpenGlRobotViewport& viewport,const std::string& root);
    Robot3dComposition(presentation::IRobot3dView& view,QObject& lifetime,RobotViewport& viewport,const std::string& root);
    ~Robot3dComposition();
    presentation::Robot3dPresenter& presenter();
private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};
}}
