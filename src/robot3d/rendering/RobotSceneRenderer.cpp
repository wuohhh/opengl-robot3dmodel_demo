#include "robot3d/rendering/RobotSceneRenderer.h"
#include "3dmodel/robotgeometry.h"
#include "3dmodel/linegeometry.h"
#include "3dmodel/spheregeometry.h"
#include "3dmodel/planegeometry.h"
#include "3dmodel/substancegeometry.h"
#include <cmath>
#include <stdexcept>
namespace robot3d { namespace rendering {
namespace {
QVector3D position(const domain::EulerPoseDeg& p) { return QVector3D(p.X,p.Y,p.Z); }
QColor axisColor(int i) { return QColor(i==0?"#b63232":i==1?"#32b67a":"#3255b6"); }
std::unique_ptr<LineGeometry> axis(int i,double length) {
    QVector3D end; end[i]=length; LineMsg msg={QVector3D(),end,axisColor(i)};
    std::unique_ptr<LineGeometry> result(new LineGeometry(msg));
    if(!result->initShaders()) throw std::runtime_error("Coordinate shader failed"); return result;
}
}
QMatrix4x4 legacyAxisAngleMatrix(const scene::LegacyAxisAngleDeg& p) {
    // Preserve the old display rotation formula, isolated from Euler-domain math.
    QMatrix4x4 matrix;
    QVector3D vector(p.Rx,p.Ry,p.Rz); const float degree=vector.length();
    if(degree>1e-8f) matrix.rotate(degree,vector/degree);
    matrix.setColumn(3,QVector4D(p.X*G_SCALE,p.Y*G_SCALE,p.Z*G_SCALE,1));
    return matrix;
}
RobotSceneRenderer::RobotSceneRenderer() {
    initializeOpenGLFunctions(); glClearColor(1,1,1,1); glFrontFace(GL_CCW); glCullFace(GL_BACK); glEnable(GL_DEPTH_TEST);
    for(int i=0;i<3;++i) { axes_.push_back(axis(i,400)); toolAxes_.push_back(axis(i,120)); }
    SphereMsg tcp={QVector3D(),10,QColor("#3255b6")}; tcp_.reset(new SphereGeometry(tcp));
    waypoints_.reset(new SphereGeometry(QVector<SphereMsg>(),500));
    trajectory_.reset(new LineGeometry(QVector<QVector3D>(),QColor("#3255b6"),1000));
    SubstanceMsg tool={0,{0,0,0,0,0,0},1,1,1,1,QColor("#3255b6")}; tool_.reset(new SubstanceGeometry(tool));
    if(!tcp_->initShaders()||!waypoints_->initShaders()||!trajectory_->initShaders()||!tool_->initShaders()) throw std::runtime_error("Scene shader failed");
}
RobotSceneRenderer::~RobotSceneRenderer() {}
bool RobotSceneRenderer::prepareModel(domain::RobotConfigurationPtr config,QString& error) {
    if(!config) { error="Empty robot configuration"; return false; }
    std::unique_ptr<RobotGeometry> robot(new RobotGeometry()),target(new RobotGeometry());
    const auto dir=QString::fromStdString(config->modelDirectory);
    if(!robot->ChangeRobot3D(dir,config->dh.data()) || !target->ChangeRobot3D(dir,config->dh.data())) { error="Robot mesh loading failed: "+dir; return false; }
    if(!robot->initShaders()||!target->initShaders()) { error="Robot shader initialization failed"; return false; }
    double zero[6]={}; robot->multjointTransform(zero); target->multjointTransform(zero);
    robot->CalRobotViewTargetPos(); target->CalRobotViewTargetPos();
    // Swap only after both candidate models are complete. Old resources die with this current context.
    robot_.swap(robot); target_.swap(target); return true;
}
void RobotSceneRenderer::apply(const scene::RobotSceneSnapshot& scene) {
    scene_=scene; if(!robot_) return;
    auto joints=domain::jointValues(scene.state.joints); robot_->multjointTransform(joints.data());
    auto target=domain::jointValues(scene.targetJoints); target_->multjointTransform(target.data());
    robot_->setEndWorkpieceVisible(scene.options.endWorkpiece); target_->setEndWorkpieceVisible(scene.options.endWorkpiece);
    tcp_->SetSphereData(position(scene.state.tcp)*G_SCALE,10);
    coordinate_=scene.options.install?QMatrix4x4():legacyAxisAngleMatrix(scene.coordinatePose);
    const QVector3D origin=scene.options.install?QVector3D():QVector3D(scene.coordinatePose.X,scene.coordinatePose.Y,scene.coordinatePose.Z);
    for(int i=0;i<3;++i) { QVector4D d; d[i]=400; const QVector3D end=QVector3D(coordinate_*d)+origin; axes_[i]->SetLineData(origin,end); }
    QVector<QVector3D> path; for(const auto& p:scene.state.trajectory) path.append(position(p));
    trajectory_->AddDeleteLineData(path);
    QVector<SphereMsg> points; for(const auto& p:scene.state.waypoints) points.append({position(p)*G_SCALE,6,QColor("#3255b6")});
    waypoints_->SetSphereData(points);
    QVector<SubstanceMsg> tools;
    for(const auto& t:scene.tools) {
        SubstanceMsg msg={t.type,{t.pose.X,t.pose.Y,t.pose.Z,t.pose.Rx,t.pose.Ry,t.pose.Rz},t.radius,t.length,t.width,t.height,QColor(t.rgb[0],t.rgb[1],t.rgb[2])}; tools.append(msg);
    }
    if(tools.isEmpty()) { const auto& p=scene.state.tcp; SubstanceMsg msg={0,{p.X,p.Y,p.Z,p.Rx,p.Ry,p.Rz},1,1,1,1,QColor("#3255b6")}; tools.append(msg); }
    tool_->AddDeleteSubstanceData(tools);
    auto toolMatrix=legacyAxisAngleMatrix(scene::legacyDisplayPose(scene.state.tcp));
    const auto tcp=position(scene.state.tcp);
    for(int i=0;i<3;++i) { QVector4D d; d[i]=120; toolAxes_[i]->SetLineData(tcp,QVector3D(toolMatrix*d)+tcp); }
    planes_.clear();
    for(const auto& p:scene.planes) {
        auto point=[&p](int i){ return QVector3D(p.corners[i][0],p.corners[i][1],p.corners[i][2]); };
        PlaneMsg msg={point(0),point(1),point(2),point(3),QColor(p.rgb[0],p.rgb[1],p.rgb[2]),p.alpha};
        std::unique_ptr<PlaneGeometry> plane(new PlaneGeometry(msg)); plane->initShaders(); planes_.push_back(std::move(plane));
    }
}
QVector3D RobotSceneRenderer::rotationCenter() const { QVector3D result; if(robot_) robot_->set_rotate_center(result); return result; }
QMatrix4x4 RobotSceneRenderer::coordinateMatrix() const { return coordinate_; }
void RobotSceneRenderer::draw(const QMatrix4x4& projection,const CameraState& camera) {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); if(!robot_) return;
    QMatrix4x4 model; model.scale(camera.scale); model=coordinate_.inverted()*model;
    const auto translated=coordinate_*QVector4D(camera.translation,0);
    model.translate(QVector3D(translated)/camera.scale); model.translate(camera.rotationCenter);
    auto rotation=camera.rotation;
    if(scene_.options.install) rotation=rotation*QQuaternion::fromEulerAngles(0,-scene_.installationTiltDeg,-scene_.installationBaseDeg);
    model.rotate(rotation); model.translate(-camera.rotationCenter);
    QMatrix4x4 view; view.lookAt(robot_->Pos,robot_->Target,QVector3D(0,0,1));
    const auto mvp=projection*view*model; QMatrix4x4 light;
    robot_->drawGeometry(model,mvp,light);
    if(scene_.options.targetRobot) target_->drawSecondGeometry(model,mvp,light);
    tcp_->drawGeometry(model,mvp,light);
    if(scene_.options.coordinateAxes) for(auto& a:axes_) a->drawGeometry(model,mvp,light);
    if(scene_.options.waypoints&&!scene_.state.waypoints.empty()) waypoints_->drawGeometry(model,mvp,light);
    if(scene_.options.trajectory&&!scene_.state.trajectory.empty()) trajectory_->drawGeometry(model,mvp,light);
    if(scene_.options.safePlanes) for(auto& p:planes_) p->drawGeometry(model,mvp,light);
    if(scene_.options.tool) { tool_->drawGeometry(model,mvp,light); for(auto& a:toolAxes_) a->drawGeometry(model,mvp,light); }
}
}}
