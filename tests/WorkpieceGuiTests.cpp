#include <QApplication>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QComboBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDebug>
#include "robot3d/shell/Robot3dDemoView.h"
#include "robot3d/shell/RobotViewport.h"
#include "robot3d/shell/WorkpiecePanel.h"
#include "robot3d/shell/WorkpieceImportDialog.h"
#include "robot3d/infrastructure/WorkpieceSceneStore.h"
#include <functional>
#include <cmath>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif
#ifdef ROBOT3D_WITH_OCCT
#include <BRepPrimAPI_MakeBox.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <IGESCAFControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDataStd_Name.hxx>
#endif
namespace {
int failed=0,passed=0;
QString previewScreenshotPath;
quint64 privateMemory(){
#ifdef Q_OS_WIN
 PROCESS_MEMORY_COUNTERS_EX m={};m.cb=sizeof(m);if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m),sizeof(m)))return quint64(m.PrivateUsage);
#endif
 return 0;
}
void check(bool ok,const char *name){if(ok){++passed;qInfo()<<"PASS"<<name;}else{++failed;qCritical()<<"FAIL"<<name;}}
bool pump(const std::function<bool()> &p,int timeout=15000){QElapsedTimer t;t.start();while(t.elapsed()<timeout){QApplication::processEvents();if(p())return true;QThread::msleep(5);}return false;}
bool importUi(WorkpiecePanel *panel,const QString &path,bool unknownUnit=true){
 QTimer timer;int stage=0;bool committed=false;QElapsedTimer deadline;deadline.start();
 QObject::connect(&timer,&QTimer::timeout,[&]{auto *d=qobject_cast<WorkpieceImportDialog*>(QApplication::activeModalWidget());if(!d)return;
  if(deadline.elapsed()>20000){d->reject();return;}if(stage==0){d->setFilePath(path);d->loadPreview();stage=1;return;}
  auto *tree=d->findChild<QTreeWidget*>("workpieceComponents");
  if(stage==1&&!d->isLoading()&&tree&&tree->topLevelItemCount()){if(unknownUnit)d->findChild<QComboBox*>("workpieceUnit")->setCurrentIndex(1);stage=2;}
  if(stage==2){auto *b=d->findChild<QPushButton*>("workpieceCommitButton");if(b&&b->isEnabled()){stage=3;if(!previewScreenshotPath.isEmpty())d->grab().save(previewScreenshotPath);b->click();committed=true;}}
 });timer.start(20);panel->importWorkpiece();timer.stop();return committed;
}
}
int main(int argc,char **argv){
 QApplication app(argc,argv);if(argc<3)return 2;QString root=QString::fromLocal8Bit(argv[1]),out=QString::fromLocal8Bit(argv[2]);QDir().mkpath(out);QDir::setCurrent(root);previewScreenshotPath=out+"/workpiece-preview.png";
 QString path=out+QString::fromUtf8("/工件预览测试.ply");QFile file(path);QByteArray ply="ply\nformat ascii 1.0\nelement vertex 4\nproperty float x\nproperty float y\nproperty float z\nelement face 4\nproperty list uchar int vertex_indices\nend_header\n0 0 0\n100 0 0\n0 200 0\n0 0 300\n3 0 2 1\n3 0 1 3\n3 0 3 2\n3 1 2 3\n";
 check(file.open(QIODevice::WriteOnly)&&file.write(ply)==ply.size(),"Unicode fixture");file.close();
 Robot3dDemoView view;view.resize(1250,900);view.show();auto *vp=view.findChild<RobotViewport*>();auto *panel=view.findChild<WorkpiecePanel*>();auto *tabs=view.findChild<QTabWidget*>("sceneControlTabs");check(vp&&panel&&tabs,"integrated panel");
 check(pump([&]{auto *b=panel->findChild<QPushButton*>("importWorkpieceButton");return b&&b->isEnabled();}),"robot ready");tabs->setCurrentIndex(1);
 check(importUi(panel,path),"preview and confirm");check(panel->state().instances.size()==1,"one instance");if(panel->state().instances.empty())return 1;
 auto id=panel->state().selectedId;auto a=panel->state().instances.front().asset;check(a->normalized&&std::fabs(a->boundsMax.x-a->boundsMin.x-100)<1e-8,"physical size");
 vp->focusWorkpiece(QString::fromStdString(id));QApplication::processEvents();view.grab().save(out+"/workpiece-main-opengl.png");
 auto *x=panel->findChild<QDoubleSpinBox*>("workpiecePose0");x->setValue(350);panel->findChild<QPushButton*>("workpieceApply")->click();check(std::fabs(panel->state().instances.front().pose.X-350)<1e-8,"numeric placement");
 panel->findChild<QPushButton*>("workpieceUndo")->click();check(std::fabs(panel->state().instances.front().pose.X)<1e-8,"undo pose");
 panel->findChild<QPushButton*>("workpieceRedo")->click();check(std::fabs(panel->state().instances.front().pose.X-350)<1e-8,"redo pose");
  auto precisionScene=panel->currentScene();precisionScene.state.baseInWorld.X=0.123456789;precisionScene.state.baseInWorld.Rz=0.234567891;panel->decorateScene(precisionScene);
  auto expectedWorld=robot3d::domain::workpieceWorldMatrix(panel->state().instances.front(),precisionScene.state);
  auto *parent=panel->findChild<QComboBox*>("workpieceParent");parent->setCurrentIndex(parent->findData("world"));panel->findChild<QPushButton*>("workpieceApply")->click();
  auto placedWorld=robot3d::domain::workpieceWorldMatrix(panel->state().instances.front(),precisionScene.state);double precisionError=0;for(int i=0;i<16;++i)precisionError=std::max(precisionError,std::fabs(expectedWorld.values[i]-placedWorld.values[i]));check(precisionError<1e-9,"reparent keeps full precision");
  auto *lock=panel->findChild<QCheckBox*>("workpieceLocked");lock->setChecked(true);panel->findChild<QPushButton*>("workpieceApply")->click();check(panel->state().instances.front().locked,"lock precise pose");
  lock->setChecked(false);panel->findChild<QPushButton*>("workpieceApply")->click();check(!panel->state().instances.front().locked,"unlock precise pose");
  auto afterUnlock=robot3d::domain::workpieceWorldMatrix(panel->state().instances.front(),precisionScene.state);double unlockError=0;for(int i=0;i<16;++i)unlockError=std::max(unlockError,std::fabs(expectedWorld.values[i]-afterUnlock.values[i]));check(unlockError<1e-9,"property edit preserves precise pose");
 check(importUi(panel,path),"repeat import");check(panel->state().instances.size()==2,"two instances");check(panel->state().instances[0].id!=panel->state().instances[1].id,"independent ids");check(panel->state().instances[0].name!=panel->state().instances[1].name,"unique names");
 QString error,package=out+"/portable.r3wp";check(robot3d::infrastructure::saveWorkpieceScene(package,panel->state(),error),"save package");QFile::remove(path);robot3d::domain::WorkpieceSceneState restored;
 check(robot3d::infrastructure::loadWorkpieceScene(package,restored,error),"restore without source");check(restored.instances.size()==2&&!restored.instances[0].asset->sourceBytes.empty(),"embedded geometry and source");
 if(RobotViewport::vtkAvailable()){check(vp->requestBackend("vtk"),"request vtk");check(pump([&]{return vp->backend()=="vtk"&&!vp->switching();}),"vtk scene switch");check(panel->state().instances.size()==2,"switch retains objects");vp->fitAllWorkpieces();QApplication::processEvents();view.grab().save(out+"/workpiece-main-vtk.png");check(vp->requestBackend("opengl"),"return opengl");check(pump([&]{return vp->backend()=="opengl"&&!vp->switching();}),"opengl restore");}
 panel->findChild<QPushButton*>("workpieceDelete")->click();check(panel->state().instances.size()==1,"delete instance");panel->findChild<QPushButton*>("workpieceUndo")->click();check(panel->state().instances.size()==2,"undo delete");
 {robot3d::domain::RobotStateSnapshot robot;WorkpieceImportDialog d(robot,&view);d.setFilePath(out+"/missing.step");d.loadPreview();d.reject();check(panel->state().instances.size()==2,"cancel preserves scene");}
 const QByteArray stl="solid plate\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 100 0 0\nvertex 0 100 0\nendloop\nendfacet\nendsolid plate\n";
 const QByteArray obj="o plate\nv 0 0 0\nv 100 0 0\nv 0 200 0\nf 1 2 3\n";
 for(int format=0;format<2;++format){QString model=out+(format?"/preview.obj":"/preview.stl");QFile mesh(model);auto bytes=format?obj:stl;check(mesh.open(QIODevice::WriteOnly)&&mesh.write(bytes)==bytes.size(),"mesh preview fixture");mesh.close();auto count=panel->state().instances.size();check(importUi(panel,model),"mesh format preview and confirm");check(panel->state().instances.size()==count+1,"mesh format scene commit");}
#ifdef ROBOT3D_WITH_OCCT
 Handle(TDocStd_Document) doc=new TDocStd_Document(TCollection_ExtendedString("BinXCAF"));
 auto shape=XCAFDoc_DocumentTool::ShapeTool(doc->Main())->AddShape(BRepPrimAPI_MakeBox(100,200,300).Shape(),Standard_False);TDataStd_Name::Set(shape,TCollection_ExtendedString("CAD preview box"));
 QString step=out+"/preview-box.stp";STEPCAFControl_Writer sw;check(sw.Transfer(doc)&&sw.Write(step.toUtf8().constData())==IFSelect_RetDone,"STEP GUI fixture");
 QString iges=out+"/preview-box.igs";IGESCAFControl_Writer iw;check(iw.Transfer(doc)&&iw.Write(iges.toUtf8().constData()),"IGES GUI fixture");
 const QStringList cadModels=QStringList()<<step<<iges;
 for(const QString &model:cadModels){auto count=panel->state().instances.size();check(importUi(panel,model,false),"CAD preview and confirm");check(panel->state().instances.size()==count+1,"CAD scene commit");if(panel->state().instances.size()==count+1){auto asset=panel->state().instances.back().asset;check(std::fabs(asset->boundsMax.x-asset->boundsMin.x-100)<.01&&std::fabs(asset->boundsMax.z-asset->boundsMin.z-300)<.01,"CAD GUI physical size");}}
#endif
 {
  const auto robot=panel->currentScene().state;const auto count=panel->state().instances.size();const QString first=out+"/race-a.ply",second=out+"/race-b.ply";QFile af(first),bf(second);QByteArray changed=ply;changed.replace("0 0 300","0 0 337");
  check(af.open(QIODevice::WriteOnly)&&af.write(ply)==ply.size()&&bf.open(QIODevice::WriteOnly)&&bf.write(changed)==changed.size(),"interleaved source fixtures");af.close();bf.close();
  WorkpieceImportDialog race(robot,&view);race.show();race.setFilePath(first);race.loadPreview();race.setFilePath(second);race.loadPreview();
  check(pump([&]{auto *tree=race.findChild<QTreeWidget*>("workpieceComponents");return tree&&tree->topLevelItemCount()&&!race.isLoading();}),"newest raw result ready");
  race.findChild<QComboBox*>("workpieceUnit")->setCurrentIndex(1);
  check(pump([&]{auto asset=race.previewAsset();auto *commit=race.findChild<QPushButton*>("workpieceCommitButton");return asset&&std::fabs(asset->boundsMax.z-asset->boundsMin.z-337)<1e-8&&commit&&commit->isEnabled();}),"stale import cannot replace newest preview");
  race.reject();check(panel->state().instances.size()==count,"cancel ready preview preserves scene");
  {WorkpieceImportDialog closing(robot,&view);closing.show();closing.setFilePath(first);closing.loadPreview();closing.reject();}
  QElapsedTimer settle;settle.start();pump([&]{return settle.elapsed()>200;},1000);check(panel->state().instances.size()==count,"close active valid load preserves scene");
 }
 if(argc>3&&(QString::fromLocal8Bit(argv[3])=="--lifecycle"||QString::fromLocal8Bit(argv[3])=="--lifecycle-vtk")){
  if(QString::fromLocal8Bit(argv[3])=="--lifecycle-vtk"){check(vp->requestBackend("vtk")&&pump([&]{return vp->backend()=="vtk"&&!vp->switching();}),"VTK lifecycle backend");}
  const QString life=out+"/lifecycle.ply";QFile fixture(life);QByteArray lifecyclePly=ply;lifecyclePly.replace("0 0 300","0 0 301");check(fixture.open(QIODevice::WriteOnly)&&fixture.write(lifecyclePly)==lifecyclePly.size(),"lifecycle distinct source");fixture.close();auto count=panel->state().instances.size();quint64 initial=privateMemory(),maximum=initial;QElapsedTimer total;total.start();QJsonArray times;bool lifecycle=true;
  for(int cycle=0;cycle<50;++cycle){QElapsedTimer elapsed;elapsed.start();if(!importUi(panel,life)){lifecycle=false;break;}panel->findChild<QPushButton*>("workpieceDelete")->click();QApplication::processEvents();if(panel->state().instances.size()!=count){lifecycle=false;break;}times.append(double(elapsed.elapsed()));maximum=std::max(maximum,privateMemory());}
  quint64 finalMemory=privateMemory();check(lifecycle&&times.size()==50,"50 preview import delete cycles");check(finalMemory<=initial+100ull*1024*1024,"lifecycle private memory bounded");QJsonObject record;record["backend"]=vp->backend();record["assetDistinctFromInitial"]=true;record["cycles"]=times.size();record["elapsedMs"]=double(total.elapsed());record["cycleMs"]=times;record["initialPrivateBytes"]=double(initial);record["finalPrivateBytes"]=double(finalMemory);record["maxSampledPrivateBytes"]=double(maximum);record["passed"]=lifecycle&&times.size()==50&&finalMemory<=initial+100ull*1024*1024;QFile report(out+"/gui-lifecycle.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(record).toJson());
 }
 view.hide();qInfo()<<"Workpiece GUI:"<<passed<<"passed,"<<failed<<"failed";return failed?1:0;
}
