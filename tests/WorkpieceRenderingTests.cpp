#include "robot3d/shell/Robot3dDemoView.h"
#include "robot3d/shell/RobotViewport.h"
#include "robot3d/shell/opengl/OpenGlRobotViewport.h"
#include "robot3d/rendering/WorkpieceSceneMath.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <iostream>
#include <functional>
#include <stdexcept>
namespace {
int checks=0;
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;}
void waitFor(const std::function<bool()> &f)
{
    QElapsedTimer timer;timer.start();
    while(!f()&&timer.elapsed()<15000){QCoreApplication::processEvents(QEventLoop::AllEvents,30);QThread::msleep(5);}
    require(f(),"timed out waiting for render");
}
class View:public Robot3dDemoView {
public:
    robot3d::scene::RobotSceneSnapshot scene;
    void submitScene(const robot3d::scene::RobotSceneSnapshot &s) override {scene=s;Robot3dDemoView::submitScene(s);}
};
robot3d::domain::WorkpieceAssetPtr fixture()
{
    using namespace robot3d::domain;
    std::shared_ptr<WorkpieceAsset> a(new WorkpieceAsset);
    a->id="render-test-box";a->name="Box";a->normalized=true;a->unitKnown=true;a->sourceUnit="mm";
    a->boundsMin=Vec3d(-100,-100,-100);a->boundsMax=Vec3d(100,100,100);
    WorkpieceComponent c;c.id="body";c.name="Box body";
    WorkpieceMesh mesh;
    const double vertices[8][3]={{-100,-100,-100},{100,-100,-100},{100,100,-100},{-100,100,-100},
                                {-100,-100,100},{100,-100,100},{100,100,100},{-100,100,100}};
    for(const auto &p:vertices)
    {
        WorkpieceVertex v;v.position=Vec3d(p[0],p[1],p[2]);v.normal=Vec3d(p[0]/173.205,p[1]/173.205,p[2]/173.205);
        v.color={{0.2f,0.6f,0.8f}};mesh.vertices.push_back(v);
    }
    const std::uint32_t ids[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
    mesh.indices.assign(ids,ids+36);c.meshes.push_back(mesh);a->components.push_back(c);return a;
}
void click(QWidget *surface)
{
    const QPointF p=surface->rect().center();
    QMouseEvent press(QEvent::MouseButtonPress,p,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease,p,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(surface,&press);QCoreApplication::sendEvent(surface,&release);
}
}
int main(int argc,char **argv)
{
    QApplication app(argc,argv);
    try
    {
        require(argc==3,"root and output arguments required");QDir::setCurrent(QString::fromLocal8Bit(argv[1]));
        QDir output(QString::fromLocal8Bit(argv[2]));require(output.mkpath("."),"output directory");
        qunsetenv("DEMO_RENDERER");qunsetenv("DEMO_DUMP_FRAME");
        View view;view.resize(1280,850);view.show();
        auto *viewport=view.findChild<RobotViewport*>();require(viewport,"viewport");
        waitFor([&](){return bool(view.scene.state.configuration)&&!viewport->capture().isNull();});
        auto scene=view.scene;
        using namespace robot3d::domain;
        WorkpieceInstance box;box.id="box";box.asset=fixture();box.pose.X=1200;box.pose.Z=800;
        scene.workpieces.push_back(box);scene.selectedWorkpieceId="box";
        scene.workpieces[0].displayMode=WorkpieceDisplayMode::Solid;
        std::string error;
        const int variants=RobotViewport::vtkAvailable()?2:1;
        QString selected;
        QObject::connect(viewport,&RobotViewport::workpieceSelected,&view,[&](const QString &id){selected=id;});
        for(int backend=0;backend<variants;++backend)
        {
            const QString name=backend?"vtk":"opengl";
            if(viewport->backend()!=name)
            {
                require(viewport->requestBackend(name),"switch requested");
                waitFor([&](){return viewport->backend()==name&&!viewport->switching();});
            }
            scene.workpieces[0].visible=true;scene.workpieces[0].opacity=1;
            scene.workpieces[0].displayMode=WorkpieceDisplayMode::Solid;
            require(viewport->prepareWorkpieces(scene,error),"prepare valid workpiece");
            viewport->submitScene(scene);viewport->focusWorkpiece("box");
            const auto solid=viewport->capture();require(!solid.isNull(),"solid render");
            require(solid.save(output.filePath(name+"-workpiece-solid.png")),"save solid");

            scene.workpieces[0].visible=false;viewport->submitScene(scene);
            const auto hidden=viewport->capture();require(hidden!=solid,"visibility changes frame");
            QWidget *surface=backend?view.findChild<QWidget*>("vtkSurface"):static_cast<QWidget*>(view.findChild<OpenGlRobotViewport*>());
            require(surface,"input surface");selected="sentinel";click(surface);require(selected.isEmpty(),"hidden workpiece is not picked");

            scene.workpieces[0].visible=true;scene.workpieces[0].opacity=0.35;viewport->submitScene(scene);
            const auto transparent=viewport->capture();require(transparent!=solid&&transparent!=hidden,"opacity changes frame");
            selected.clear();click(surface);require(selected=="box","transparent workpiece remains selectable");

            scene.workpieces[0].opacity=1;scene.workpieces[0].displayMode=WorkpieceDisplayMode::Wireframe;
            viewport->submitScene(scene);const auto wire=viewport->capture();require(wire!=solid,"wireframe changes frame");
            scene.workpieces[0].displayMode=WorkpieceDisplayMode::SolidEdges;scene.workpieces[0].showAxes=true;
            viewport->submitScene(scene);const auto edges=viewport->capture();require(edges!=wire,"edges and axes change frame");

            const auto valid=viewport->capture();auto bad=scene;
            std::shared_ptr<WorkpieceAsset> broken(new WorkpieceAsset(*scene.workpieces[0].asset));
            broken->components[0].meshes[0].indices[0]=999999;bad.workpieces[0].asset=broken;
            require(!viewport->prepareWorkpieces(bad,error)&&!error.empty(),"bad topology preparation fails");
            require(viewport->capture()==valid,"failed preparation preserves scene and camera");

            // Ray order is checked independently of GPU lighting and drawing order.
            auto nearerInstance=scene.workpieces[0];nearerInstance.id="near";nearerInstance.pose.X+=300;scene.workpieces.push_back(nearerInstance);
            robot3d::rendering::CameraState camera;
            require(robot3d::rendering::workpieceqt::focus(scene,"box",false,1.0,camera),"focus numerical fixture");
            const auto projection=robot3d::rendering::workpieceqt::projection(camera,1.0);
            const auto matrix=robot3d::rendering::workpieceqt::view(scene,camera,QVector3D(),QVector3D());
            require(robot3d::rendering::workpieceqt::pick(scene,camera,projection,matrix,QSize(600,600),QPointF(300,300))=="near","nearest triangle picked");
            scene.workpieces.pop_back();
        }
        view.close();QCoreApplication::processEvents();
        std::cout<<"Workpiece rendering: "<<checks<<" checks passed\n";return 0;
    }
    catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
