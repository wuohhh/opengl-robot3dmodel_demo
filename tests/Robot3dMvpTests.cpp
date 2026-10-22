#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cmath>
#include "robot3d/domain/Kinematics.h"
#include "robot3d/domain/TrajectorySampler.h"
#include "robot3d/infrastructure/XmlRobotConfigRepository.h"
#include "robot3d/presentation/Robot3dPresenter.h"
using namespace robot3d;
namespace {
int checks=0;
void require(bool ok,const char* message) { ++checks; if(!ok) throw std::runtime_error(message); }
bool near(double a,double b,double tolerance=1e-7) { return std::fabs(a-b)<tolerance; }
struct View : presentation::IRobot3dView {
    bool rejectModel=false; int updates=0,scenes=0,errors=0;
    presentation::Robot3dDisplayModel model;
    scene::RobotSceneSnapshot scene;
    bool prepareModel(domain::RobotConfigurationPtr,std::string& error) override { if(rejectModel) error="mesh failure"; return !rejectModel; }
    void showState(const presentation::Robot3dDisplayModel& value) override { model=value; ++updates; }
    void submitScene(const scene::RobotSceneSnapshot& value) override { scene=value; ++scenes; }
    void showError(const std::string&) override { ++errors; }
    void appendLog(const std::string&) override {}
};
void fixture(const QDir& root,const QString& id,const QByteArray& xml) {
    root.mkpath(id); QFile f(root.filePath(id+"/config.xml")); require(f.open(QIODevice::WriteOnly),"fixture open"); require(f.write(xml)==xml.size(),"fixture write");
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        require(argc==2,"robotType root required");
        infrastructure::XmlRobotConfigRepository repository(argv[1]);
        const char* types[]={"G6L-3-20","G6-1-1","X20-1-3"};
        const double expected[3][3]={
        { -104.27773842531339,928.01557474610593,-466.97496859265345 },
        { 16.170837049006238,524.3617038350427,-151.59323561249082 },
        { -36.526347634996206,977.78558823369792,-383.88621496369308 }
        };
        require(repository.robotTypes().size()>=3,"three robot models discovered");
        application::Robot3dService service(repository); application::SimulationService simulation;
        for(int i=0;i<3;++i) {
            auto config=service.prepareRobotType(types[i]); require(config.ok,"load real config");
            require(service.commitRobotType(config.value).ok,"commit config");
            simulation.reset(); application::SimulationSample sample;
            for(int n=0;n<90;++n) sample=simulation.step(.04);
            require(service.applySample(sample.joints,sample.tcpSpeed).ok,"90 steps");
            const auto p=service.snapshot().tcp;
            require(near(p.X,expected[i][0])&&near(p.Y,expected[i][1])&&near(p.Z,expected[i][2]),"independent DH position golden");
        }
        const auto revision=service.snapshot().revision;
        auto invalid=service.snapshot().joints; invalid.L1=std::numeric_limits<double>::quiet_NaN();
        require(!service.applyJointPose(invalid).ok && service.snapshot().revision==revision,"NaN does not mutate state");
        invalid={0,0,166,0,0,0}; require(!service.applyJointPose(invalid).ok,"demonstration limits at service boundary");
        require(!service.selectCoordinate("missing").ok && service.snapshot().revision==revision,"invalid coordinate atomic");
        require(service.selectCoordinate("t_0").ok,"tool coordinate");
        require(near(service.snapshot().tcp.X,service.snapshot().coordinatePose.X),"tool coordinate tracks TCP");
        domain::EulerPoseDeg pose={120,-30,55,20,-35,50},inverse={},identity={};
        domain::inversePose(inverse,pose); domain::EulerPoseDeg zero={};
        domain::poseRelativeTo(identity,pose,pose);
        require(near(identity.X,0)&&near(identity.Y,0)&&near(identity.Z,0)&&near(identity.Rx,0)&&near(identity.Ry,0)&&near(identity.Rz,0),"relative transform identity");
        domain::AxisAngleRad axis={}; domain::EulerPoseDeg roundtrip={};
        domain::eulerToAxisAngle(pose,axis); domain::axisAngleToEuler(axis,roundtrip);
        require(near(roundtrip.Rx,pose.Rx)&&near(roundtrip.Ry,pose.Ry)&&near(roundtrip.Rz,pose.Rz),"nonzero rotation representation roundtrip");
        domain::TrajectorySampler sampler(2); std::vector<domain::EulerPoseDeg> path;
        require(sampler.append(zero,path),"first path point");
        auto point=zero; point.X=4; require(!sampler.append(point,path),"path sampling threshold");
        point.X=5; require(sampler.append(point,path),"path threshold boundary"); point.X=10; sampler.append(point,path);
        require(path.size()==2 && path.front().X==5,"path bounded capacity");
        const double time=simulation.time(); bool threw=false;
        try { simulation.step(-.1); } catch(const std::invalid_argument&) { threw=true; }
        require(threw&&simulation.time()==time,"invalid simulation step atomic"); simulation.reset(); require(simulation.time()==0,"instance simulation reset");
        QTemporaryDir temp; require(temp.isValid(),"temporary config root"); QDir dir(temp.path());
        fixture(dir,"badxml","<RobotMsg><Feature>"); fixture(dir,"badcount","<RobotMsg><DHpara><j0>0,0,0,0,0</j0></DHpara></RobotMsg>");
        fixture(dir,"badnumber","<RobotMsg><DHpara><j0>0,NaN,0,0,0</j0></DHpara></RobotMsg>");
        infrastructure::XmlRobotConfigRepository broken(temp.path().toStdString());
        require(!broken.load("missing").ok,"missing file fails"); require(!broken.load("../escape").ok,"model ID validation");
        require(!broken.load("badxml").ok,"malformed XML fails"); require(!broken.load("badcount").ok,"missing axes fail"); require(!broken.load("badnumber").ok,"invalid number fails");
        View view; QObject lifetime; application::Robot3dService service2(repository); application::SimulationService sim2;
        presentation::Robot3dPresenter presenter(view,lifetime,service2,sim2);
        presenter.start(types[0]); require(view.model.state.configuration->id==types[0],"presenter initial model");
        const auto current=presenter.state().configuration;
        view.rejectModel=true; require(!presenter.onRobotTypeRequested(types[1]),"GPU preparation rejection");
        require(presenter.state().configuration==current && view.model.state.configuration==current,"failed model keeps state and view");
        view.rejectModel=false; require(presenter.onRobotTypeRequested(types[1]),"model transaction succeeds");
        presenter.onJointPoseRequested({35,-20,15,10,25,-30});
        require(view.scene.state.revision==view.model.state.revision && near(view.scene.state.tcp.X,view.model.state.tcp.X),"single coherent scene/display snapshot");
        auto options=view.model.options; options.trajectory=true; presenter.onDisplayOptionsRequested(options);
        require(view.model.options.trajectory && view.scene.options.trajectory,"display options unified");
        presenter.onCoordinateRequested("p_0"); require(view.scene.state.coordinateId=="p_0"&&view.scene.coordinatePose.X==400,"coordinate ID mapped");
        presenter.onCoordinateRequested("wcs_0"); require(view.scene.coordinatePose.X==-200&&view.scene.coordinatePose.Z==0,"preserve legacy work-coordinate composition");
        presenter.addWaypoint(); require(view.model.state.waypoints.size()==1,"waypoint application entry");
        presenter.onTrajectoryPoint(zero,false); require(view.model.state.trajectory.empty(),"path script state gate");
        presenter.onTrajectoryPoint(zero,true); require(view.model.state.trajectory.size()==1,"path recorded through application");
        const auto simTime=sim2.time(); const int errors=view.errors;
        presenter.advanceSimulation(1,-.1); require(view.errors==errors+1&&sim2.time()==simTime,"presenter invalid timer input rejected");
        View expired; QObject* life=new QObject;
        presentation::Robot3dPresenter detached(expired,*life,service2,sim2);
        delete life; const auto previousRevision=service2.snapshot().revision;
        detached.onJointPoseRequested({0,0,0,0,0,0}); detached.onCoordinateRequested("b_0"); detached.refresh();
        require(expired.updates==0&&service2.snapshot().revision==previousRevision,"expired view callbacks do not update state or UI");
        std::cout<<"MVP regression: "<<checks<<" checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
