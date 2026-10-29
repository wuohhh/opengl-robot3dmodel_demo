#include "robot3d/shell/vtk/VtkPointCloudViewport.h"
#include <QVTKOpenGLNativeWidget.h>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVBoxLayout>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>
#include <functional>
namespace
{
class VtkPointCloudRenderSurface : public QVTKOpenGLNativeWidget
{
public:
    std::function<void()> ready;
    explicit VtkPointCloudRenderSurface(QWidget *parent) : QVTKOpenGLNativeWidget(parent)
    {
        auto format = QVTKOpenGLNativeWidget::defaultFormat();
        format.setProfile(QSurfaceFormat::CompatibilityProfile);
        setFormat(format);
    }

protected:
    void initializeGL() override
    {
        QVTKOpenGLNativeWidget::initializeGL();
        QTimer::singleShot(0, this,
                           [this]
                           {
                               if (ready)
                                   ready();
                           });
    }
};
}
/** @brief 持有点云视口的 Qt 渲染表面、VTK 管线及当前点云数据。 */
struct VtkPointCloudViewport::Implementation
{
    VtkPointCloudRenderSurface *surface; ///< 嵌入视口的 OpenGL 渲染表面，由 Qt 父控件管理生命周期。
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> window =
        vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New(); ///< 绑定 Qt 渲染表面的 VTK 窗口。
    vtkSmartPointer<vtkRenderer> renderer =
        vtkSmartPointer<vtkRenderer>::New(); ///< 管理背景、相机和点云 Actor 的渲染器。
    vtkSmartPointer<vtkPolyDataMapper> mapper =
        vtkSmartPointer<vtkPolyDataMapper>::New(); ///< 将点坐标与可选 RGB 数据映射到 Actor 的管线节点。
    vtkSmartPointer<vtkActor> actor =
        vtkSmartPointer<vtkActor>::New(); ///< 以点模式绘制的 Actor，保存点大小、统一颜色与显隐状态。
    std::shared_ptr<const robot3d::domain::PointCloud> cloud; ///< 当前显示的只读点云；空指针表示没有已加载的点云。
    explicit Implementation(QWidget *owner) : surface(new VtkPointCloudRenderSurface(owner))
    {
        window->SetMultiSamples(0);
        window->AddRenderer(renderer);
        surface->setRenderWindow(window);
        auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        window->GetInteractor()->SetInteractorStyle(style);
        renderer->SetBackground(.07, .10, .16);
        renderer->SetBackground2(.16, .21, .29);
        renderer->GradientBackgroundOn();
        auto data = vtkSmartPointer<vtkPolyData>::New();
        mapper->SetInputData(data);
        mapper->SetColorModeToDirectScalars();
        mapper->SetScalarModeToUsePointData();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetRepresentationToPoints();
        actor->GetProperty()->SetPointSize(3);
        actor->GetProperty()->LightingOff();
        actor->GetProperty()->SetColor(.2, .8, 1);
        renderer->AddActor(actor);
    }
};
VtkPointCloudViewport::VtkPointCloudViewport(QWidget *parent) : QWidget(parent), impl_(new Implementation(this))
{
    setObjectName("pointCloudViewport");
    impl_->surface->setObjectName("pointCloudVtkSurface");
    auto *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->addWidget(impl_->surface);
    impl_->surface->ready = [this]
    {
        render();
    };
    setMinimumSize(360, 280);
}
VtkPointCloudViewport::~VtkPointCloudViewport()
{
    impl_->surface->ready = nullptr;
    if (impl_->surface->isValid())
        impl_->surface->makeCurrent();
    impl_->renderer->RemoveActor(impl_->actor);
    impl_->surface->setRenderWindow(static_cast<vtkGenericOpenGLRenderWindow *>(nullptr));
    if (impl_->surface->isValid())
        impl_->surface->doneCurrent();
}
void VtkPointCloudViewport::setCloud(std::shared_ptr<const robot3d::domain::PointCloud> cloud)
{
    auto data = vtkSmartPointer<vtkPolyData>::New();
    if (cloud)
    {
        const auto count = static_cast<vtkIdType>(cloud->points.size());
        auto points = vtkSmartPointer<vtkPoints>::New();
        points->SetDataTypeToDouble();
        points->SetNumberOfPoints(count);
        auto vertices = vtkSmartPointer<vtkCellArray>::New();
        vertices->AllocateEstimate(count, 1);
        auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
        colors->SetName("RGB");
        colors->SetNumberOfComponents(3);
        if (cloud->hasColors)
            colors->SetNumberOfTuples(count);
        for (vtkIdType i = 0; i < count; ++i)
        {
            const auto &p = cloud->points[std::size_t(i)];
            points->SetPoint(i, p.x, p.y, p.z);
            vertices->InsertNextCell(1, &i);
            if (cloud->hasColors)
                colors->SetTypedTuple(i, p.color.data());
        }
        data->SetPoints(points);
        data->SetVerts(vertices);
        if (cloud->hasColors)
            data->GetPointData()->SetScalars(colors);
    }
    impl_->mapper->SetInputData(data);
    impl_->cloud = std::move(cloud);
    resetCamera();
}
void VtkPointCloudViewport::setPointSize(int size)
{
    impl_->actor->GetProperty()->SetPointSize(size);
    render();
}
void VtkPointCloudViewport::setColor(const QColor &color, bool useFileColors)
{
    impl_->actor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
    impl_->mapper->SetScalarVisibility(useFileColors && impl_->cloud && impl_->cloud->hasColors);
    render();
}
void VtkPointCloudViewport::setCloudVisible(bool visible)
{
    impl_->actor->SetVisibility(visible);
    render();
}
void VtkPointCloudViewport::resetCamera()
{
    auto *camera = impl_->renderer->GetActiveCamera();
    camera->SetPosition(0, 0, 1);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);
    if (pointCount())
        impl_->renderer->ResetCamera(impl_->mapper->GetInput()->GetBounds());
    impl_->renderer->ResetCameraClippingRange();
    render();
}
bool VtkPointCloudViewport::isRendererReady() const
{
    return impl_->surface->isValid();
}
std::size_t VtkPointCloudViewport::pointCount() const
{
    return impl_->cloud ? impl_->cloud->points.size() : 0;
}
void VtkPointCloudViewport::render()
{
    if (!isVisible() || !isRendererReady())
        return;
    impl_->surface->makeCurrent();
    impl_->window->Render();
    impl_->surface->doneCurrent();
    impl_->surface->update();
}
QImage VtkPointCloudViewport::capture()
{
    render();
    return impl_->surface->grabFramebuffer();
}
