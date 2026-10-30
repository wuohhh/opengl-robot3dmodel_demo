#include "robot3d/infrastructure/WorkpieceFileLoader.h"
#include "robot3d/domain/Workpiece.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <iostream>
#include <cmath>
#include <cstring>
#include <future>
#include <chrono>
#ifdef ROBOT3D_WITH_OCCT
#include <BRepPrimAPI_MakeBox.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <IGESCAFControl_Writer.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <TDataStd_Name.hxx>
#include <Interface_Static.hxx>
#include <Quantity_Color.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#endif
using namespace robot3d;
struct ConcurrentLoad{bool ok=false;std::shared_ptr<domain::WorkpieceAsset> asset;QString error;};
static int failures=0,checks=0;
static bool traceEnabled(){return !qgetenv("ROBOT3D_LOADER_TRACE").isEmpty();}
static void check(bool ok,const char*name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<std::endl;}}
static bool writeFile(const QString&p,const QByteArray&b){QFile f(p);return f.open(QIODevice::WriteOnly)&&f.write(b)==b.size();}
static bool load(const QString&p,std::shared_ptr<domain::WorkpieceAsset>&a,QString&e,infrastructure::WorkpieceLoadOptions o=infrastructure::WorkpieceLoadOptions()){
 if(traceEnabled())std::cerr<<"LOAD "<<p.toUtf8().constData()<<std::endl;
 if(traceEnabled()){auto callback=o.progress;o.progress=[callback](const QString &stage,int percent){std::cerr<<"STAGE "<<stage.toUtf8().constData()<<" "<<percent<<std::endl;if(callback)callback(stage,percent);};}
 bool ok=infrastructure::loadWorkpieceFile(p,o,a,e);if(traceEnabled())std::cerr<<"LOAD END "<<ok<<" "<<e.toUtf8().constData()<<std::endl;return ok;
}
static QByteArray plyBinary(QDataStream::ByteOrder order) {
 QByteArray b="ply\nformat "+QByteArray(order==QDataStream::LittleEndian?"binary_little_endian":"binary_big_endian")+" 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n";
 QDataStream d(&b,QIODevice::Append);d.setByteOrder(order);d.setFloatingPointPrecision(QDataStream::SinglePrecision);
 for(int i=0;i<3;++i){d<<float(i==1?100:0)<<float(i==2?100:0)<<float(0)<<quint8(i==0?255:0)<<quint8(i==1?255:0)<<quint8(i==2?255:0);}
 d<<quint8(3)<<qint32(0)<<qint32(1)<<qint32(2);return b;
}
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv);QTemporaryDir temp;
 check(temp.isValid(),"temp");
 const QString folder=temp.path()+QString::fromUtf8("/中文 工件");QDir().mkpath(folder);
 const QByteArray stl="solid plate\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 100 0 0\nvertex 0 100 0\nendloop\nendfacet\nendsolid plate\n";
 const QByteArray obj="mtllib colors.mtl\no plate\nv 0 0 0\nv 100 0 0\nv 100 100 0\nv 0 100 0\nusemtl blue\nf -4 -3 -2 -1\n";
 writeFile(folder+"/colors.mtl","newmtl blue\nKd 0.1 0.2 0.8\n");
 writeFile(folder+"/plate.obj",obj);writeFile(folder+"/plate.stl",stl);
 const QByteArray ply="ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n0 0 0\n100 0 0\n0 100 0\n3 0 1 2\n";
 writeFile(folder+"/plate.ply",ply);writeFile(folder+"/little.ply",plyBinary(QDataStream::LittleEndian));writeFile(folder+"/big.ply",plyBinary(QDataStream::BigEndian));
 std::shared_ptr<domain::WorkpieceAsset>a;QString error;
 const QStringList meshFiles=QStringList()<<"plate.stl"<<"plate.obj"<<"plate.ply"<<"little.ply"<<"big.ply";
 for(const QString&name:meshFiles){
  bool ok=load(folder+"/"+name,a,error);check(ok,qPrintable(name));if(!ok)std::cerr<<error.toUtf8().constData()<<std::endl;
  if(ok){check(domain::workpieceTriangleCount(*a)>0,"faces");check(!a->sourceBytes.empty(),"snapshot");check(!a->unitKnown,"mesh unit unknown");domain::WorkpieceNormalizeOptions o;o.unitToMm=1;auto normalized=domain::normalizeWorkpieceAsset(*a,o);check(normalized.ok,"normalization");}
 }
 check(load(folder+"/plate.obj",a,error),"obj repeated");
 if(a){check(!a->dependencies.empty(),"material snapshot");check(domain::workpieceTriangleCount(*a)==2,"quad triangulation");}
 const QByteArray points="ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\nend_header\n1 2 3\n";
 writeFile(folder+"/points.ply",points);auto before=a;check(!load(folder+"/points.ply",a,error),"point cloud rejected");check(a==before,"failure preserves result");
 infrastructure::WorkpieceLoadOptions limited;limited.maxTriangles=1;check(!load(folder+"/plate.obj",a,error,limited),"triangle limit");
 infrastructure::WorkpieceLoadOptions cancelDuringRead;cancelDuringRead.cancellation=std::make_shared<std::atomic_bool>(false);auto meshToken=cancelDuringRead.cancellation;
 cancelDuringRead.progress=[meshToken](const QString&stage,int){if(stage==QString::fromUtf8("解析网格数据"))meshToken->store(true);};auto meshPrior=a;
 check(!load(folder+"/plate.obj",a,error,cancelDuringRead),"mesh cancellation from C IO callback");check(a==meshPrior,"mesh callback cancellation preserves result");
 limited=infrastructure::WorkpieceLoadOptions();limited.maxFileBytes=8;check(!load(folder+"/plate.obj",a,error,limited),"file limit");
 limited=infrastructure::WorkpieceLoadOptions();limited.cancellation=std::make_shared<std::atomic_bool>(true);check(!load(folder+"/plate.stl",a,error,limited),"cancelled");
 QByteArray encrypted;encrypted.append(char(0x88));encrypted.append(char(0x7d));encrypted.append(char(0x1c));encrypted.append(128,'x');writeFile(folder+"/encrypted.stl",encrypted);check(!load(folder+"/encrypted.stl",a,error),"encrypted file rejected");check(error.contains(QString::fromUtf8("加密")),"encrypted diagnosis");
 writeFile(folder+"/empty.obj","");check(!load(folder+"/empty.obj",a,error),"empty rejected");
 writeFile(folder+"/invalid.obj","v nan 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");check(!load(folder+"/invalid.obj",a,error),"nonfinite rejected");
 // Parallel previews have independent results; a waiter can cancel without
 // interrupting the active C-API session or borrowing its global error text.
 std::promise<void> activeStarted,releaseActive,waiterStarted;auto activeReady=activeStarted.get_future();auto waiterReady=waiterStarted.get_future();auto gate=releaseActive.get_future().share();
 auto activeNotified=std::make_shared<std::atomic_bool>(false);auto waiterNotified=std::make_shared<std::atomic_bool>(false);
 infrastructure::WorkpieceLoadOptions activeOptions;activeOptions.progress=[&](const QString&stage,int){if(stage==QString::fromUtf8("解析网格数据")&&!activeNotified->exchange(true)){activeStarted.set_value();gate.wait();}};
 auto active=std::async(std::launch::async,[&]{ConcurrentLoad r;r.asset=a;r.ok=infrastructure::loadWorkpieceFile(folder+"/plate.obj",activeOptions,r.asset,r.error);return r;});
 check(activeReady.wait_for(std::chrono::seconds(5))==std::future_status::ready,"active parallel mesh enters C IO");
 infrastructure::WorkpieceLoadOptions waiterOptions;waiterOptions.cancellation=std::make_shared<std::atomic_bool>(false);waiterOptions.progress=[&](const QString&stage,int){if(stage==QString::fromUtf8("解析模型")&&!waiterNotified->exchange(true))waiterStarted.set_value();};
 auto waiterPrior=a;auto waiter=std::async(std::launch::async,[&]{ConcurrentLoad r;r.asset=waiterPrior;r.ok=infrastructure::loadWorkpieceFile(folder+"/plate.ply",waiterOptions,r.asset,r.error);return r;});
 check(waiterReady.wait_for(std::chrono::seconds(5))==std::future_status::ready,"parallel waiter starts");waiterOptions.cancellation->store(true);releaseActive.set_value();
 auto activeResult=active.get(),waiterResult=waiter.get();check(activeResult.ok&&domain::workpieceTriangleCount(*activeResult.asset)==2,"active mesh unaffected by waiter cancellation");check(!waiterResult.ok&&waiterResult.asset==waiterPrior,"waiting mesh cancellation preserves its result");
 writeFile(folder+"/badparse.obj","v 0 0 0\nf 999 999 999\n");
 auto good=std::async(std::launch::async,[&]{ConcurrentLoad r;r.ok=infrastructure::loadWorkpieceFile(folder+"/plate.stl",infrastructure::WorkpieceLoadOptions(),r.asset,r.error);return r;});
 auto bad=std::async(std::launch::async,[&]{ConcurrentLoad r;r.asset=a;r.ok=infrastructure::loadWorkpieceFile(folder+"/badparse.obj",infrastructure::WorkpieceLoadOptions(),r.asset,r.error);return r;});
 auto goodResult=good.get(),badResult=bad.get();check(goodResult.ok&&goodResult.error.isEmpty(),"parallel valid mesh has no borrowed error");check(!badResult.ok&&!badResult.error.isEmpty()&&badResult.asset==a,"parallel parse failure isolated");
 // Source recovery is entirely memory-backed, including paths outside the
 // original model directory. No original model/material file is required.
 domain::WorkpieceAsset embedded;embedded.name="embedded";embedded.format="OBJ";
 embedded.sourcePath=(temp.path()+"/uncreated/model.obj").toUtf8().toStdString();
 QByteArray embeddedObj=obj;embeddedObj.replace("colors.mtl","../materials/colors.mtl");embedded.sourceBytes.assign(embeddedObj.begin(),embeddedObj.end());
 domain::WorkpieceSourceFile material;material.relativePath="../materials/colors.mtl";const QByteArray materialBytes="newmtl blue\nKd 0.1 0.2 0.8\n";material.bytes.assign(materialBytes.begin(),materialBytes.end());material.sha256=QCryptographicHash::hash(materialBytes,QCryptographicHash::Sha256).toHex().toStdString();embedded.dependencies.push_back(material);
 embedded.linearDeflectionMm=.25;embedded.angularDeflectionDeg=20;
 infrastructure::WorkpieceLoadOptions recovery;recovery.linearDeflectionMm=.75;
 check(infrastructure::rebuildWorkpieceAsset(embedded,recovery,a,error),"memory OBJ source recovery with parent dependency");
 if(a){check(domain::workpieceTriangleCount(*a)==2,"recovered source geometry");check(a->linearDeflectionMm==.25&&a->angularDeflectionDeg==20,"recovery preserves saved CAD parameters");check(QString::fromStdString(a->adapterVersion)==infrastructure::workpieceAdapterVersion("OBJ"),"recovery adapter identity");}
 auto recoveryBefore=a;embedded.dependencies.front().bytes.push_back('x');
 check(!infrastructure::rebuildWorkpieceAsset(embedded,recovery,a,error),"dependency snapshot SHA mismatch rejected");check(a==recoveryBefore,"snapshot failure preserves result");embedded.dependencies.front()=material;
 embedded.dependencies.clear();check(infrastructure::rebuildWorkpieceAsset(embedded,recovery,a,error),"missing nongeometric material can downgrade");if(a)check(!a->warnings.empty(),"missing embedded material warning");
#ifdef ROBOT3D_WITH_OCCT
 const QString cad=temp.path()+"/cad";QDir().mkpath(cad);
 if(traceEnabled())std::cerr<<"CAD document create"<<std::endl;
 Handle(TDocStd_Document)doc=new TDocStd_Document(TCollection_ExtendedString("BinXCAF"));
 if(traceEnabled())std::cerr<<"CAD document ready"<<std::endl;
 if(traceEnabled())std::cerr<<"CAD ShapeTool create"<<std::endl;
 auto shapes=XCAFDoc_DocumentTool::ShapeTool(doc->Main());if(traceEnabled())std::cerr<<"CAD ColorTool create"<<std::endl;auto colors=XCAFDoc_DocumentTool::ColorTool(doc->Main());
 if(traceEnabled())std::cerr<<"CAD tools ready"<<std::endl;
 if(traceEnabled())std::cerr<<"CAD box create"<<std::endl;
 auto box=BRepPrimAPI_MakeBox(100,50,25).Shape();if(traceEnabled())std::cerr<<"CAD shape add"<<std::endl;auto part=shapes->AddShape(box,Standard_False);if(traceEnabled())std::cerr<<"CAD name set"<<std::endl;TDataStd_Name::Set(part,TCollection_ExtendedString("Box"));
 if(traceEnabled())std::cerr<<"CAD fixture ready"<<std::endl;
 colors->SetColor(part,Quantity_Color(0.1,0.6,0.3,Quantity_TOC_RGB),XCAFDoc_ColorGen);
 for(const char*schema:{"AP203","AP214IS","AP242DIS"}){
  Interface_Static::SetCVal("write.step.schema",schema);Interface_Static::SetCVal("write.step.unit","MM");
  if(traceEnabled())std::cerr<<"CAD writer create "<<schema<<std::endl;STEPCAFControl_Writer writer;if(traceEnabled())std::cerr<<"CAD writer ready"<<std::endl;writer.SetColorMode(Standard_True);writer.SetNameMode(Standard_True);
  const QString p=cad+"/"+schema+".stp";bool ok=writer.Transfer(doc)&&writer.Write(p.toUtf8().constData())==IFSelect_RetDone;check(ok,"CAD fixture write");
  ok=load(p,a,error);check(ok,schema);if(!ok)std::cerr<<error.toUtf8().constData()<<std::endl;
  if(ok){check(a->unitKnown&&a->sourceUnitToMm==1,"CAD transfer mm");domain::WorkpieceNormalizeOptions n;auto r=domain::normalizeWorkpieceAsset(*a,n);check(r.ok,"CAD normalize");if(r.ok)check(std::abs(r.value->boundsMax.x-r.value->boundsMin.x-100)<0.01,"CAD 100mm");}
 }
 // A header filename and a product name do not declare external geometry.
 TDataStd_Name::Set(part,TCollection_ExtendedString("manufacturing-source.step"));
 STEPCAFControl_Writer renamedWriter;const QString original=cad+"/original.stp";
 check(renamedWriter.Transfer(doc)&&renamedWriter.Write(original.toUtf8().constData())==IFSelect_RetDone,"renamed fixture write");
 QFile originalInput(original);check(originalInput.open(QIODevice::ReadOnly),"renamed fixture open");QString renamedText=QString::fromLatin1(originalInput.readAll());originalInput.close();
 renamedText.replace(QRegularExpression("FILE_NAME\\s*\\(\\s*'[^']*'"),"FILE_NAME('C:/unavailable-robot3d-fixture/original.stp'");
 const QString renamed=cad+"/moved-renamed.stp";check(writeFile(renamed,renamedText.toLatin1()),"renamed fixture copy");check(QFile::remove(original),"remove original fixture");
 bool renamedOk=load(renamed,a,error);check(renamedOk,"STEP header and product filenames are metadata");if(!renamedOk)std::cerr<<error.toUtf8().constData()<<std::endl;
 if(renamedOk)check(a->dependencies.empty(),"metadata does not create dependencies");
 TDataStd_Name::Set(part,TCollection_ExtendedString("Box"));
 // Reused definitions and nested locations must accumulate exactly once.
 Handle(TDocStd_Document)assemblyDoc=new TDocStd_Document(TCollection_ExtendedString("BinXCAF"));
 auto assemblyShapes=XCAFDoc_DocumentTool::ShapeTool(assemblyDoc->Main());auto assemblyColors=XCAFDoc_DocumentTool::ColorTool(assemblyDoc->Main());
 auto definition=assemblyShapes->AddShape(BRepPrimAPI_MakeBox(10,20,30).Shape(),Standard_False);
 TDataStd_Name::Set(definition,TCollection_ExtendedString("SharedPart"));assemblyColors->SetColor(definition,Quantity_Color(0,1,0,Quantity_TOC_RGB),XCAFDoc_ColorGen);
 auto inner=assemblyShapes->NewShape();auto rootAssembly=assemblyShapes->NewShape();TDataStd_Name::Set(inner,TCollection_ExtendedString("Nested"));TDataStd_Name::Set(rootAssembly,TCollection_ExtendedString("Assembly"));
 gp_Trsf local;local.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,0,1)),3.14159265358979323846/2);local.SetTranslationPart(gp_Vec(10,20,30));
 auto first=assemblyShapes->AddComponent(inner,definition,TopLoc_Location(local));TDataStd_Name::Set(first,TCollection_ExtendedString("First"));assemblyColors->SetColor(first,Quantity_Color(1,0,0,Quantity_TOC_RGB),XCAFDoc_ColorGen);
 gp_Trsf outer;outer.SetTranslation(gp_Vec(100,200,300));assemblyShapes->AddComponent(rootAssembly,inner,TopLoc_Location(outer));
 gp_Trsf other;other.SetTranslation(gp_Vec(-100,-200,-300));auto second=assemblyShapes->AddComponent(rootAssembly,definition,TopLoc_Location(other));TDataStd_Name::Set(second,TCollection_ExtendedString("Second"));
 assemblyShapes->UpdateAssemblies();
 STEPCAFControl_Writer assemblyWriter;const QString assemblyFile=cad+"/nested.stp";
 check(assemblyWriter.Transfer(assemblyDoc)&&assemblyWriter.Write(assemblyFile.toUtf8().constData())==IFSelect_RetDone,"nested STEP fixture write");
 bool assemblyOk=load(assemblyFile,a,error);check(assemblyOk,"nested STEP read");if(!assemblyOk)std::cerr<<error.toUtf8().constData()<<std::endl;
 if(assemblyOk){
  check(a->components.size()>=4,"nested hierarchy retained");check(domain::workpieceTriangleCount(*a)==24,"both reused occurrences retained");
  domain::WorkpieceNormalizeOptions n;auto normalized=domain::normalizeWorkpieceAsset(*a,n);check(normalized.ok,"nested STEP normalization");
  if(normalized.ok){
   auto lo=normalized.value->boundsMin,hi=normalized.value->boundsMax;
   check(std::abs(lo.x+100)<.01&&std::abs(lo.y+200)<.01&&std::abs(lo.z+300)<.01&&std::abs(hi.x-110)<.01&&std::abs(hi.y-230)<.01&&std::abs(hi.z-360)<.01,"nested STEP transforms exactly once");
   bool red=false,green=false;for(const auto&c:normalized.value->components)for(const auto&m:c.meshes)for(const auto&v:m.vertices){red=red||(v.color[0]>.9&&v.color[1]<.1);green=green||(v.color[1]>.9&&v.color[0]<.1);}
   check(red&&green,"STEP instance and definition colors retained");
  }
 }
 // Multifile STEP must retain actual geometry snapshots and reject missing data.
 STEPCAFControl_Writer multi;const QString multiPath=cad+"/assembly.stp";
 check(multi.Transfer(assemblyDoc,STEPControl_AsIs,"part_")&&multi.Write(multiPath.toUtf8().constData())==IFSelect_RetDone,"external STEP fixture write");
 bool multiOk=load(multiPath,a,error);check(multiOk,"external STEP read");if(!multiOk)std::cerr<<error.toUtf8().constData()<<std::endl;
 if(multiOk){
  check(!a->dependencies.empty(),"actual STEP dependencies captured");
  if(!a->dependencies.empty()){
   const QString dependency=QDir(cad).absoluteFilePath(QString::fromUtf8(a->dependencies.front().relativePath.c_str()));
   QFile dependencyInput(dependency);check(dependencyInput.open(QIODevice::ReadOnly),"external STEP fixture read");const auto dependencyBytes=dependencyInput.readAll();dependencyInput.close();
   check(QFile::remove(dependency),"external STEP fixture remove");auto prior=a;
   check(!load(multiPath,a,error),"missing external STEP rejected");check(a==prior,"missing external STEP preserves result");
   check(writeFile(dependency,"invalid CAD"),"invalid external fixture write");check(!load(multiPath,a,error),"invalid external STEP rejected");check(a==prior,"invalid external STEP preserves result");
   check(writeFile(dependency,dependencyBytes),"external STEP fixture restore");
   const auto embeddedCad=*prior;
   check(QFile::remove(multiPath),"remove original STEP assembly for memory recovery");
   for(const auto&dep:embeddedCad.dependencies)check(QFile::remove(QDir(cad).absoluteFilePath(QString::fromUtf8(dep.relativePath.c_str()))),"remove original external STEP for memory recovery");
   bool recovered=infrastructure::rebuildWorkpieceAsset(embeddedCad,infrastructure::WorkpieceLoadOptions(),a,error);check(recovered,"external STEP recovery from embedded streams");if(!recovered)std::cerr<<error.toUtf8().constData()<<std::endl;
   if(recovered){check(domain::workpieceTriangleCount(*a)==24,"recovered external STEP complete geometry");check(a->components.size()==embeddedCad.components.size(),"recovered STEP stable component count");for(std::size_t i=0;i<a->components.size()&&i<embeddedCad.components.size();++i)check(a->components[i].id==embeddedCad.components[i].id,"recovered STEP stable component IDs");}
  }
 }
 Interface_Static::SetCVal("write.step.unit","M");STEPCAFControl_Writer meters;const QString meterFile=cad+"/meter.stp";check(meters.Transfer(doc)&&meters.Write(meterFile.toUtf8().constData())==IFSelect_RetDone,"meter write");check(load(meterFile,a,error),"meter read");
 if(a){domain::WorkpieceNormalizeOptions n;auto r=domain::normalizeWorkpieceAsset(*a,n);check(r.ok&&std::abs(r.value->boundsMax.x-r.value->boundsMin.x-100)<0.01,"meter converted once");}
 Interface_Static::SetCVal("write.step.unit","MM");
 IGESCAFControl_Writer iges;const QString ip=cad+"/box.igs";check(iges.Transfer(doc)&&iges.Write(ip.toUtf8().constData()),"IGES fixture");bool igesOk=load(ip,a,error);check(igesOk,"IGES read");if(!igesOk)std::cerr<<error.toUtf8().constData()<<std::endl;
 if(igesOk){
  const auto embeddedIges=*a;check(QFile::remove(ip),"remove original IGES for memory recovery");
  check(infrastructure::rebuildWorkpieceAsset(embeddedIges,infrastructure::WorkpieceLoadOptions(),a,error),"IGES recovery from embedded source");
  if(a){domain::WorkpieceNormalizeOptions n;auto result=domain::normalizeWorkpieceAsset(*a,n);check(result.ok&&std::abs(result.value->boundsMax.x-result.value->boundsMin.x-100)<.01,"IGES recovery physical units");}
  infrastructure::WorkpieceLoadOptions cancelled;cancelled.cancellation=std::make_shared<std::atomic_bool>(false);auto token=cancelled.cancellation;
  cancelled.progress=[token](const QString&stage,int){if(stage.contains("IGES"))token->store(true);};auto prior=a;
  check(!infrastructure::rebuildWorkpieceAsset(embeddedIges,cancelled,a,error),"IGES memory read cancellation");check(a==prior,"IGES cancellation preserves result");
 }
#endif
 std::cout<<"Workpiece loader: "<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;
}
