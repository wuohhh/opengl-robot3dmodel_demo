#include "robot3d/rendering/opengl/OpenGlRobotSceneRenderer.h"
#include "robot3d/rendering/opengl/geometry/OpenGlRobotGeometry.h"
#include "robot3d/rendering/opengl/geometry/OpenGlLineGeometry.h"
#include "robot3d/rendering/opengl/geometry/OpenGlSphereGeometry.h"
#include "robot3d/rendering/opengl/geometry/OpenGlPlaneGeometry.h"
#include "robot3d/rendering/opengl/geometry/OpenGlSolidGeometry.h"
#include <cmath>
#include <stdexcept>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLShaderProgram>
#include <map>
#include <set>
#include <cstddef>
#include <limits>
#include "robot3d/rendering/WorkpieceSceneMath.h"
namespace robot3d
{
namespace rendering
{
namespace
{
QVector3D position(const domain::EulerPoseDeg &p)
{
    return QVector3D(p.X, p.Y, p.Z);
}
QColor axisColor(int i)
{
    return QColor(i == 0 ? "#b63232" : i == 1 ? "#32b67a" : "#3255b6");
}
std::unique_ptr<OpenGlLineGeometry> axis(int i, double length)
{
    QVector3D end;
    end[i] = length;
    OpenGlLineData msg = {QVector3D(), end, axisColor(i)};
    std::unique_ptr<OpenGlLineGeometry> result(new OpenGlLineGeometry(msg));
    if (!result->initShaders())
        throw std::runtime_error("Coordinate shader failed");
    return result;
}
}
QMatrix4x4 legacyAxisAngleMatrix(const scene::LegacyAxisAngleDeg &p)
{
    // Preserve the old display rotation formula, isolated from Euler-domain math.
    QMatrix4x4 matrix;
    QVector3D vector(p.Rx, p.Ry, p.Rz);
    const float degree = vector.length();
    if (degree > 1e-8f)
        matrix.rotate(degree, vector / degree);
    matrix.setColumn(3, QVector4D(p.X * robot3d::rendering::kMillimetersToModelUnits,
                                  p.Y * robot3d::rendering::kMillimetersToModelUnits,
                                  p.Z * robot3d::rendering::kMillimetersToModelUnits, 1));
    return matrix;
}

struct OpenGlRobotSceneRenderer::WorkpieceResources : protected QOpenGLFunctions
{
    struct Vertex { GLfloat p[3],n[3],c[3]; };
    struct Mesh
    {
        QOpenGLBuffer vertices,indices;
        int count=0;
        domain::Matrix4d component;
        Mesh():vertices(QOpenGLBuffer::VertexBuffer),indices(QOpenGLBuffer::IndexBuffer){}
    };
    struct Asset
    {
        domain::WorkpieceAssetPtr source;
        std::vector<std::unique_ptr<Mesh>> meshes;
    };
    QOpenGLShaderProgram program;
    QOpenGLBuffer axes;
    std::map<const domain::WorkpieceAsset*,std::shared_ptr<Asset>> cache;
    WorkpieceResources():axes(QOpenGLBuffer::VertexBuffer)
    {
        initializeOpenGLFunctions();
        const char *vertex =
            "#version 120\nattribute vec3 position;attribute vec3 normal;attribute vec3 color;"
            "uniform mat4 mvp;uniform mat3 normals;varying vec3 vColor;varying vec3 vNormal;"
            "void main(){gl_Position=mvp*vec4(position,1.0);vColor=color;vNormal=normals*normal;}";
        const char *fragment =
            "#version 120\nuniform vec3 overrideColor;uniform int useOverride;uniform int selected;"
            "uniform float opacity;uniform int unlit;varying vec3 vColor;varying vec3 vNormal;"
            "void main(){vec3 c=useOverride==1?overrideColor:vColor;"
            "float l=unlit==1?1.0:0.5+0.5*abs(dot(normalize(vNormal),normalize(vec3(0.4,0.5,1.0))));"
            "if(selected==1)c=mix(c,vec3(1.0,0.58,0.08),0.35);gl_FragColor=vec4(c*l,opacity);}";
        if(!program.addShaderFromSourceCode(QOpenGLShader::Vertex,vertex) ||
           !program.addShaderFromSourceCode(QOpenGLShader::Fragment,fragment) || !program.link())
            throw std::runtime_error(program.log().toStdString());
        const Vertex a[6]={{{0,0,0},{0,0,1},{0.85f,0.15f,0.15f}},{{1,0,0},{0,0,1},{0.85f,0.15f,0.15f}},
                           {{0,0,0},{0,0,1},{0.15f,0.7f,0.25f}},{{0,1,0},{0,0,1},{0.15f,0.7f,0.25f}},
                           {{0,0,0},{0,0,1},{0.15f,0.3f,0.9f}},{{0,0,1},{0,0,1},{0.15f,0.3f,0.9f}}};
        if(!axes.create()||!axes.bind())throw std::runtime_error("Workpiece axis buffer failed");
        axes.allocate(a,sizeof(a));axes.release();
    }
    void attributes()
    {
        program.enableAttributeArray("position");program.enableAttributeArray("normal");program.enableAttributeArray("color");
        program.setAttributeBuffer("position",GL_FLOAT,int(offsetof(Vertex,p)),3,sizeof(Vertex));
        program.setAttributeBuffer("normal",GL_FLOAT,int(offsetof(Vertex,n)),3,sizeof(Vertex));
        program.setAttributeBuffer("color",GL_FLOAT,int(offsetof(Vertex,c)),3,sizeof(Vertex));
    }
    bool prepare(const scene::RobotSceneSnapshot &s,QString &error)
    {
        std::map<const domain::WorkpieceAsset*,std::shared_ptr<Asset>> candidates;
        try
        {
            for(const auto &instance:s.workpieces)
            {
                if(!instance.asset || cache.count(instance.asset.get()) || candidates.count(instance.asset.get()))continue;
                const auto valid=domain::validateWorkpieceAsset(*instance.asset);
                if(!valid.ok){error=QString::fromStdString(valid.error);return false;}
                std::shared_ptr<Asset> asset(new Asset);asset->source=instance.asset;
                for(const auto &component:instance.asset->components)
                {
                    if(!component.selected)continue;
                    for(const auto &mesh:component.meshes)
                    {
                        if(mesh.indices.empty())continue;
                        if(mesh.vertices.size()>std::size_t((std::numeric_limits<int>::max)())/sizeof(Vertex) ||
                           mesh.indices.size()>std::size_t((std::numeric_limits<int>::max)())/sizeof(std::uint32_t))
                        {error="Workpiece GPU buffer exceeds allocation limit";return false;}
                        std::vector<Vertex> vertices;vertices.reserve(mesh.vertices.size());
                        for(const auto &v:mesh.vertices)
                        {
                            Vertex out={{float(v.position.x*kMillimetersToModelUnits),float(v.position.y*kMillimetersToModelUnits),float(v.position.z*kMillimetersToModelUnits)},
                                        {float(v.normal.x),float(v.normal.y),float(v.normal.z)},{v.color[0],v.color[1],v.color[2]}};
                            vertices.push_back(out);
                        }
                        std::unique_ptr<Mesh> part(new Mesh);part->component=component.transform;
                        while(glGetError()!=GL_NO_ERROR){}
                        if(!part->vertices.create()||!part->indices.create()||!part->vertices.bind())
                        {error="Cannot create workpiece GPU buffer";return false;}
                        part->vertices.setUsagePattern(QOpenGLBuffer::StaticDraw);
                        part->vertices.allocate(vertices.data(),int(vertices.size()*sizeof(Vertex)));part->vertices.release();
                        if(!part->indices.bind()){error="Cannot bind workpiece index buffer";return false;}
                        part->indices.setUsagePattern(QOpenGLBuffer::StaticDraw);
                        part->indices.allocate(mesh.indices.data(),int(mesh.indices.size()*sizeof(std::uint32_t)));part->indices.release();
                        if(glGetError()!=GL_NO_ERROR){error="Insufficient GPU resources for workpiece";return false;}
                        part->count=int(mesh.indices.size());asset->meshes.push_back(std::move(part));
                    }
                }
                candidates[instance.asset.get()]=asset;
            }
            auto preparedCache=cache;
            preparedCache.insert(candidates.begin(),candidates.end());
            cache.swap(preparedCache);
            return true;
        }
        catch(const std::exception &e){error=QString::fromUtf8(e.what());return false;}
    }
    void prune(const scene::RobotSceneSnapshot &s)
    {
        std::set<const domain::WorkpieceAsset*> used;
        for(const auto &i:s.workpieces)if(i.asset)used.insert(i.asset.get());
        for(auto i=cache.begin();i!=cache.end();)if(!used.count(i->first))i=cache.erase(i);else ++i;
    }
    void draw(const scene::RobotSceneSnapshot &s,const QMatrix4x4 &projection,const QMatrix4x4 &view,
              const QMatrix4x4 &root)
    {
        auto *desktop=QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_2_1>();
        if(!desktop || !desktop->initializeOpenGLFunctions())return;
        std::vector<const domain::WorkpieceInstance*> ordered;
        for(const auto &i:s.workpieces)if(i.visible&&i.asset&&cache.count(i.asset.get()))ordered.push_back(&i);
        std::stable_sort(ordered.begin(),ordered.end(),[&](const domain::WorkpieceInstance *a,const domain::WorkpieceInstance *b){
            if((a->opacity<0.999)!=(b->opacity<0.999))return a->opacity>=0.999;
            const auto ca=workpieceqt::matrix(workpieceqt::baseMatrix(s,*a),true).column(3);
            const auto cb=workpieceqt::matrix(workpieceqt::baseMatrix(s,*b),true).column(3);
            return (view*root*ca).z()<(view*root*cb).z();
        });
        program.bind();glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        for(const auto *instance:ordered)
        {
            const auto &i=*instance;const auto frame=workpieceqt::baseMatrix(s,i);
            const bool selected=i.id==s.selectedWorkpieceId;
            program.setUniformValue("useOverride",i.colorOverride?1:0);
            program.setUniformValue("overrideColor",QVector3D(i.color[0],i.color[1],i.color[2]));
            program.setUniformValue("selected",selected?1:0);
            program.setUniformValue("opacity",float(qBound(0.0,i.opacity,1.0)));
            program.setUniformValue("unlit",0);
            glDepthMask(i.opacity<0.999?GL_FALSE:GL_TRUE);
            for(const auto &mesh:cache[i.asset.get()]->meshes)
            {
                const auto model=root*workpieceqt::matrix(domain::multiply(frame,mesh->component),true);
                program.setUniformValue("mvp",projection*view*model);
                program.setUniformValue("normals",model.normalMatrix());
                mesh->vertices.bind();mesh->indices.bind();attributes();
                if(i.displayMode!=domain::WorkpieceDisplayMode::Wireframe)
                {
                    desktop->glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
                    glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1,1);
                    glDrawElements(GL_TRIANGLES,mesh->count,GL_UNSIGNED_INT,nullptr);
                    glDisable(GL_POLYGON_OFFSET_FILL);
                }
                if(i.displayMode!=domain::WorkpieceDisplayMode::Solid || selected)
                {
                    program.setUniformValue("useOverride",1);program.setUniformValue("unlit",1);
                    program.setUniformValue("overrideColor",selected?QVector3D(1,0.5f,0):QVector3D(0.18f,0.2f,0.23f));
                    desktop->glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
                    glLineWidth(selected?2.0f:1.0f);glDrawElements(GL_TRIANGLES,mesh->count,GL_UNSIGNED_INT,nullptr);
                    desktop->glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
                    program.setUniformValue("useOverride",i.colorOverride?1:0);
                    program.setUniformValue("overrideColor",QVector3D(i.color[0],i.color[1],i.color[2]));
                    program.setUniformValue("unlit",0);
                }
                mesh->indices.release();mesh->vertices.release();
            }
            if(i.showAxes)
            {
                QMatrix4x4 m=root*workpieceqt::matrix(frame,true);m.scale(float(100*kMillimetersToModelUnits));
                program.setUniformValue("mvp",projection*view*m);program.setUniformValue("normals",m.normalMatrix());
                program.setUniformValue("useOverride",0);program.setUniformValue("selected",0);program.setUniformValue("unlit",1);program.setUniformValue("opacity",1.0f);
                axes.bind();attributes();glLineWidth(3);glDrawArrays(GL_LINES,0,6);axes.release();
            }
        }
        glDepthMask(GL_TRUE);glDisable(GL_BLEND);glLineWidth(1);
        program.disableAttributeArray("position");program.disableAttributeArray("normal");program.disableAttributeArray("color");program.release();
    }
};
bool OpenGlRobotSceneRenderer::prepareWorkpieces(const scene::RobotSceneSnapshot &s,QString &error)
{
    try
    {
        if(s.workpieces.empty())return true;
        if(!workpieceResources_)workpieceResources_.reset(new WorkpieceResources);
        return workpieceResources_->prepare(s,error);
    }
    catch(const std::exception &e){error=QString::fromUtf8(e.what());return false;}
}
QString OpenGlRobotSceneRenderer::pickWorkpiece(const CameraState &camera,const QSize &size,const QPointF &point) const
{
    if(!robot_)return QString();
    return QString::fromStdString(workpieceqt::pick(scene_,camera,workpieceqt::sceneProjection(scene_,camera,double(size.width())/std::max(1,size.height()),robot_->Pos,robot_->Target),
       workpieceqt::view(scene_,camera,robot_->Pos,robot_->Target),size,point));
}

OpenGlRobotSceneRenderer::OpenGlRobotSceneRenderer()
{
    initializeOpenGLFunctions();
    glClearColor(1, 1, 1, 1);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    for (int i = 0; i < 3; ++i)
    {
        axes_.push_back(axis(i, 400));
        toolAxes_.push_back(axis(i, 120));
    }
    OpenGlSphereData tcp = {QVector3D(), 10, QColor("#3255b6")};
    tcp_.reset(new OpenGlSphereGeometry(tcp));
    waypoints_.reset(new OpenGlSphereGeometry(QVector<OpenGlSphereData>(), 500));
    trajectory_.reset(new OpenGlLineGeometry(QVector<QVector3D>(), QColor("#3255b6"), 1000));
    OpenGlSolidData tool = {0, {0, 0, 0, 0, 0, 0}, 1, 1, 1, 1, QColor("#3255b6")};
    tool_.reset(new OpenGlSolidGeometry(tool));
    if (!tcp_->initShaders() || !waypoints_->initShaders() || !trajectory_->initShaders() || !tool_->initShaders())
        throw std::runtime_error("Scene shader failed");
}
OpenGlRobotSceneRenderer::~OpenGlRobotSceneRenderer()
{
}
bool OpenGlRobotSceneRenderer::prepareModel(domain::RobotConfigurationPtr config, QString &error)
{
    if (!config)
    {
        error = "Empty robot configuration";
        return false;
    }
    std::unique_ptr<OpenGlRobotGeometry> robot(new OpenGlRobotGeometry()), target(new OpenGlRobotGeometry());
    const auto dir = QString::fromStdString(config->modelDirectory);
    if (!robot->ChangeRobot3D(dir, config->dh.data()) || !target->ChangeRobot3D(dir, config->dh.data()))
    {
        error = "Robot mesh loading failed: " + dir;
        return false;
    }
    if (!robot->initShaders() || !target->initShaders())
    {
        error = "Robot shader initialization failed";
        return false;
    }
    double zero[6] = {};
    robot->multjointTransform(zero);
    target->multjointTransform(zero);
    robot->CalRobotViewTargetPos();
    target->CalRobotViewTargetPos();
    // Swap only after both candidate models are complete. Old resources die with this current context.
    robot_.swap(robot);
    target_.swap(target);
    return true;
}
void OpenGlRobotSceneRenderer::apply(const scene::RobotSceneSnapshot &scene)
{
    QString workpieceError;
    if (!prepareWorkpieces(scene,workpieceError)) throw std::runtime_error(workpieceError.toStdString());
    scene_ = scene;
    if(workpieceResources_)workpieceResources_->prune(scene);
    if (!robot_)
        return;
    auto joints = domain::jointValues(scene.state.joints);
    robot_->multjointTransform(joints.data());
    auto target = domain::jointValues(scene.targetJoints);
    target_->multjointTransform(target.data());
    robot_->setEndWorkpieceVisible(scene.options.endWorkpiece);
    target_->setEndWorkpieceVisible(scene.options.endWorkpiece);
    tcp_->SetSphereData(position(scene.state.tcp) * robot3d::rendering::kMillimetersToModelUnits, 10);
    coordinate_ = scene.options.install ? QMatrix4x4() : legacyAxisAngleMatrix(scene.coordinatePose);
    const QVector3D origin = scene.options.install
                                 ? QVector3D()
                                 : QVector3D(scene.coordinatePose.X, scene.coordinatePose.Y, scene.coordinatePose.Z);
    for (int i = 0; i < 3; ++i)
    {
        QVector4D d;
        d[i] = 400;
        const QVector3D end = QVector3D(coordinate_ * d) + origin;
        axes_[i]->SetLineData(origin, end);
    }
    QVector<QVector3D> path;
    for (const auto &p : scene.state.trajectory)
        path.append(position(p));
    trajectory_->AddDeleteLineData(path);
    QVector<OpenGlSphereData> points;
    for (const auto &p : scene.state.waypoints)
        points.append({position(p) * robot3d::rendering::kMillimetersToModelUnits, 6, QColor("#3255b6")});
    waypoints_->SetSphereData(points);
    QVector<OpenGlSolidData> tools;
    for (const auto &t : scene.tools)
    {
        OpenGlSolidData msg = {t.type,
                               {t.pose.X, t.pose.Y, t.pose.Z, t.pose.Rx, t.pose.Ry, t.pose.Rz},
                               t.radius,
                               t.length,
                               t.width,
                               t.height,
                               QColor(t.rgb[0], t.rgb[1], t.rgb[2])};
        tools.append(msg);
    }
    if (tools.isEmpty())
    {
        const auto &p = scene.state.tcp;
        OpenGlSolidData msg = {0, {p.X, p.Y, p.Z, p.Rx, p.Ry, p.Rz}, 1, 1, 1, 1, QColor("#3255b6")};
        tools.append(msg);
    }
    tool_->AddDeleteSubstanceData(tools);
    auto toolMatrix = legacyAxisAngleMatrix(scene::legacyDisplayPose(scene.state.tcp));
    const auto tcp = position(scene.state.tcp);
    for (int i = 0; i < 3; ++i)
    {
        QVector4D d;
        d[i] = 120;
        toolAxes_[i]->SetLineData(tcp, QVector3D(toolMatrix * d) + tcp);
    }
    planes_.clear();
    for (const auto &p : scene.planes)
    {
        auto point = [&p](int i)
        {
            return QVector3D(p.corners[i][0], p.corners[i][1], p.corners[i][2]);
        };
        OpenGlPlaneData msg = {point(0), point(1), point(2), point(3), QColor(p.rgb[0], p.rgb[1], p.rgb[2]), p.alpha};
        std::unique_ptr<OpenGlPlaneGeometry> plane(new OpenGlPlaneGeometry(msg));
        plane->initShaders();
        planes_.push_back(std::move(plane));
    }
}
QVector3D OpenGlRobotSceneRenderer::rotationCenter() const
{
    QVector3D result;
    if (robot_)
        robot_->set_rotate_center(result);
    return result;
}
QMatrix4x4 OpenGlRobotSceneRenderer::coordinateMatrix() const
{
    return coordinate_;
}
void OpenGlRobotSceneRenderer::draw(const QMatrix4x4 &projection, const CameraState &camera)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!robot_)
        return;
    const auto model=workpieceqt::root(scene_,camera);
    const auto view=workpieceqt::view(scene_,camera,robot_->Pos,robot_->Target);
    const auto drawingProjection=workpieceqt::sceneProjection(scene_,camera,double(projection(1,1))/projection(0,0),robot_->Pos,robot_->Target);
    const auto mvp = drawingProjection * view * model;
    QMatrix4x4 light;
    robot_->drawGeometry(model, mvp, light);
    if (scene_.options.targetRobot)
        target_->drawSecondGeometry(model, mvp, light);
    tcp_->drawGeometry(model, mvp, light);
    if (scene_.options.coordinateAxes)
        for (auto &a : axes_)
            a->drawGeometry(model, mvp, light);
    if (scene_.options.waypoints && !scene_.state.waypoints.empty())
        waypoints_->drawGeometry(model, mvp, light);
    if (scene_.options.trajectory && !scene_.state.trajectory.empty())
        trajectory_->drawGeometry(model, mvp, light);
    if (scene_.options.safePlanes)
        for (auto &p : planes_)
            p->drawGeometry(model, mvp, light);
    if (scene_.options.tool)
    {
        tool_->drawGeometry(model, mvp, light);
        for (auto &a : toolAxes_)
            a->drawGeometry(model, mvp, light);
    }
    if(workpieceResources_)workpieceResources_->draw(scene_,drawingProjection,view,model);
}
}
}
