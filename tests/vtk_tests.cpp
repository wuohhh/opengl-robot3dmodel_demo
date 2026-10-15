#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QDir>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QEvent>
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include "demoui.h"
#include "3dmodel/robot3dglwidget.h"
#include "3dmodel/robotgeometry.h"
#include "robot3d/rendering/RobotViewport.h"
#include "robot3d/rendering/vtk/VtkRobotViewport.h"
#include "robot3d/rendering/vtk/VtkRobotSceneRenderer.h"
namespace {
int checks=0;
void require(bool condition,const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
void waitFor(const std::function<bool()>& predicate) {
    QElapsedTimer time; time.start();
    while(!predicate()) {
        if(time.elapsed()>20000) throw std::runtime_error("VTK GUI wait timed out");
        QCoreApplication::processEvents(); QThread::msleep(5);
    }
    QCoreApplication::processEvents();
}
class RecordingView : public DemoUi {
public:
    robot3d::presentation::Robot3dDisplayModel model;
    void showState(const robot3d::presentation::Robot3dDisplayModel& value) override { DemoUi::showState(value); model=value; }
};
bool hasGeometry(const QImage& image) {
    int colored=0;
    for(int y=0;y<image.height();y+=3) for(int x=0;x<image.width();x+=3) {
        const QColor c=image.pixelColor(x,y);
        if(c.red()<235||c.green()<235||c.blue()<235) ++colored;
    }
    return colored>100;
}
void compareLinks(Robot3dGLWidget* gl,VtkRobotViewport* vtk,const robot3d::scene::RobotSceneSnapshot& scene) {
    gl->makeCurrent();
    {
        RobotGeometry reference;
        require(reference.ChangeRobot3D(QString::fromStdString(scene.state.configuration->modelDirectory),scene.state.configuration->dh.data()),"legacy reference model loads");
        auto angles=robot3d::domain::jointValues(scene.state.joints); reference.multjointTransform(angles.data());
        for(int i=0;i<6;++i) {
            const auto actual=vtk->renderer()->linkMatrix(i+1);
            const auto expected=reference.jointData[i].transform;
            double error=0;
            for(int r=0;r<4;++r) for(int c=0;c<4;++c) error=qMax(error,std::fabs(double(actual(r,c)-expected(r,c))));
            require(error<1e-4,"VTK link matrix agrees with original GL geometry");
        }
    }
    gl->doneCurrent();
}
}
int main(int argc,char** argv) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc,argv);
    try {
        require(argc==3,"project root and output directory required");
        QDir::setCurrent(QString::fromLocal8Bit(argv[1]));
        QDir output(QString::fromLocal8Bit(argv[2])); require(output.mkpath("."),"output directory");
        qunsetenv("DEMO_DUMP_FRAME"); qunsetenv("DEMO_RENDERER"); qunsetenv("DEMO_ROBOT_TYPE");
        {
            RecordingView view; view.resize(1280,800); view.show();
            auto* viewport=view.findChild<RobotViewport*>(); auto* gl=view.findChild<Robot3dGLWidget*>();
            auto* backend=view.findChild<QComboBox*>("renderBackendCombo");
            require(viewport&&gl&&backend&&backend->count()==2,"both backends exposed");
            int frames=0,failures=0;
            QObject::connect(viewport,&RobotViewport::framePresented,&view,[&](qulonglong) { ++frames; });
            QObject::connect(viewport,&RobotViewport::renderFailed,&view,[&](const QString& error) { ++failures; std::cerr<<error.toStdString()<<"\n"; });
            waitFor([&] { return view.model.state.configuration&&frames>0; });
            require(viewport->backend()=="opengl","OpenGL remains default");
            const int angles[]={42,-30,25,10,-15,35};
            for(int i=0;i<6;++i) view.findChild<QSlider*>(QString("jointSlider%1").arg(i+1))->setValue(angles[i]);
            // Joint labels change size hints; settle posted layout requests before taking the reference.
            QCoreApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest); QCoreApplication::processEvents();
            auto camera=gl->cameraState(); camera.rotation=QQuaternion::fromAxisAndAngle(0,0,1,17); camera.translation=QVector3D(0,2,1); camera.scale=1.1;
            gl->setCameraState(camera);
            const auto referenceImage=viewport->capture();
            require(referenceImage.save(output.filePath("opengl-reference.png")),"save OpenGL reference");
            const auto revision=view.model.state.revision;
            backend->setCurrentIndex(backend->findData("vtk"));
            waitFor([&] { return viewport->backend()=="vtk"&&!viewport->switching(); });
            auto* vtk=view.findChild<VtkRobotViewport*>(); require(vtk&&vtk->isRendererReady(),"VTK initialized");
            require(view.model.state.revision==revision&&view.model.state.joints.L1==42,"switch preserves application state");
            require(std::fabs(vtk->cameraState().scale-camera.scale)<1e-6&&(vtk->cameraState().translation-camera.translation).length()<1e-5,"switch preserves camera");
            auto* coordinate=view.findChild<QComboBox*>("coordinateCombo");
            auto* overlay=vtk->findChild<QComboBox*>("selCoordSysCb"); require(coordinate&&overlay,"VTK coordinate overlay exists");
            coordinate->setCurrentIndex(coordinate->findData("p_0"));
            require(overlay->currentData()=="p_0","VTK overlay follows side coordinate selection");
            overlay->setCurrentIndex(overlay->findData("t_0"));
            require(view.model.state.coordinateId=="t_0"&&coordinate->currentData()=="t_0","VTK overlay sends coordinate intent");
            coordinate->setCurrentIndex(coordinate->findData("b_0"));
            auto image=viewport->capture(); require(!image.isNull()&&hasGeometry(image),"VTK frame contains geometry");
            require(image.size()==referenceImage.size(),"backend switch preserves viewport size for comparison");
            require(image.save(output.filePath("vtk-reference.png")),"save VTK reference");
            const auto types=view.findChild<QComboBox*>("robotTypeCombo");
            const char* names[]={"G6-1-1","X20-1-3","G6L-3-20"};
            const int triangles[]={2306,2048,2300};
            for(int k=0;k<3;++k) {
                types->setCurrentText(names[k]);
                require(view.model.state.configuration->id==names[k],"VTK model switches");
                viewport->capture();
                require(vtk->renderer()->triangleCount()==triangles[k],"VTK preserves OBJ face topology");
                robot3d::scene::RobotSceneSnapshot scene; scene.state=view.model.state; scene.options=view.model.options;
                compareLinks(gl,vtk,scene);
                require(viewport->capture().save(output.filePath(QString("vtk-%1.png").arg(names[k]))),"save switched VTK model");
            }
            const auto before=viewport->capture();
            std::shared_ptr<robot3d::domain::RobotConfiguration> bad(new robot3d::domain::RobotConfiguration(*view.model.state.configuration));
            bad->modelDirectory="missing-model-directory"; std::string error;
            require(!view.prepareModel(bad,error)&&!error.empty(),"VTK rejects missing mesh");
            require(viewport->capture()==before,"failed VTK preparation retains displayed scene");
            robot3d::scene::RobotSceneSnapshot scene; scene.state=view.model.state; scene.options=view.model.options;
            scene.options.tool=true; scene.options.waypoints=true; scene.options.trajectory=true; scene.options.safePlanes=true; scene.options.targetRobot=true;
            scene.targetJoints={-20,30,40,0,0,0};
            scene.state.waypoints.push_back(scene.state.tcp);
            scene.state.trajectory={{0,0,0,0,0,0},scene.state.tcp};
            for(int i=0;i<4;++i) {
                robot3d::scene::ToolShape tool; tool.type=i; tool.pose={double(i*60),0,200,10,20,30};
                tool.radius=10; tool.length=20; tool.width=30; tool.height=50; scene.tools.push_back(tool);
            }
            robot3d::scene::SafePlane plane; plane.corners={{{{0,0,100}},{{100,0,100}},{{100,100,100}},{{0,100,100}}}}; plane.alpha=.3f; scene.planes.push_back(plane);
            view.submitScene(scene); require(hasGeometry(viewport->capture()),"VTK renders tools, planes, target, path and markers");
            require(viewport->capture().save(output.filePath("vtk-all-primitives.png")),"save VTK primitives");
            auto* surface=vtk->findChild<QOpenGLWidget*>("vtkSurface"); require(surface!=nullptr,"native VTK surface exists");
            surface->makeCurrent(); require(surface->context()->functions()->glGetError()==GL_NO_ERROR,"VTK reports no GL error"); surface->doneCurrent();
            for(int i=0;i<3;++i) {
                backend->setCurrentIndex(backend->findData("opengl")); waitFor([&] { return viewport->backend()=="opengl"; });
                require(hasGeometry(viewport->capture()),"OpenGL resumes after VTK");
                backend->setCurrentIndex(backend->findData("vtk")); waitFor([&] { return viewport->backend()=="vtk"; });
                require(hasGeometry(viewport->capture()),"VTK resumes after OpenGL");
            }
            scene.options.install=true; view.submitScene(scene); viewport->capture();
            backend->setCurrentIndex(backend->findData("opengl")); waitFor([&] { return viewport->backend()=="opengl"; });
            scene.options.install=false; view.submitScene(scene); viewport->capture();
            require(std::fabs(gl->cameraState().scale-camera.scale)<1e-6,"installation view backup survives backend switch");
            backend->setCurrentIndex(backend->findData("vtk")); waitFor([&] { return viewport->backend()=="vtk"; });
            auto* sim=view.findChild<QCheckBox*>("simulationCheck"); sim->click();
            const auto previous=view.model.state.revision; waitFor([&] { return view.model.state.revision>previous; }); sim->click();
            require(!view.model.simulationRunning,"VTK simulation stops");
            view.hide(); view.show(); require(hasGeometry(viewport->capture()),"VTK survives hide/show");
            require(failures==0,"no backend rendering errors"); view.close();
        }
        qputenv("DEMO_RENDERER","vtk");
        {
            RecordingView view; view.resize(1280,800); view.show();
            auto* viewport=view.findChild<RobotViewport*>();
            waitFor([&] { return view.model.state.configuration&&viewport->backend()=="vtk"&&!viewport->capture().isNull(); });
            require(hasGeometry(viewport->capture()),"VTK startup backend and window recreation");
            view.close();
        }
        std::cout<<"VTK regression: "<<checks<<" checks passed\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<"\n"; return 1; }
}
