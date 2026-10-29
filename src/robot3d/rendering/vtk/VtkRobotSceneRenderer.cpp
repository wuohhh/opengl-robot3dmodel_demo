#include "robot3d/rendering/vtk/VtkRobotSceneRenderer.h"
#include "robot3d/rendering/vtk/VtkSceneMath.h"
#include <QDir>
#include <QFileInfo>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkConeSource.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkFloatArray.h>
#include <vtkGlyph3DMapper.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkUnsignedCharArray.h>
#include <algorithm>
#include <limits>
#include <vector>

namespace robot3d
{
namespace rendering
{
namespace
{
using Actor = vtkSmartPointer<vtkActor>;
using Poly = vtkSmartPointer<vtkPolyData>;
QVector3D xyz(const domain::EulerPoseDeg &p)
{
    return QVector3D(p.X, p.Y, p.Z);
}
void matrix(vtkActor *actor, const QMatrix4x4 &value)
{
    vtkNew<vtkMatrix4x4> m;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            m->SetElement(r, c, value(r, c));
    actor->SetUserMatrix(m);
}
Actor actorFor(vtkPolyData *poly, const std::array<int, 3> &color, bool lit = false, double opacity = 1)
{
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(poly);
    if (!lit)
        mapper->ScalarVisibilityOff();
    Actor a = Actor::New();
    a->SetMapper(mapper);
    a->GetProperty()->SetColor(color[0] / 255., color[1] / 255., color[2] / 255.);
    a->GetProperty()->SetOpacity(opacity);
    a->GetProperty()->SetLighting(lit);
    a->GetProperty()->SetAmbient(.55);
    a->GetProperty()->SetDiffuse(.45);
    a->GetProperty()->SetSpecular(.2);
    a->GetProperty()->SetSpecularPower(32);
    return a;
}
Poly mesh(const QString &filename, QString &error)
{
    Assimp::Importer importer;
    const auto name = filename.toStdString();
    const auto *scene =
        importer.ReadFile(name.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);
    if (!scene || !scene->HasMeshes())
    {
        error = filename + ": " + QString::fromUtf8(importer.GetErrorString());
        return Poly();
    }
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> triangles;
    vtkNew<vtkFloatArray> normals;
    normals->SetNumberOfComponents(3);
    vtkNew<vtkUnsignedCharArray> colors;
    colors->SetNumberOfComponents(3);
    colors->SetName("DiffuseRGB");
    for (unsigned j = 0; j < scene->mNumMeshes; ++j)
    {
        const auto *part = scene->mMeshes[j];
        if (!part->HasPositions() || !part->HasNormals())
        {
            error = "Mesh positions/normals missing: " + filename;
            return Poly();
        }
        aiColor3D color(.7f, .7f, .7f);
        if (part->mMaterialIndex < scene->mNumMaterials)
            scene->mMaterials[part->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        const vtkIdType offset = points->GetNumberOfPoints();
        for (unsigned i = 0; i < part->mNumVertices; ++i)
        {
            const auto &p = part->mVertices[i];
            const auto &n = part->mNormals[i];
            points->InsertNextPoint(p.x * vtkmath::scale, p.y * vtkmath::scale, p.z * vtkmath::scale);
            normals->InsertNextTuple3(n.x, n.y, n.z);
            colors->InsertNextTuple3(255 * color.r, 255 * color.g, 255 * color.b);
        }
        for (unsigned i = 0; i < part->mNumFaces; ++i)
        {
            const auto &face = part->mFaces[i];
            if (face.mNumIndices != 3)
            {
                error = "Non-triangle face: " + filename;
                return Poly();
            }
            vtkIdType ids[3];
            for (int k = 0; k < 3; ++k)
            {
                if (face.mIndices[k] >= part->mNumVertices)
                {
                    error = "Invalid mesh index: " + filename;
                    return Poly();
                }
                ids[k] = offset + face.mIndices[k];
            }
            triangles->InsertNextCell(3, ids);
        }
    }
    if (triangles->GetNumberOfCells() == 0)
    {
        error = "Empty robot mesh: " + filename;
        return Poly();
    }
    Poly result = Poly::New();
    result->SetPoints(points);
    result->SetPolys(triangles);
    result->GetPointData()->SetNormals(normals);
    result->GetPointData()->SetScalars(colors);
    return result;
}
Poly line(const std::vector<QVector3D> &values, bool polygon = false)
{
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> cells;
    for (const auto &p : values)
        points->InsertNextPoint(p.x(), p.y(), p.z());
    if (values.size() > 1)
    {
        cells->InsertNextCell(vtkIdType(values.size()));
        for (vtkIdType i = 0; i < vtkIdType(values.size()); ++i)
            cells->InsertCellPoint(i);
    }
    Poly result = Poly::New();
    result->SetPoints(points);
    if (polygon)
        result->SetPolys(cells);
    else
        result->SetLines(cells);
    return result;
}
Poly sphere(double radius, int theta = 12, int phi = 7)
{
    vtkNew<vtkSphereSource> s;
    s->SetRadius(radius);
    s->SetThetaResolution(theta);
    s->SetPhiResolution(phi);
    s->Update();
    return s->GetOutput();
}
const std::array<int, 3> blue = {{50, 85, 182}};
std::array<int, 3> axisColor(int i)
{
    return i == 0 ? std::array<int, 3>{{182, 50, 50}} : i == 1 ? std::array<int, 3>{{50, 182, 122}} : blue;
}
/** @brief 保存附加场景图元与局部变换，用于统一应用场景根矩阵。 */
struct Decoration
{
    Actor actor;      ///< 坐标轴、轨迹、工具或安全平面等附加图元的 VTK Actor。
    QMatrix4x4 local; ///< 图元自身的局部变换；绘制时与场景根矩阵相乘。
};
}

/** @brief 保存 VTK 机器人 Actor、场景快照及从快照计算的变换缓存。 */
struct VtkRobotSceneRenderer::Implementation
{
    vtkRenderer *renderer;                       ///< 外部视口持有的渲染器；本实现仅向其添加或移除 Actor。
    domain::RobotConfigurationPtr configuration; ///< 最近成功加载模型所使用的机器人配置；空表示尚无模型。
    scene::RobotSceneSnapshot scene;             ///< 最近一次 apply 接收的场景快照，供相机更新时读取显示选项。
    std::vector<Actor> robot;                    ///< 当前姿态的基座、六个关节及可选末端工件 Actor，按模型加载顺序保存。
    std::vector<Actor> target;                   ///< 与 robot 逐项对应的目标姿态 Actor，由目标机器人显示选项控制显隐。
    std::vector<Decoration> decorations;         ///< 当前场景的附加图元，每次 apply 时移除后重建。
    std::array<QMatrix4x4, 7> links;             ///< 当前姿态的基座及六关节累积变换，索引 0 为基座，平移采用模型单位。
    std::array<QMatrix4x4, 7> targets;           ///< 目标关节姿态的累积变换，索引规则与 links 相同。
    QMatrix4x4 coordinate;                       ///< 当前显示坐标系变换；安装模式使用单位矩阵。
    QVector3D eye;                               ///< VTK 相机位置，采用模型单位，模型加载后根据包围盒中心设置。
    QVector3D focus;                             ///< VTK 相机焦点，采用模型单位，对应已加载模型的包围盒中心。
    int triangles = 0;                           ///< 最近成功加载的全部机器人网格三角形总数。
    explicit Implementation(vtkRenderer *r) : renderer(r)
    {
        renderer->SetBackground(1, 1, 1);
    }
    ~Implementation()
    {
        for (auto &a : robot)
            renderer->RemoveActor(a);
        for (auto &a : target)
            renderer->RemoveActor(a);
        for (auto &d : decorations)
            renderer->RemoveActor(d.actor);
    }
    void add(Actor a, const QMatrix4x4 &local = QMatrix4x4())
    {
        renderer->AddActor(a);
        decorations.push_back({a, local});
    }
};
VtkRobotSceneRenderer::VtkRobotSceneRenderer(vtkRenderer *r) : impl_(new Implementation(r))
{
}
VtkRobotSceneRenderer::~VtkRobotSceneRenderer()
{
}
bool VtkRobotSceneRenderer::prepareModel(domain::RobotConfigurationPtr c, QString &error)
{
    if (!c)
    {
        error = "Empty robot configuration";
        return false;
    }
    std::vector<Actor> candidates, targets;
    QVector3D low(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()),
        high = -low;
    int count = 0;
    QDir directory(QString::fromStdString(c->modelDirectory));
    const bool hasTool = QFileInfo(directory.filePath("tool.obj")).isFile();
    for (int i = 0; i < 7 + int(hasTool); ++i)
    {
        const auto filename = directory.filePath(i == 0   ? "base.obj"
                                                 : i == 7 ? "tool.obj"
                                                          : QString("joint%1.obj").arg(i));
        const auto poly = mesh(filename, error);
        if (!poly)
            return false;
        count += int(poly->GetNumberOfPolys());
        double bounds[6];
        poly->GetBounds(bounds);
        for (int k = 0; k < 3; ++k)
        {
            low[k] = qMin(low[k], float(bounds[2 * k]));
            high[k] = qMax(high[k], float(bounds[2 * k + 1]));
        }
        auto actor = actorFor(poly, blue, true);
        candidates.push_back(actor);
        auto target = actorFor(poly, {{25, 186, 255}});
        target->SetVisibility(false);
        targets.push_back(target);
    }
    // All CPU resources are complete before touching the currently displayed scene.
    for (auto &a : impl_->robot)
        impl_->renderer->RemoveActor(a);
    for (auto &a : impl_->target)
        impl_->renderer->RemoveActor(a);
    impl_->robot.swap(candidates);
    impl_->target.swap(targets);
    impl_->configuration = c;
    impl_->triangles = count;
    impl_->focus = (low + high) * .5;
    impl_->eye = impl_->focus + QVector3D(150, 0, 0);
    for (auto &a : impl_->robot)
        impl_->renderer->AddActor(a);
    for (auto &a : impl_->target)
        impl_->renderer->AddActor(a);
    return true;
}
void VtkRobotSceneRenderer::apply(const scene::RobotSceneSnapshot &s)
{
    auto &p = *impl_;
    p.scene = s;
    if (!p.configuration)
        return;
    p.links = vtkmath::linkMatrices(*p.configuration, s.state.joints);
    p.targets = vtkmath::linkMatrices(*p.configuration, s.targetJoints);
    p.coordinate = s.options.install ? QMatrix4x4() : vtkmath::poseMatrix(s.coordinatePose);
    for (auto &d : p.decorations)
        p.renderer->RemoveActor(d.actor);
    p.decorations.clear();
    // Preserve the legacy marker position conversion for backend comparisons.
    QMatrix4x4 tcp;
    tcp.translate(xyz(s.state.tcp) * vtkmath::scale * vtkmath::scale);
    p.add(actorFor(sphere(10 * vtkmath::scale), blue), tcp);
    const QVector3D origin =
        s.options.install ? QVector3D() : QVector3D(s.coordinatePose.X, s.coordinatePose.Y, s.coordinatePose.Z);
    if (s.options.coordinateAxes)
        for (int i = 0; i < 3; ++i)
        {
            QVector4D direction;
            direction[i] = 400;
            auto a = actorFor(
                line({origin * vtkmath::scale, (QVector3D(p.coordinate * direction) + origin) * vtkmath::scale}),
                axisColor(i));
            a->GetProperty()->SetLineWidth(3);
            p.add(a);
        }
    if (s.options.trajectory && s.state.trajectory.size() > 1)
    {
        std::vector<QVector3D> points;
        for (const auto &v : s.state.trajectory)
            points.push_back(xyz(v) * vtkmath::scale);
        auto a = actorFor(line(points), blue);
        a->GetProperty()->SetLineWidth(3);
        p.add(a);
    }
    if (s.options.waypoints && !s.state.waypoints.empty())
    {
        vtkNew<vtkPoints> points;
        for (std::size_t i = 0; i < s.state.waypoints.size() && i < 500; ++i)
        {
            const auto v = xyz(s.state.waypoints[i]) * vtkmath::scale * vtkmath::scale;
            points->InsertNextPoint(v.x(), v.y(), v.z());
        }
        vtkNew<vtkPolyData> data;
        data->SetPoints(points);
        vtkNew<vtkGlyph3DMapper> mapper;
        mapper->SetInputData(data);
        mapper->SetSourceData(sphere(6 * vtkmath::scale));
        mapper->ScalingOff();
        mapper->ScalarVisibilityOff();
        Actor a = Actor::New();
        a->SetMapper(mapper);
        a->GetProperty()->LightingOff();
        a->GetProperty()->SetColor(50 / 255., 85 / 255., 182 / 255.);
        p.add(a);
    }
    if (s.options.safePlanes)
        for (const auto &plane : s.planes)
        {
            std::vector<QVector3D> points;
            for (const auto &v : plane.corners)
                points.push_back(QVector3D(v[0], v[1], v[2]) * vtkmath::scale);
            p.add(actorFor(line(points, true), plane.rgb, false, qBound(0., double(plane.alpha), 1.)));
        }
    if (s.options.tool)
    {
        auto tools = s.tools;
        if (tools.empty())
        {
            scene::ToolShape tool;
            tool.pose = scene::legacyDisplayPose(s.state.tcp);
            tools.push_back(tool);
        }
        for (const auto &tool : tools)
        {
            Poly poly;
            QMatrix4x4 local = vtkmath::poseMatrix(tool.pose);
            if (tool.type == 0)
                poly = sphere(qMax(.001, double(tool.radius)) * vtkmath::scale, 36, 19);
            else if (tool.type == 1)
            {
                vtkNew<vtkCubeSource> source;
                source->SetXLength(qMax(.001, double(tool.length)) * vtkmath::scale);
                source->SetYLength(qMax(.001, double(tool.width)) * vtkmath::scale);
                source->SetZLength(qMax(.001, double(tool.height)) * vtkmath::scale);
                source->SetCenter(0, 0, tool.height * vtkmath::scale / 2);
                source->Update();
                poly = source->GetOutput();
            }
            else if (tool.type == 2)
            {
                vtkNew<vtkCylinderSource> source;
                source->SetRadius(qMax(.001, double(tool.radius)) * vtkmath::scale);
                source->SetHeight(qMax(.001, double(tool.height)) * vtkmath::scale);
                source->SetCenter(0, tool.height * vtkmath::scale / 2, 0);
                source->SetResolution(36);
                source->Update();
                poly = source->GetOutput();
                local.rotate(90, 1, 0, 0);
            }
            else if (tool.type == 3)
            {
                vtkNew<vtkConeSource> source;
                source->SetRadius(qMax(.001, double(tool.radius)) * vtkmath::scale);
                source->SetHeight(qMax(.001, double(tool.height)) * vtkmath::scale);
                source->SetCenter(0, 0, tool.height * vtkmath::scale / 2);
                source->SetDirection(0, 0, 1);
                source->SetResolution(36);
                source->Update();
                poly = source->GetOutput();
            }
            if (poly)
                p.add(actorFor(poly, tool.rgb, false, .5), local);
        }
        const auto toolMatrix = vtkmath::poseMatrix(scene::legacyDisplayPose(s.state.tcp));
        const auto origin = xyz(s.state.tcp);
        for (int i = 0; i < 3; ++i)
        {
            QVector4D direction;
            direction[i] = 120;
            auto a =
                actorFor(line({origin * vtkmath::scale, (QVector3D(toolMatrix * direction) + origin) * vtkmath::scale}),
                         axisColor(i));
            a->GetProperty()->SetLineWidth(3);
            p.add(a);
        }
    }
}
void VtkRobotSceneRenderer::updateCamera(const CameraState &camera)
{
    auto &p = *impl_;
    if (!p.configuration)
        return;
    const auto root = vtkmath::sceneMatrix(p.scene, camera);
    for (std::size_t i = 0; i < p.robot.size(); ++i)
    {
        const int link = int(qMin(i, std::size_t(6)));
        matrix(p.robot[i], root * p.links[link]);
        matrix(p.target[i], root * p.targets[link]);
        p.robot[i]->SetVisibility(i < 7 || p.scene.options.endWorkpiece);
        p.target[i]->SetVisibility(p.scene.options.targetRobot && (i < 7 || p.scene.options.endWorkpiece));
    }
    for (auto &d : p.decorations)
        matrix(d.actor, root * d.local);
    auto *c = p.renderer->GetActiveCamera();
    c->SetPosition(p.eye.x(), p.eye.y(), p.eye.z());
    c->SetFocalPoint(p.focus.x(), p.focus.y(), p.focus.z());
    c->SetViewUp(0, 0, 1);
    c->SetViewAngle(45);
    c->SetClippingRange(30, 400);
}
QMatrix4x4 VtkRobotSceneRenderer::coordinateMatrix() const
{
    return impl_->coordinate;
}
QVector3D VtkRobotSceneRenderer::rotationCenter() const
{
    return QVector3D();
}
QMatrix4x4 VtkRobotSceneRenderer::linkMatrix(int i) const
{
    return impl_->links.at(std::size_t(i));
}
int VtkRobotSceneRenderer::triangleCount() const
{
    return impl_->triangles;
}
}
}
