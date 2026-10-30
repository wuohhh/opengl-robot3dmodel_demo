#include "robot3d/application/WorkpieceService.h"
#include "robot3d/infrastructure/WorkpieceSceneStore.h"
#include "robot3d/infrastructure/WorkpieceFileLoader.h"
#include <QDataStream>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <limits>
using namespace robot3d;
using namespace robot3d::domain;
namespace {
int checks=0;
void require(bool ok,const char*message) {++checks;if(!ok)throw std::runtime_error(message);}
bool near(double a,double b,double e=1e-8) {return std::fabs(a-b)<=e;}
WorkpieceAsset triangle(double size=100) {
    WorkpieceAsset a;a.id="reference-source";a.name="Part";a.format="TEST";a.sourcePath="reference.stl";a.sourceBytes={1,2,3,4};
    a.sourceUnit="mm";a.sourceUnitToMm=1;a.unitKnown=true;a.adapterVersion="test";
    WorkpieceComponent c;c.id="component";c.name="Surface";WorkpieceMesh m;
    WorkpieceVertex p;p.position=Vec3d(0,0,0);p.normal=Vec3d(0,0,1);m.vertices.push_back(p);
    p.position=Vec3d(size,0,0);m.vertices.push_back(p);p.position=Vec3d(0,size,0);m.vertices.push_back(p);
    m.indices={0,1,2};m.triangleFaceIds={37};c.meshes.push_back(m);a.components.push_back(c);return a;
}
WorkpieceAssetPtr normalizedAsset() {
    auto a=normalizeWorkpieceAsset(triangle(),WorkpieceNormalizeOptions());require(a.ok,"normalize baseline");return a.value;
}
RobotStateSnapshot robot() {
    RobotStateSnapshot r;std::shared_ptr<RobotConfiguration>c(new RobotConfiguration);c->id="test-robot";r.configuration=c;
    r.baseInWorld={10,20,30,0,0,0};r.coordinates.push_back({"p_0","User",{100,0,0,0,0,0}});return r;
}
void mathAndNormalization() {
    const EulerPoseDeg p={100,200,300,30,45,60};auto m=poseMatrix(p);
    auto q=transformPoint(m,Vec3d(1,2,3));
    // Independent golden values for Rz(60)*Ry(45)*Rx(30).
    require(near(q.x,101.4247035404069)&&near(q.y,202.9317605328458)&&near(q.z,301.8371173070874),"ZYX transform golden");
    auto identity=multiply(rigidInverse(m),m);
    for(int row=0;row<4;++row)for(int col=0;col<4;++col)require(near(identity.values[row*4+col],row==col?1:0),"rigid inverse identity");
    const auto roundtrip=poseMatrix(matrixPose(m));for(int n=0;n<16;++n)require(near(roundtrip.values[n],m.values[n]),"pose matrix roundtrip");
    auto a=triangle();auto first=normalizeWorkpieceAsset(a,WorkpieceNormalizeOptions());require(first.ok,"millimeter model");
    a=triangle(0.1);a.sourceUnit="m";a.sourceUnitToMm=1000;
    auto meters=normalizeWorkpieceAsset(a,WorkpieceNormalizeOptions());require(meters.ok&&near(meters.value->boundsMax.x,100),"meter normalization");
    require(near(first.value->boundsMax.x,meters.value->boundsMax.x),"same physical dimensions");
    a=triangle();a.unitKnown=false;require(!normalizeWorkpieceAsset(a,WorkpieceNormalizeOptions()).ok,"unknown unit blocks import");
    WorkpieceNormalizeOptions options;options.unitToMm=10;
    auto explicitUnit=normalizeWorkpieceAsset(a,options);require(explicitUnit.ok&&near(explicitUnit.value->boundsMax.x,1000),"explicit source unit");
    options.unitToMm=std::numeric_limits<double>::infinity();require(!normalizeWorkpieceAsset(a,options).ok,"infinite scale rejected");
    options=WorkpieceNormalizeOptions();options.origin=WorkpieceOrigin::Center;
    auto center=normalizeWorkpieceAsset(triangle(),options);require(center.ok&&near(center.value->boundsMin.x,-50)&&near(center.value->boundsMax.y,50),"center origin");
    options.origin=WorkpieceOrigin::BottomCenter;auto bottom=normalizeWorkpieceAsset(triangle(),options);require(bottom.ok&&near(bottom.value->boundsMin.z,0),"bottom center origin");
    options=WorkpieceNormalizeOptions();options.upAxis=1;options.forwardAxis=1;require(!normalizeWorkpieceAsset(triangle(),options).ok,"parallel axes rejected");
    options=WorkpieceNormalizeOptions();options.leftHanded=true;auto mirrored=normalizeWorkpieceAsset(triangle(),options);
    require(mirrored.ok&&mirrored.value->components[0].meshes[0].indices[1]==2,"reflection repairs triangle winding");
    a=triangle();a.components[0].transform=poseMatrix({20,30,40,0,0,90});
    auto node=normalizeWorkpieceAsset(a,WorkpieceNormalizeOptions());require(node.ok&&near(node.value->boundsMin.x,-80)&&near(node.value->boundsMax.y,130),"source node cumulative transform");
    a=triangle();auto second=a.components[0];second.id="second";second.transform=poseMatrix({200,0,0,0,0,0});a.components.push_back(second);
    options=WorkpieceNormalizeOptions();options.selectAllComponents=false;options.selectedComponentIds={"second"};
    auto selected=normalizeWorkpieceAsset(a,options);require(selected.ok&&near(selected.value->boundsMin.x,200)&&workpieceTriangleCount(*selected.value)==1,"component subset normalization");
    options.selectedComponentIds.clear();require(!normalizeWorkpieceAsset(a,options).ok,"empty component selection rejected");
    a=triangle();a.components[0].meshes[0].indices[2]=99;require(!normalizeWorkpieceAsset(a,WorkpieceNormalizeOptions()).ok,"bad index rejected");
    a=triangle();a.components[0].meshes[0].vertices[0].position.x=std::numeric_limits<double>::quiet_NaN();require(!normalizeWorkpieceAsset(a,WorkpieceNormalizeOptions()).ok,"NaN rejected");
    a=triangle();a.components[0].meshes[0].indices={0,0,0};require(!normalizeWorkpieceAsset(a,WorkpieceNormalizeOptions()).ok,"all degenerate triangles rejected");
    a=triangle();a.components[0].meshes[0].indices={0,0,0,0,1,2};a.components[0].meshes[0].triangleFaceIds={0,37};
    auto partial=normalizeWorkpieceAsset(a,WorkpieceNormalizeOptions());require(partial.ok&&workpieceTriangleCount(*partial.value)==1&&!partial.value->warnings.empty(),"degenerate triangle skipped with warning");
    require(partial.value->components[0].meshes[0].triangleFaceIds[0]==37,"surface mapping survives skipped face");
    options=WorkpieceNormalizeOptions();options.cancellation=std::make_shared<std::atomic_bool>(true);
    require(!normalizeWorkpieceAsset(triangle(),options).ok,"cancelled normalization cannot produce a candidate");
}
quint64 little64(const QByteArray&data,int offset) {
    quint64 value=0;for(int i=0;i<8;++i)value|=quint64(static_cast<unsigned char>(data[offset+i]))<<(i*8);return value;
}
void putLittle64(QByteArray&data,int offset,quint64 value) {
    for(int i=0;i<8;++i)data[offset+i]=char((value>>(i*8))&255);
}
void repairPackageChecksum(QByteArray&data) {
    putLittle64(data,12,quint64(data.size()-52));
    data.replace(20,32,QCryptographicHash::hash(data.mid(52),QCryptographicHash::Sha256));
}
void writePackageFixture(const QString&path,const QByteArray&data) {
    QFile f(path);require(f.open(QIODevice::WriteOnly)&&f.write(data)==data.size(),"write package fixture");
}
QByteArray readPackageFixture(const QString&path) {
    QFile f(path);require(f.open(QIODevice::ReadOnly),"read package fixture");return f.readAll();
}
void cacheRecoveryPersistence() {
    QTemporaryDir temp;require(temp.isValid(),"cache recovery directory");
    const auto file=QDir(temp.path()).filePath("approved.ply"),package=QDir(temp.path()).filePath("recover.r3wp");
    const QByteArray source="ply\nformat ascii 1.0\nelement vertex 4\nproperty float x\nproperty float y\nproperty float z\nelement face 4\nproperty list uchar int vertex_indices\nend_header\n10 20 30\n110 20 30\n10 220 30\n10 20 330\n3 0 2 1\n3 0 1 3\n3 0 3 2\n3 1 2 3\n";
    writePackageFixture(file,source);QString error;std::shared_ptr<WorkpieceAsset> raw;
    require(infrastructure::loadWorkpieceFile(file,infrastructure::WorkpieceLoadOptions(),raw,error),qPrintable(error));
    WorkpieceNormalizeOptions options;options.unitToMm=10;options.upAxis=2;options.forwardAxis=-1;options.leftHanded=true;options.origin=WorkpieceOrigin::BottomCenter;options.selectAllComponents=false;
    for(const auto&c:raw->components)if(!c.meshes.empty())options.selectedComponentIds.push_back(c.id);
    auto approved=normalizeWorkpieceAsset(*raw,options);require(approved.ok,"approve nondefault physical normalization");
    application::WorkpieceService service;require(service.add(approved.value,"Approved","world",{125,25,35,12,23,34},robot()).ok,"approved instance");
    require(infrastructure::saveWorkpieceScene(package,service.snapshot(),error),qPrintable(error));const auto original=readPackageFixture(package);
    require(QFile::remove(file),"remove original source before recovery");
    const int marker=original.indexOf("R3CACHE3");require(marker>=52&&marker+48<original.size(),"locate independent mesh cache");
    const auto cacheSize=little64(original,marker+8);require(cacheSize>=4&&cacheSize<=quint64(original.size()-marker-48),"cache extent");
    const int begin=marker+48;WorkpieceSceneState restored;
    auto verify=[&] {
        require(restored.instances.size()==1,"cache recovery instance count");const auto&actual=*restored.instances.front().asset;
        require(actual.id==approved.value->id&&actual.normalization.values==approved.value->normalization.values,"cache recovery preserves stable ID and exact approved matrix");
        require(actual.sourceUnitToMm==10&&actual.linearDeflectionMm==approved.value->linearDeflectionMm&&actual.angularDeflectionDeg==approved.value->angularDeflectionDeg,"cache recovery preserves confirmed unit and mesh quality");
        require(actual.components.size()==approved.value->components.size(),"cache recovery preserves component manifest");
        for(std::size_t c=0;c<actual.components.size();++c) {
            const auto&a=actual.components[c];const auto&b=approved.value->components[c];
            require(a.id==b.id&&a.selected==b.selected&&a.meshes.size()==b.meshes.size(),"cache recovery preserves component selection");
            for(std::size_t m=0;m<a.meshes.size();++m) {
                require(a.meshes[m].indices==b.meshes[m].indices&&a.meshes[m].vertices.size()==b.meshes[m].vertices.size(),"cache recovery preserves reflected triangle topology");
                for(std::size_t n=0;n<a.meshes[m].vertices.size();++n) {
                    const auto&p=a.meshes[m].vertices[n].position;const auto&q=b.meshes[m].vertices[n].position;
                    require(near(p.x,q.x)&&near(p.y,q.y)&&near(p.z,q.z),"cache recovery preserves approved position and local origin");
                }
            }
        }
        require(restored.instances.front().pose.X==125&&restored.instances.front().pose.Rz==34,"cache recovery preserves instance placement");
        bool explained=false;for(const auto&w:actual.warnings)if(QString::fromUtf8(w.c_str()).contains(QString::fromUtf8("缓存")))explained=true;
        require(explained,"cache recovery explains reason");
    };
    auto invalidChecksum=original;invalidChecksum[begin]=char(invalidChecksum[begin]^0x40);repairPackageChecksum(invalidChecksum);writePackageFixture(package,invalidChecksum);
    require(infrastructure::loadWorkpieceScene(package,restored,error),qPrintable(error));verify();
    auto invalidStructure=original;invalidStructure[begin]=char(0xa1);invalidStructure[begin+1]=char(0x86);invalidStructure[begin+2]=char(0x01);invalidStructure[begin+3]=char(0);
    invalidStructure.replace(marker+16,32,QCryptographicHash::hash(invalidStructure.mid(begin,int(cacheSize)),QCryptographicHash::Sha256));repairPackageChecksum(invalidStructure);writePackageFixture(package,invalidStructure);
    require(infrastructure::loadWorkpieceScene(package,restored,error),"invalid cache structure rebuilds from source");verify();
    auto outdated=original;const auto version=approved.value->adapterVersion;
    const int adapter=outdated.indexOf(version.c_str());require(adapter>=52&&!version.empty(),"locate adapter version");outdated[adapter+int(version.size())-1]='0';repairPackageChecksum(outdated);writePackageFixture(package,outdated);
    require(infrastructure::loadWorkpieceScene(package,restored,error),"outdated adapter rebuilds from source");verify();
    const auto retained=restored.instances.front().asset;auto damagedSource=original;const auto sourceAt=damagedSource.indexOf(source);
    require(sourceAt>=52,"locate source snapshot");damagedSource[sourceAt]='X';repairPackageChecksum(damagedSource);writePackageFixture(package,damagedSource);
    require(!infrastructure::loadWorkpieceScene(package,restored,error)&&restored.instances.front().asset==retained,"source checksum failure cannot fall back to cache or replace scene");
}
void externalDependencyPersistence() {
    auto raw=triangle();raw.format="TEST";raw.sourcePath="C:/source/models/part.obj";
    raw.sourceBytes={'m','t','l','l','i','b',' ','a'};
    for(const std::string&reference:std::vector<std::string>{"../materials/part.mtl","C:/shared/materials/other.mtl","../../textures/../coat.png"}) {
        WorkpieceSourceFile source;source.relativePath=reference;source.bytes={1,3,5,7,9};raw.dependencies.push_back(source);
    }
    auto normalized=normalizeWorkpieceAsset(raw,WorkpieceNormalizeOptions());require(normalized.ok,"normalize external resource references");
    application::WorkpieceService service;require(service.add(normalized.value,"External materials","world",{},robot()).ok,"add external material asset");
    QTemporaryDir temp;require(temp.isValid(),"external resource temporary directory");QString error;
    const auto original=QDir(temp.path()).filePath("source.r3wp"),moved=QDir(temp.path()).filePath("portable.r3wp");
    require(infrastructure::saveWorkpieceScene(original,service.snapshot(),error),qPrintable(error));
    require(QFile::rename(original,moved),"external resource package move");
    WorkpieceSceneState restored;require(infrastructure::loadWorkpieceScene(moved,restored,error),qPrintable(error));
    const auto&dependencies=restored.instances.front().asset->dependencies;
    require(dependencies.size()==raw.dependencies.size(),"all external resource snapshots restored");
    for(std::size_t n=0;n<dependencies.size();++n) {
        require(dependencies[n].relativePath==raw.dependencies[n].relativePath,"original external reference preserved as metadata");
        require(dependencies[n].bytes==raw.dependencies[n].bytes&&!dependencies[n].sha256.empty(),"external resource bytes and digest preserved");
    }
    require(!QFile::exists(QDir(temp.path()).filePath("materials/part.mtl")),"package load does not extract resources");
    require(infrastructure::saveWorkpieceScene(original,restored,error),"resave external resource package");
    require(infrastructure::loadWorkpieceScene(original,restored,error),"reload external resource package");
    QFile malformed(original);require(malformed.open(QIODevice::ReadWrite),"open invalid resource key fixture");
    auto packageBytes=malformed.readAll();const auto keyOffset=packageBytes.indexOf("resources/0-");
    require(keyOffset>=52,"locate safe resource key");packageBytes.replace(keyOffset,3,"../");
    const auto correctedDigest=QCryptographicHash::hash(packageBytes.mid(52),QCryptographicHash::Sha256);
    packageBytes.replace(20,32,correctedDigest);require(malformed.seek(0)&&malformed.write(packageBytes)==packageBytes.size(),"write invalid resource key with valid package checksum");malformed.close();
    const auto retained=restored.instances.front().asset;
    require(!infrastructure::loadWorkpieceScene(original,restored,error)&&restored.instances.front().asset==retained,"unsafe resource key cannot replace scene even with valid checksum");

    // Convert the no-dependency v3 fixture back to its v1 inline-cache layout
    // and recompute the container checksum to exercise real backward reading.
    application::WorkpieceService legacy;require(legacy.add(normalizedAsset(),"Legacy","world",{},robot()).ok,"legacy asset");
    require(infrastructure::saveWorkpieceScene(original,legacy.snapshot(),error),"write legacy fixture");
    auto legacyBytes=readPackageFixture(original);const int marker=legacyBytes.indexOf("R3CACHE3");require(marker>=52,"locate legacy cache");
    QByteArray manifest;QDataStream stream(&manifest,QIODevice::WriteOnly);stream.setVersion(QDataStream::Qt_5_6);stream.setByteOrder(QDataStream::LittleEndian);stream<<quint32(1);
    for(const auto&s:std::vector<std::string>{"component","Surface",""}) {stream<<quint32(s.size());if(!s.empty())stream.writeRawData(s.data(),int(s.size()));}stream<<true;
    require(legacyBytes.mid(marker-manifest.size(),manifest.size())==manifest,"locate independent legacy manifest");
    legacyBytes.remove(marker-manifest.size(),manifest.size()+48);legacyBytes[8]=char(1);repairPackageChecksum(legacyBytes);writePackageFixture(original,legacyBytes);
    require(infrastructure::loadWorkpieceScene(original,restored,error),"version one scene remains readable");
    require(restored.instances.front().asset->dependencies.empty(),"legacy dependency layout preserved");
    std::shared_ptr<WorkpieceAsset>cacheOnly(new WorkpieceAsset(*normalizedAsset()));cacheOnly->sourceBytes.clear();
    application::WorkpieceService cached;require(cached.add(cacheOnly,"Cache only","world",{},robot()).ok,"cache-only fixture");
    require(infrastructure::saveWorkpieceScene(original,cached.snapshot(),error),"save explicitly limited cache-only package");
    require(infrastructure::loadWorkpieceScene(original,restored,error),"read valid cache-only display data");
    bool limited=false;for(const auto&w:restored.instances.front().asset->warnings)
        if(QString::fromUtf8(w.c_str()).contains(QString::fromUtf8("仅包含显示缓存")))limited=true;
    require(limited,"cache-only recovery limitation is explicit");
    auto unavailable=readPackageFixture(original);const int cacheAt=unavailable.indexOf("R3CACHE3");require(cacheAt>=52,"locate cache-only cache");
    unavailable[cacheAt+48]=char(unavailable[cacheAt+48]^0x40);repairPackageChecksum(unavailable);writePackageFixture(original,unavailable);
    const auto cacheOnlyPrior=restored.instances.front().asset;
    require(!infrastructure::loadWorkpieceScene(original,restored,error)&&restored.instances.front().asset==cacheOnlyPrior,"invalid cache without source preserves current scene");
}
void serviceAndPersistence() {
    application::WorkpieceService service;const auto asset=normalizedAsset();auto r=robot();
    auto added=service.add(asset,"Part","b_0",{100,0,0,0,0,0},r);require(added.ok,"add workpiece");const auto id=added.value;
    require(service.dirty()&&service.snapshot().instances.size()==1,"add changes dirty state");
    auto world=workpieceWorldMatrix(*service.find(id),r);require(near(world.values[3],110)&&near(world.values[7],20),"base parent physical translation");
    service.markSaved();require(!service.dirty(),"mark saved");
    std::shared_ptr<WorkpieceAsset> sameSource(new WorkpieceAsset(*asset));
    auto duplicate=service.add(sameSource,"part","world",{},r);require(duplicate.ok&&duplicate.value!=id&&service.find(duplicate.value)->name=="part (2)","duplicate creates independently named instance");
    require(service.find(duplicate.value)->asset==service.find(id)->asset,"same asset ID is deduplicated across separate import results");
    std::shared_ptr<WorkpieceAsset> conflict(new WorkpieceAsset(*asset));conflict->components[0].meshes[0].vertices[0].position.x+=1;
    const auto originalCount=service.snapshot().instances.size();
    require(!service.add(conflict,"Conflict","world",{},r).ok&&service.snapshot().instances.size()==originalCount,"same ID with different geometry is rejected atomically");
    require(service.undo()&&service.snapshot().instances.size()==1&&!service.dirty(),"undo returns saved content version");
    require(service.redo()&&service.snapshot().instances.size()==2&&service.dirty(),"redo restores new instance");
    auto moved=*service.find(id);moved.pose.X=250;require(service.update(moved,r).ok,"move workpiece");
    require(near(service.find(duplicate.value)->pose.X,0),"moving one instance preserves other");
    moved=*service.find(id);moved.locked=true;require(service.update(moved,r).ok,"lock workpiece");
    require(!service.remove(id).ok,"locked delete rejected");
    moved=*service.find(id);moved.pose.X=300;const auto revision=service.snapshot().revision;require(!service.update(moved,r).ok&&service.snapshot().revision==revision,"locked move atomic");
    moved=*service.find(id);moved.locked=false;require(service.update(moved,r).ok,"unlock workpiece");
    moved=*service.find(id);moved.opacity=std::numeric_limits<double>::quiet_NaN();require(!service.update(moved,r).ok,"invalid display numeric rejected");
    moved=*service.find(id);moved.parentCoordinateId="p_0";moved.pose={};require(service.update(moved,r).ok,"static user parent");
    const auto beforeLoss=workpieceWorldMatrix(*service.find(id),r);service.markSaved();
    auto noParent=r;noParent.coordinates.clear();require(service.syncRobotContext(noParent),"parent loss transition");
    require(service.find(id)->parentCoordinateId=="world"&&near(workpieceWorldMatrix(*service.find(id),noParent).values[3],beforeLoss.values[3]),"lost parent freezes world placement");
    require(service.undo()&&service.find(id)->parentCoordinateId=="p_0","lost parent change undoable");
    const auto oldRevision=service.snapshot().revision;const bool oldDirty=service.dirty();
    for(int n=0;n<100;++n){r.joints.L1=n;require(!service.syncRobotContext(r),"joint simulation does not reparent static workpiece");}
    require(service.snapshot().revision==oldRevision&&service.dirty()==oldDirty,"simulation does not dirty workpiece or history");
    moved=*service.find(id);moved.parentCoordinateId="world";require(service.update(moved,r).ok,"world parent");
    for(int n=0;n<25;++n){moved=*service.find(id);moved.pose.Z=n+1;require(service.update(moved,r).ok,"history change");}
    for(int n=0;n<20;++n)require(service.undo(),"at least twenty undo steps");
    QTemporaryDir temp;require(temp.isValid(),"temporary scene directory");
    QString error;const QString path=QDir(temp.path()).filePath(QString::fromUtf8("工件场景.r3wp"));
    require(infrastructure::saveWorkpieceScene(path,service.snapshot(),error),qPrintable(error));
    WorkpieceSceneState restored;require(infrastructure::loadWorkpieceScene(path,restored,error),qPrintable(error));
    require(restored.instances.size()==2&&restored.instances[0].asset==restored.instances[1].asset,"shared asset restored once");
    require(restored.instances[0].asset->sourceBytes==asset->sourceBytes,"source snapshot is self contained");
    require(restored.instances[0].asset->components[0].meshes[0].triangleFaceIds[0]==37,"surface mapping persisted");
    auto conflictScene=service.snapshot();conflictScene.instances[1].asset=conflict;
    require(!infrastructure::saveWorkpieceScene(path,conflictScene,error),"package rejects same ID with conflicting content");
    require(infrastructure::loadWorkpieceScene(path,restored,error),"failed atomic save preserves prior valid package");
    auto invalidSelection=service.snapshot();invalidSelection.selectedId="missing-instance";
    require(!infrastructure::saveWorkpieceScene(path,invalidSelection,error),"invalid selection cannot overwrite package");
    require(infrastructure::loadWorkpieceScene(path,restored,error),"invalid selection save preserves valid package");
    auto duplicateNames=service.snapshot();duplicateNames.instances[1].name=duplicateNames.instances[0].name;
    require(!infrastructure::saveWorkpieceScene(path,duplicateNames,error),"duplicate names cannot overwrite package");
    require(infrastructure::loadWorkpieceScene(path,restored,error),"duplicate name save preserves valid package");
    const QString relocated=QDir(temp.path()).filePath("relocated.r3wp");require(QFile::rename(path,relocated),"package relocation");
    require(infrastructure::loadWorkpieceScene(relocated,restored,error),"restore after moving package independent of source");
    application::WorkpieceService opened;require(opened.replace(restored,r,true).ok&&!opened.dirty(),"atomic restore service");
    auto invalid=restored;invalid.instances.push_back(invalid.instances[0]);const auto savedSize=opened.snapshot().instances.size();
    require(!opened.replace(invalid,r).ok&&opened.snapshot().instances.size()==savedSize,"invalid restore retains prior scene");
    invalid=restored;invalid.instances[1].asset=conflict;
    require(!opened.replace(invalid,r).ok&&opened.snapshot().instances.size()==savedSize,"conflicting asset identity cannot replace scene");
    QFile damaged(relocated);require(damaged.open(QIODevice::ReadWrite),"open corrupt fixture");
    require(damaged.seek(damaged.size()-1),"seek corrupt fixture");char last;require(damaged.read(&last,1)==1,"read corrupt fixture");last^=0x40;require(damaged.seek(damaged.size()-1)&&damaged.write(&last,1)==1,"write corrupt fixture");damaged.close();
    const auto prior=restored.instances.size();require(!infrastructure::loadWorkpieceScene(relocated,restored,error)&&restored.instances.size()==prior,"checksum failure leaves output intact");
    require(!infrastructure::saveWorkpieceScene(QDir(temp.path()).filePath("missing-dir/failed.r3wp"),service.snapshot(),error),"save failure reported");
}
}
int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);
    try {mathAndNormalization();serviceAndPersistence();externalDependencyPersistence();cacheRecoveryPersistence();std::cout<<"Workpiece regression: "<<checks<<" checks passed\n";return 0;}
    catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
