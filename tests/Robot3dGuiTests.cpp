#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QDir>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <functional>
#include <iostream>
#include <stdexcept>
#include "robot3d/shell/Robot3dDemoView.h"
#include "robot3d/shell/opengl/OpenGlRobotViewport.h"
#include "robot3d/compatibility/LegacyRobot3dViewAdapter.h"
#include "robot3d/shell/input/LongPressRepeatFilter.h"
namespace
{
int checks = 0;
void require(bool ok, const char *message)
{
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
void waitFor(const std::function<bool()> &predicate)
{
    QElapsedTimer time;
    time.start();
    while (!predicate())
    {
        if (time.elapsed() > 15000)
            throw std::runtime_error("GUI wait timed out");
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    QCoreApplication::processEvents();
}
class RecordingView : public Robot3dDemoView
{
public:
    robot3d::presentation::Robot3dDisplayModel model;
    int updates = 0;
    void showState(const robot3d::presentation::Robot3dDisplayModel &m) override
    {
        Robot3dDemoView::showState(m);
        model = m;
        ++updates;
    }
};
}
int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    try
    {
        require(argc == 3, "project root and output directory required");
        QDir::setCurrent(QString::fromLocal8Bit(argv[1]));
        QDir output(QString::fromLocal8Bit(argv[2]));
        require(output.mkpath("."), "output directory");
        qunsetenv("DEMO_DUMP_FRAME");
        qunsetenv("DEMO_ROBOT_TYPE");
        for (int pass = 0; pass < 2; ++pass)
        {
            RecordingView view;
            view.resize(1280, 800);
            view.show();
            auto *viewport = view.findChild<OpenGlRobotViewport *>();
            require(viewport != nullptr, "viewport exists");
            int frames = 0;
            QObject::connect(viewport, &OpenGlRobotViewport::framePresented, &view,
                             [&frames](qulonglong)
                             {
                                 ++frames;
                             });
            waitFor(
                [&]
                {
                    return view.model.state.configuration && frames > 0;
                });
            require(view.model.state.configuration->id == "G6L-3-20", "first frame model");
            viewport->update();
            const int before = frames;
            waitFor(
                [&]
                {
                    return frames > before;
                });
            require(!viewport->grabFramebuffer().isNull(), "first frame valid");
            auto *cloudButton = view.findChild<QPushButton *>("pointCloudButton");
            require(cloudButton != nullptr, "point cloud entry exists");
#ifndef ROBOT3D_WITH_VTK
            require(!cloudButton->isEnabled() && !cloudButton->toolTip().isEmpty(),
                    "OpenGL-only build explains VTK requirement");
#endif
            if (pass == 1)
            {
                view.close();
                continue;
            }
            view.grab().save(output.filePath("mvp-window.png"));
            std::cout << "Window " << view.width() << "x" << view.height() << " viewport " << viewport->width() << "x"
                      << viewport->height() << "\n";
            auto *joint = view.findChild<QSlider *>("jointSlider1");
            require(joint != nullptr, "joint slider");
            joint->setValue(42);
            require(view.model.state.joints.L1 == 42, "slider -> presenter -> service");
            auto *coordinate = view.findChild<QComboBox *>("coordinateCombo");
            auto *overlay = viewport->findChild<QComboBox *>("selCoordSysCb");
            require(coordinate && overlay, "coordinate controls");
            coordinate->setCurrentIndex(coordinate->findData("p_0"));
            require(view.model.state.coordinateId == "p_0" && overlay->currentData() == "p_0",
                    "side panel coordinate synchronized");
            overlay->setCurrentIndex(overlay->findData("t_0"));
            require(view.model.state.coordinateId == "t_0" && coordinate->currentData() == "t_0",
                    "overlay coordinate synchronized");
            auto *path = viewport->findChild<QPushButton *>("pathCurve");
            auto *sidePath = view.findChild<QCheckBox *>("trajectoryCheck");
            require(path && sidePath, "display controls");
            path->click();
            require(view.model.options.trajectory && sidePath->isChecked(), "overlay display option -> side panel");
            sidePath->click();
            require(!view.model.options.trajectory && !path->isChecked(), "side display option -> overlay");
            auto *type = view.findChild<QComboBox *>("robotTypeCombo");
            for (const char *id : {"G6-1-1", "X20-1-3", "G6L-3-20"})
            {
                type->setCurrentText(id);
                require(view.model.state.configuration->id == id, "live model switch");
                require(view.model.state.joints.L1 == 42, "model switch preserves joint state");
                const int count = frames;
                viewport->update();
                waitFor(
                    [&]
                    {
                        return frames > count;
                    });
                const auto frame = viewport->grabFramebuffer();
                require(!frame.isNull(), "switched scene rendered");
                require(frame.save(output.filePath(QString(id) + ".png")), "save switched frame");
            }
            const auto oldImage = viewport->grabFramebuffer();
            const auto oldConfig = view.model.state.configuration;
            std::shared_ptr<robot3d::domain::RobotConfiguration> bad(
                new robot3d::domain::RobotConfiguration(*oldConfig));
            bad->modelDirectory = "missing-model-directory";
            std::string error;
            require(!view.prepareModel(bad, error), "missing mesh rejected");
            require(!error.empty() && view.model.state.configuration == oldConfig,
                    "failed prepare keeps application state");
            require(viewport->grabFramebuffer() == oldImage, "failed prepare preserves GPU scene");
            robot3d::scene::RobotSceneSnapshot scene;
            scene.state = view.model.state;
            scene.options = view.model.options;
            scene.options.tool = true;
            scene.options.waypoints = true;
            scene.options.trajectory = true;
            scene.coordinatePose = robot3d::scene::legacyDisplayPose(scene.state.coordinatePose);
            scene.state.waypoints.push_back(scene.state.tcp);
            scene.state.trajectory.push_back({0, 0, 0, 0, 0, 0});
            scene.state.trajectory.push_back(scene.state.tcp);
            for (int i = 0; i < 3; ++i)
            {
                robot3d::scene::ToolShape tool;
                tool.type = i;
                tool.pose = {double(i * 60), 0, 200, 0, 0, 0};
                tool.radius = 10;
                tool.height = 50;
                scene.tools.push_back(tool);
            }
            view.submitScene(scene);
            require(!viewport->grabFramebuffer().isNull(), "dynamic tools/path/waypoints");
            viewport->makeCurrent();
            require(viewport->context()->functions()->glGetError() == GL_NO_ERROR,
                    "no GL error after dynamic snapshot upload");
            viewport->doneCurrent();
            // Restore normal snapshot before testing timer and visibility.
            auto *sim = view.findChild<QCheckBox *>("simulationCheck");
            sim->click();
            const auto revision = view.model.state.revision;
            waitFor(
                [&]
                {
                    return view.model.state.revision > revision;
                });
            sim->click();
            require(!view.model.simulationRunning, "simulation toggle and timer");
            view.hide();
            view.show();
            const int count = frames;
            viewport->update();
            waitFor(
                [&]
                {
                    return frames > count;
                });
            view.close();
        }
        // A host facade can be destroyed and recreated without retaining a dangling singleton.
        LegacyRobot3dViewAdapter::InitInstance(nullptr);
        require(LegacyRobot3dViewAdapter::GetInstance() != nullptr, "legacy facade creates");
        auto *adapter = LegacyRobot3dViewAdapter::GetInstance();
        auto *legacyViewport = adapter->openGlViewport(show_CoordSys | show_Zoom, false);
        require(legacyViewport != nullptr, "legacy adapter returns the OpenGL viewport");
        int legacyFrames = 0;
        QObject::connect(legacyViewport, &OpenGlRobotViewport::framePresented, adapter,
                         [&](qulonglong)
                         {
                             ++legacyFrames;
                         });
        adapter->resize(800, 600);
        legacyViewport->resize(800, 600);
        adapter->show();
        waitFor(
            [&]
            {
                return legacyFrames > 0;
            });
        auto *legacyCoordinate = legacyViewport->findChild<QComboBox *>("selCoordSysCb");
        auto *legacyZoom = legacyViewport->findChild<QWidget *>("zoomWidget");
        auto *legacyPath = legacyViewport->findChild<QPushButton *>("pathCurve");
        require(legacyCoordinate && legacyCoordinate->isVisible() && legacyZoom && legacyZoom->isVisible(),
                "legacy display flags reach View state");
        require(legacyPath && !legacyPath->isVisible(), "legacy flags hide unrequested panels");
        require(!legacyViewport->grabFramebuffer().isNull(), "renamed legacy adapter renders a frame");
        adapter->close();
        LegacyRobot3dViewAdapter::UnInitInstance();
        require(LegacyRobot3dViewAdapter::GetInstance() == nullptr, "legacy facade clears");
        LegacyRobot3dViewAdapter::InitInstance(nullptr);
        LegacyRobot3dViewAdapter::UnInitInstance();
        QPushButton repeatButton;
        LongPressRepeatFilter repeatFilter;
        int repeats = 0, stops = 0;
        QObject::connect(&repeatButton, &QPushButton::clicked,
                         [&]
                         {
                             ++repeats;
                         });
        QObject::connect(&repeatFilter, &LongPressRepeatFilter::stopRequested,
                         [&]
                         {
                             ++stops;
                         });
        repeatFilter.senderPushButton = &repeatButton;
        for (int i = 0; i < 3; ++i)
            repeatFilter.handleLongPress_slot();
        repeatFilter.touchEndEvent();
        require(repeats == 3 && stops == 1, "extracted long-press filter repeats and emits stop intent");
        require(repeatFilter.jogKeepCount == 0 && repeatFilter.senderPushButton.isNull(),
                "long-press filter clears input state");
        std::cout << "GUI regression: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
