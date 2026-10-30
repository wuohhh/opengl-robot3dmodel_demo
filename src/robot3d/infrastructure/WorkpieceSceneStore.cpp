#include "robot3d/infrastructure/WorkpieceSceneStore.h"
#include "robot3d/infrastructure/WorkpieceFileLoader.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryFile>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
namespace robot3d { namespace infrastructure {
using namespace domain;
namespace {
const quint64 kMaxPackage=1536ULL*1024*1024;
const quint64 kMaxAllocation=2048ULL*1024*1024;
const char kMagic[8]={'R','3','W','P','K','G','0','1'};
void require(bool ok,const char*message) {if(!ok)throw std::runtime_error(message);}
void configure(QDataStream&s) {s.setVersion(QDataStream::Qt_5_6);s.setByteOrder(QDataStream::LittleEndian);s.setFloatingPointPrecision(QDataStream::DoublePrecision);}
void check(QDataStream&s) {require(s.status()==QDataStream::Ok,"Truncated or invalid package data");}
void putString(QDataStream&s,const std::string&v) {
    require(v.size()<=1024*1024,"Package string is too long");
    const QByteArray bytes(v.data(),int(v.size()));require(QString::fromUtf8(bytes).toUtf8()==bytes,"Package contains invalid UTF-8");
    s<<quint32(v.size());if(!v.empty())require(s.writeRawData(v.data(),static_cast<int>(v.size()))==int(v.size()),"Package write failed");
}
struct Budget {
    quint64 allocated=0;
    void take(quint64 bytes) {require(bytes<=kMaxAllocation&&allocated<=kMaxAllocation-bytes,"Package memory budget exceeded");allocated+=bytes;}
};
std::string getString(QDataStream&s,Budget&b) {
    quint32 size=0;s>>size;check(s);require(size<=1024*1024,"Package string is too long");b.take(size);
    std::string out(size,'\0');if(size)require(s.readRawData(&out[0],size)==int(size),"Truncated package string");
    const QByteArray bytes(out.data(),int(out.size()));require(QString::fromUtf8(bytes).toUtf8()==bytes,"Package contains invalid UTF-8");return out;
}
void putBlob(QDataStream&s,const std::vector<std::uint8_t>&bytes) {
    s<<quint64(bytes.size());std::size_t offset=0;
    while(offset<bytes.size()) {const int n=int((std::min)(std::size_t(1024*1024),bytes.size()-offset));require(s.writeRawData(reinterpret_cast<const char*>(bytes.data()+offset),n)==n,"Package blob write failed");offset+=n;}
}
std::vector<std::uint8_t> getBlob(QDataStream&s,Budget&b,quint64 limit) {
    quint64 size=0;s>>size;check(s);require(size<=limit,"Package source blob exceeds limit");b.take(size);
    std::vector<std::uint8_t> out(static_cast<std::size_t>(size));quint64 offset=0;
    while(offset<size) {const int n=int((std::min)(quint64(1024*1024),size-offset));require(s.readRawData(reinterpret_cast<char*>(out.data()+offset),n)==n,"Truncated package source data");offset+=n;}
    return out;
}
void putVec(QDataStream&s,const Vec3d&v) {s<<v.x<<v.y<<v.z;}
Vec3d getVec(QDataStream&s) {Vec3d v;s>>v.x>>v.y>>v.z;check(s);require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"Invalid package vector");return v;}
void putMatrix(QDataStream&s,const Matrix4d&m) {for(double v:m.values)s<<v;}
Matrix4d getMatrix(QDataStream&s) {Matrix4d m;for(auto&v:m.values)s>>v;check(s);require(finiteMatrix(m),"Invalid package transform");return m;}
void putPose(QDataStream&s,const EulerPoseDeg&p) {s<<p.X<<p.Y<<p.Z<<p.Rx<<p.Ry<<p.Rz;}
EulerPoseDeg getPose(QDataStream&s) {
    EulerPoseDeg p={};s>>p.X>>p.Y>>p.Z>>p.Rx>>p.Ry>>p.Rz;check(s);
    for(double v:{p.X,p.Y,p.Z,p.Rx,p.Ry,p.Rz})require(std::isfinite(v),"Invalid package pose");return p;
}
quint32 count(QDataStream&s,quint32 limit) {quint32 n=0;s>>n;check(s);require(n<=limit,"Package count exceeds supported capacity");return n;}
bool safeRelative(const std::string&name) {
    const QString p=QString::fromUtf8(name.data(),int(name.size())).replace('\\','/');
    if(p.isEmpty()||QDir::isAbsolutePath(p)||p.contains(':'))return false;
    const auto parts=p.split('/');for(const auto&part:parts)if(part==".."||part.isEmpty())return false;return true;
}
std::string hashBlob(const std::vector<std::uint8_t>&bytes) {
    QCryptographicHash h(QCryptographicHash::Sha256);std::size_t offset=0;
    while(offset<bytes.size()) {int n=int((std::min)(std::size_t(1024*1024),bytes.size()-offset));h.addData(reinterpret_cast<const char*>(bytes.data()+offset),n);offset+=n;}
    return h.result().toHex().toStdString();
}
bool validSourceReference(const std::string&name) {
    // An original CAD/OBJ reference is opaque metadata, never an extraction path.
    const QByteArray bytes(name.data(),int(name.size()));
    return !name.empty()&&name.find(char(0))==std::string::npos&&QString::fromUtf8(bytes).toUtf8()==bytes;
}
std::string resourceKey(quint32 index,const std::string&checksum) {
    return "resources/"+std::to_string(index)+"-"+checksum;
}
class CacheSlice : public QIODevice {
public:
    CacheSlice(QIODevice&source,qint64 size):source_(source),remaining_(size) {open(QIODevice::ReadOnly|QIODevice::Unbuffered);}
    bool isSequential()const override{return true;}
    bool atEnd()const override{return remaining_==0;}
protected:
    qint64 readData(char*data,qint64 size)override {
        if(remaining_==0)return 0;
        const auto n=source_.read(data,(std::min)(size,remaining_));if(n>0)remaining_-=n;return n;
    }
    qint64 writeData(const char*,qint64)override{return -1;}
private:
    QIODevice&source_;qint64 remaining_;
};
QByteArray hashRange(QIODevice&device,quint64 size) {
    QCryptographicHash digest(QCryptographicHash::Sha256);
    while(size) {
        const auto part=device.read(qint64((std::min)(quint64(1024*1024),size)));
        require(!part.isEmpty(),"Cannot read mesh cache");digest.addData(part);size-=quint64(part.size());
    }
    return digest.result();
}
void putComponents(QDataStream&s,const std::vector<WorkpieceComponent>&components) {
    s<<quint32(components.size());
    for(const auto&c:components) {
        require(c.meshes.size()<=100000,"Package mesh count exceeds supported capacity");
        putString(s,c.id);putString(s,c.name);putString(s,c.parentId);putMatrix(s,c.transform);s<<c.selected<<quint32(c.meshes.size());
        for(const auto&m:c.meshes) {
            s<<quint32(m.vertices.size());for(const auto&v:m.vertices) {putVec(s,v.position);putVec(s,v.normal);for(float color:v.color)s<<double(color);}
            s<<quint32(m.indices.size());for(auto index:m.indices)s<<quint32(index);
            s<<quint32(m.triangleFaceIds.size());for(auto id:m.triangleFaceIds)s<<quint32(id);
        }
    }
    check(s);
}
std::vector<WorkpieceComponent> getComponents(QDataStream&s,Budget&b) {
    std::vector<WorkpieceComponent> components;
    const auto nc=count(s,100000);b.take(quint64(nc)*sizeof(WorkpieceComponent));components.reserve(nc);
    quint64 triangles=0,vertices=0;
    for(quint32 n=0;n<nc;++n) {
        WorkpieceComponent c;c.id=getString(s,b);c.name=getString(s,b);c.parentId=getString(s,b);c.transform=getMatrix(s);s>>c.selected;
        const auto meshes=count(s,100000);b.take(quint64(meshes)*sizeof(WorkpieceMesh));c.meshes.reserve(meshes);
        for(quint32 k=0;k<meshes;++k) {
            WorkpieceMesh m;const auto nv=count(s,15000000);vertices+=nv;require(vertices<=15000000,"Asset vertex capacity exceeded");
            b.take(quint64(nv)*sizeof(WorkpieceVertex));m.vertices.resize(nv);
            for(auto&v:m.vertices) {v.position=getVec(s);v.normal=getVec(s);for(auto&color:v.color){double value;s>>value;require(std::isfinite(value)&&value>=0&&value<=1,"Invalid vertex color");color=float(value);}}
            const auto ni=count(s,15000000);require(ni%3==0,"Invalid triangle index count");triangles+=ni/3;require(triangles<=5000000,"Asset triangle capacity exceeded");
            b.take(quint64(ni)*sizeof(std::uint32_t));m.indices.resize(ni);for(auto&index:m.indices){quint32 value;s>>value;require(value<nv,"Invalid triangle index");index=value;}
            const auto nf=count(s,5000000);require(nf==0||nf==ni/3,"Invalid surface mapping count");
            b.take(quint64(nf)*sizeof(std::uint32_t));m.triangleFaceIds.resize(nf);for(auto&id:m.triangleFaceIds){quint32 value;s>>value;id=value;}
            check(s);c.meshes.push_back(std::move(m));
        }
        components.push_back(std::move(c));
    }
    return components;
}
bool matchingManifest(const std::vector<WorkpieceComponent>&cache,const std::vector<WorkpieceComponent>&manifest) {
    if(cache.size()!=manifest.size())return false;
    for(std::size_t n=0;n<cache.size();++n) {
        const auto&a=cache[n];const auto&b=manifest[n];
        if(a.id!=b.id||a.name!=b.name||a.parentId!=b.parentId||a.selected!=b.selected)return false;
    }
    return true;
}
WorkpieceAssetPtr rebuildAsset(const WorkpieceAsset&stored,const std::vector<WorkpieceComponent>&manifest,const QString&reason,Budget&budget) {
    require(!stored.sourceBytes.empty(),"Mesh cache is invalid and the package has no source snapshot");
    WorkpieceLoadOptions options;options.linearDeflectionMm=stored.linearDeflectionMm;options.angularDeflectionDeg=stored.angularDeflectionDeg;
    std::shared_ptr<WorkpieceAsset>raw;QString error;
    const bool loaded=rebuildWorkpieceAsset(stored,options,raw,error);
    require(loaded,qPrintable(QString::fromUtf8("工件缓存失效，源快照重建失败：")+error));
    require(bool(raw)&&raw->components.size()==manifest.size(),"Source components changed; confirmed component selection cannot be restored");
    std::map<std::string,const WorkpieceComponent*> confirmed;
    for(const auto&c:manifest)require(!c.id.empty()&&confirmed.insert(std::make_pair(c.id,&c)).second,"Invalid confirmed component manifest");
    WorkpieceNormalizeOptions normalization;normalization.unitToMm=1;normalization.selectAllComponents=false;
    for(auto&c:raw->components) {
        const auto found=confirmed.find(c.id);
        require(found!=confirmed.end()&&found->second->name==c.name&&found->second->parentId==c.parentId,
                "Source hierarchy changed; confirmed component selection cannot be restored");
        if(found->second->selected)normalization.selectedComponentIds.push_back(c.id);
        // The saved matrix contains the exact approved scale, handedness, axes
        // and local-origin offset; never infer a new origin from a new mesh.
        c.transform=multiply(stored.normalization,c.transform);
    }
    const auto normalized=normalizeWorkpieceAsset(*raw,normalization);require(normalized.ok,normalized.error.c_str());
    // normalizeWorkpieceAsset creates this unique mutable object and only
    // exposes it as const. Complete its restored metadata before publication.
    require(normalized.value.use_count()==1,"Unexpected shared rebuilt candidate");
    auto restored=std::const_pointer_cast<WorkpieceAsset>(normalized.value);
    restored->id=stored.id;restored->name=stored.name;restored->sourcePath=stored.sourcePath;
    restored->sourceUnit=stored.sourceUnit;restored->sourceUnitToMm=stored.sourceUnitToMm;restored->normalization=stored.normalization;
    std::set<std::string>warnings(restored->warnings.begin(),restored->warnings.end());
    for(const auto&w:stored.warnings)if(warnings.insert(w).second)restored->warnings.push_back(w);
    restored->warnings.push_back((QString::fromUtf8("工件缓存已从包内源快照重建；沿用已确认单位、轴向、原点偏移、组件选择和离散参数。原因：")+reason).toUtf8().toStdString());
    auto valid=validateWorkpieceAsset(*restored);require(valid.ok,valid.error.c_str());
    budget.take(quint64(restored->components.size())*sizeof(WorkpieceComponent));
    for(const auto&c:restored->components)for(const auto&m:c.meshes)
        budget.take(quint64(m.vertices.size())*sizeof(WorkpieceVertex)+quint64(m.indices.size()+m.triangleFaceIds.size())*sizeof(std::uint32_t));
    return restored;
}
void putAsset(QDataStream&s,const WorkpieceAsset&a) {
    auto valid=validateWorkpieceAsset(a);require(valid.ok,valid.error.c_str());require(!a.id.empty(),"Missing asset ID");
    require(std::isfinite(a.linearDeflectionMm)&&a.linearDeflectionMm>0&&std::isfinite(a.angularDeflectionDeg)&&a.angularDeflectionDeg>0,"Invalid tessellation metadata");
    putString(s,a.id);putString(s,a.name);putString(s,a.sourcePath);putString(s,a.format);putString(s,a.adapterVersion);putString(s,a.sourceUnit);
    s<<a.sourceUnitToMm<<a.unitKnown<<a.normalized<<a.linearDeflectionMm<<a.angularDeflectionDeg;
    putMatrix(s,a.normalization);putVec(s,a.boundsMin);putVec(s,a.boundsMax);putBlob(s,a.sourceBytes);putString(s,hashBlob(a.sourceBytes));
    require(a.dependencies.size()<=10000,"Package dependency count exceeds supported capacity");
    require(a.warnings.size()<=100000,"Package warning count exceeds supported capacity");
    s<<quint32(a.dependencies.size());
    quint32 resourceIndex=0;std::set<std::string> references;
    for(const auto&f:a.dependencies) {
        require(validSourceReference(f.relativePath),"Invalid dependency source reference");
        require(references.insert(f.relativePath).second,"Duplicated dependency source reference");
        const auto checksum=hashBlob(f.bytes);
        putString(s,resourceKey(resourceIndex++,checksum));putString(s,f.relativePath);putBlob(s,f.bytes);putString(s,checksum);
    }
    s<<quint32(a.warnings.size());for(const auto&w:a.warnings)putString(s,w);
    // The independent manifest survives cache replacement and preserves the
    // user's approved component selection without guessing after re-parsing.
    s<<quint32(a.components.size());
    for(const auto&c:a.components) {putString(s,c.id);putString(s,c.name);putString(s,c.parentId);s<<c.selected;}
    QIODevice&device=*s.device();require(s.writeRawData("R3CACHE3",8)==8,"Cannot write mesh cache marker");
    const auto header=device.pos();s<<quint64(0);
    const QByteArray emptyDigest(32,char(0));require(s.writeRawData(emptyDigest.constData(),32)==32,"Cannot reserve mesh cache checksum");
    const auto begin=device.pos();putComponents(s,a.components);const auto end=device.pos();
    require(end>=begin&&quint64(end-begin)<=kMaxPackage,"Mesh cache exceeds size limit");
    require(device.seek(begin),"Cannot checksum mesh cache");const auto checksum=hashRange(device,quint64(end-begin));
    require(device.seek(header),"Cannot write mesh cache header");s<<quint64(end-begin);
    require(s.writeRawData(checksum.constData(),32)==32&&device.seek(end),"Cannot finish mesh cache header");check(s);
}
WorkpieceAssetPtr getAsset(QDataStream&s,Budget&b,quint32 version) {
    std::shared_ptr<WorkpieceAsset>a(new WorkpieceAsset);
    a->id=getString(s,b);a->name=getString(s,b);a->sourcePath=getString(s,b);a->format=getString(s,b);a->adapterVersion=getString(s,b);a->sourceUnit=getString(s,b);
    s>>a->sourceUnitToMm>>a->unitKnown>>a->normalized>>a->linearDeflectionMm>>a->angularDeflectionDeg;check(s);
    require(!a->id.empty()&&a->normalized&&a->unitKnown&&std::isfinite(a->sourceUnitToMm)&&a->sourceUnitToMm>0,"Invalid asset normalization metadata");
    require(std::isfinite(a->linearDeflectionMm)&&a->linearDeflectionMm>0&&std::isfinite(a->angularDeflectionDeg)&&a->angularDeflectionDeg>0,"Invalid tessellation metadata");
    a->normalization=getMatrix(s);a->boundsMin=getVec(s);a->boundsMax=getVec(s);a->sourceBytes=getBlob(s,b,512ULL*1024*1024);
    require(a->boundsMin.x<=a->boundsMax.x&&a->boundsMin.y<=a->boundsMax.y&&a->boundsMin.z<=a->boundsMax.z,"Invalid asset bounds metadata");
    require(getString(s,b)==hashBlob(a->sourceBytes),"Source snapshot checksum mismatch");
    const auto dependencies=count(s,10000);a->dependencies.reserve(dependencies);quint64 sourceBytes=a->sourceBytes.size();
    std::set<std::string> dependencyNames;
    for(quint32 n=0;n<dependencies;++n) {
        WorkpieceSourceFile f;std::string key;
        if(version>=2) {key=getString(s,b);require(safeRelative(key),"Unsafe package resource key");}
        f.relativePath=getString(s,b);
        require(version>=2?validSourceReference(f.relativePath):safeRelative(f.relativePath),"Invalid dependency source reference");
        require(dependencyNames.insert(f.relativePath).second,"Duplicated dependency path");
        f.bytes=getBlob(s,b,1024ULL*1024*1024);sourceBytes+=f.bytes.size();require(sourceBytes<=1024ULL*1024*1024,"Source dependencies exceed 1 GiB");
        f.sha256=getString(s,b);require(f.sha256==hashBlob(f.bytes),"Dependency checksum mismatch");
        if(version>=2)require(key==resourceKey(n,f.sha256),"Package resource key does not match its content");
        a->dependencies.push_back(std::move(f));
    }
    const auto warnings=count(s,100000);for(quint32 n=0;n<warnings;++n)a->warnings.push_back(getString(s,b));
    if(a->sourceBytes.empty()) {
        const auto warning=QString::fromUtf8("此资产仅包含显示缓存，未包含源几何快照；缓存失效时无法重新生成。").toUtf8().toStdString();
        if(std::find(a->warnings.begin(),a->warnings.end(),warning)==a->warnings.end())a->warnings.push_back(warning);
    }
    QString reason;const auto currentAdapter=workpieceAdapterVersion(QString::fromUtf8(a->format.c_str()));
    if(!currentAdapter.isEmpty()&&currentAdapter.toUtf8().toStdString()!=a->adapterVersion)
        reason=QString::fromUtf8("解析适配器版本已变化：%1 → %2").arg(QString::fromUtf8(a->adapterVersion.c_str()),currentAdapter);
    if(version<3) {
        a->components=getComponents(s,b);auto valid=validateWorkpieceAsset(*a);require(valid.ok,valid.error.c_str());
        if(reason.isEmpty())return a;
        std::vector<WorkpieceComponent>manifest;
        for(const auto&c:a->components) {WorkpieceComponent m;m.id=c.id;m.name=c.name;m.parentId=c.parentId;m.selected=c.selected;manifest.push_back(std::move(m));}
        a->components.clear();return rebuildAsset(*a,manifest,reason,b);
    }
    std::vector<WorkpieceComponent>manifest;const auto nc=count(s,100000);b.take(quint64(nc)*sizeof(WorkpieceComponent));manifest.reserve(nc);
    std::set<std::string>componentIds;
    for(quint32 n=0;n<nc;++n) {
        WorkpieceComponent c;c.id=getString(s,b);c.name=getString(s,b);c.parentId=getString(s,b);s>>c.selected;check(s);
        require(!c.id.empty()&&componentIds.insert(c.id).second,"Invalid confirmed component manifest");manifest.push_back(std::move(c));
    }
    char cacheMarker[8];require(s.readRawData(cacheMarker,8)==8,"Missing mesh cache marker");
    if(QByteArray(cacheMarker,8)!="R3CACHE3")reason=QString::fromUtf8("规范化网格缓存头失效");
    quint64 cacheSize;s>>cacheSize;QByteArray expected(32,char(0));require(s.readRawData(expected.data(),32)==32,"Missing mesh cache checksum");check(s);
    QIODevice&device=*s.device();const auto begin=device.pos();
    require(cacheSize<=kMaxPackage&&begin>=0&&quint64(begin)<=quint64(device.size())&&cacheSize<=quint64(device.size()-begin),"Invalid mesh cache extent");
    const auto end=begin+qint64(cacheSize);const auto actual=hashRange(device,cacheSize);
    if(actual!=expected)reason=QString::fromUtf8("规范化网格缓存校验失败");
    const auto beforeCache=b.allocated;
    if(reason.isEmpty()) {
        try {
            require(device.seek(begin),"Cannot seek mesh cache");CacheSlice slice(device,qint64(cacheSize));QDataStream cache(&slice);configure(cache);
            a->components=getComponents(cache,b);check(cache);require(slice.atEnd(),"Unexpected trailing mesh cache data");
            require(matchingManifest(a->components,manifest),"Mesh cache disagrees with confirmed component manifest");
            auto valid=validateWorkpieceAsset(*a);require(valid.ok,valid.error.c_str());
        }catch(const std::exception&e) {reason=QString::fromUtf8(e.what());a->components.clear();b.allocated=beforeCache;}
    }
    require(device.seek(end),"Cannot skip mesh cache");
    if(reason.isEmpty())return a;
    return rebuildAsset(*a,manifest,reason,b);
}
void putState(QDataStream&s,const WorkpieceSceneState&state) {
    require(state.instances.size()<=50,"Scene instance capacity exceeded");
    std::map<std::string,WorkpieceAssetPtr> assets;std::set<std::string> ids,names;quint64 uniqueTriangles=0,visibleTriangles=0;
    for(const auto&i:state.instances) {
        require(bool(i.asset),"Missing scene asset");require(!i.id.empty()&&!i.name.empty()&&ids.insert(i.id).second,"Invalid or duplicated instance ID");
        std::string folded=i.name;for(auto&c:folded)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
        require(names.insert(folded).second,"Duplicated instance name");
        const auto inserted=assets.insert(std::make_pair(i.asset->id,i.asset));
        if(inserted.second)uniqueTriangles+=workpieceTriangleCount(*i.asset);
        if(i.visible)visibleTriangles+=workpieceTriangleCount(*i.asset);
        require(uniqueTriangles<=10000000&&visibleTriangles<=10000000,"Scene triangle capacity exceeded");
        require(inserted.second||sameWorkpieceAssetContent(*inserted.first->second,*i.asset),"Asset ID conflicts with different content");
        require(finiteMatrix(i.fallbackWorld)&&finiteMatrix(poseMatrix(i.pose)),"Invalid instance placement");
        require(std::isfinite(i.opacity)&&i.opacity>=0&&i.opacity<=1,"Invalid instance opacity");
        require(int(i.displayMode)>=0&&int(i.displayMode)<=2,"Invalid instance display mode");
        for(float color:i.color)require(std::isfinite(color)&&color>=0&&color<=1,"Invalid instance color");
    }
    require(state.selectedId.empty()||ids.count(state.selectedId)!=0,"Invalid stored selection");
    require(finiteMatrix(poseMatrix(state.baseInWorld)),"Invalid robot base pose");
    putString(s,state.robotId);putPose(s,state.baseInWorld);putString(s,state.selectedId);s<<quint32(assets.size());
    for(const auto&a:assets)putAsset(s,*a.second);
    s<<quint32(state.instances.size());
    for(const auto&i:state.instances) {
        putString(s,i.id);putString(s,i.name);putString(s,i.asset->id);putString(s,i.parentCoordinateId);putPose(s,i.pose);putMatrix(s,i.fallbackWorld);
        s<<i.visible<<i.locked<<i.colorOverride<<i.showAxes;for(float color:i.color)s<<double(color);s<<i.opacity<<qint32(i.displayMode);putString(s,i.warning);
    }
    check(s);
}
WorkpieceSceneState getState(QDataStream&s,Budget&b,quint32 version) {
    WorkpieceSceneState state;state.robotId=getString(s,b);state.baseInWorld=getPose(s);state.selectedId=getString(s,b);
    std::map<std::string,WorkpieceAssetPtr>assets;
    const auto na=count(s,50);quint64 uniqueTriangles=0;
    for(quint32 n=0;n<na;++n){auto a=getAsset(s,b,version);require(assets.insert(std::make_pair(a->id,a)).second,"Duplicated asset ID");uniqueTriangles+=workpieceTriangleCount(*a);require(uniqueTriangles<=10000000,"Scene unique asset capacity exceeded");}
    const auto ni=count(s,50);std::set<std::string>ids,names;quint64 visibleTriangles=0;
    for(quint32 n=0;n<ni;++n) {
        WorkpieceInstance i;i.id=getString(s,b);i.name=getString(s,b);const auto assetId=getString(s,b);
        require(!i.id.empty()&&!i.name.empty()&&ids.insert(i.id).second,"Invalid or duplicated instance");
        std::string folded=i.name;for(auto&c:folded)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
        require(names.insert(folded).second,"Duplicated instance name");
        const auto found=assets.find(assetId);require(found!=assets.end(),"Missing referenced asset");i.asset=found->second;
        i.parentCoordinateId=getString(s,b);i.pose=getPose(s);i.fallbackWorld=getMatrix(s);
        s>>i.visible>>i.locked>>i.colorOverride>>i.showAxes;
        for(auto&color:i.color){double value;s>>value;require(std::isfinite(value)&&value>=0&&value<=1,"Invalid instance color");color=float(value);}
        qint32 mode;s>>i.opacity>>mode;check(s);require(std::isfinite(i.opacity)&&i.opacity>=0&&i.opacity<=1&&mode>=0&&mode<=2,"Invalid instance display settings");
        i.displayMode=static_cast<WorkpieceDisplayMode>(mode);i.warning=getString(s,b);
        if(i.visible)visibleTriangles+=workpieceTriangleCount(*i.asset);require(visibleTriangles<=10000000,"Visible scene capacity exceeded");
        state.instances.push_back(std::move(i));
    }
    if(!state.selectedId.empty())require(ids.count(state.selectedId)!=0,"Invalid stored selection");
    return state;
}
}
bool saveWorkpieceScene(const QString&path,const WorkpieceSceneState&state,QString&error) {
    error.clear();
    try {
        require(!path.isEmpty(),"Scene package path is empty");
        const QFileInfo info(path);QTemporaryFile payload(info.absoluteDir().filePath(".robot3d-workpiece-XXXXXX.tmp"));
        require(payload.open(),"Cannot create temporary scene package");
        {QDataStream stream(&payload);configure(stream);putState(stream,state);}
        require(payload.flush(),"Cannot flush scene package");
        require(payload.size()>=0&&quint64(payload.size())<=kMaxPackage,"Scene package exceeds size limit");
        require(payload.seek(0),"Cannot read temporary scene package");
        QCryptographicHash checksum(QCryptographicHash::Sha256);require(checksum.addData(&payload),"Cannot checksum scene package");
        const auto digest=checksum.result();require(payload.seek(0),"Cannot rewind scene package");
        QSaveFile destination(path);destination.setDirectWriteFallback(false);require(destination.open(QIODevice::WriteOnly),"Cannot open destination package for atomic save");
        {QDataStream header(&destination);configure(header);require(header.writeRawData(kMagic,8)==8,"Cannot write scene header");header<<quint32(3)<<quint64(payload.size());require(header.writeRawData(digest.constData(),digest.size())==digest.size(),"Cannot write checksum");check(header);}
        while(!payload.atEnd()){const auto block=payload.read(1024*1024);require(!block.isEmpty()||payload.atEnd(),"Cannot read temporary package");require(destination.write(block)==block.size(),"Cannot write scene package");}
        require(destination.commit(),"Cannot atomically commit scene package");return true;
    } catch(const std::exception&e){error=QString::fromUtf8(e.what());return false;} catch(...){error=QStringLiteral("Unexpected scene package save failure");return false;}
}
bool loadWorkpieceScene(const QString&path,WorkpieceSceneState&state,QString&error) {
    error.clear();
    try {
        QFile input(path);require(input.open(QIODevice::ReadOnly),"Cannot open scene package");require(input.size()>=52&&quint64(input.size())<=kMaxPackage+52,"Invalid scene package size");
        QDataStream header(&input);configure(header);char magic[8];require(header.readRawData(magic,8)==8&&std::equal(magic,magic+8,kMagic),"Not a robot3d workpiece package");
        quint32 version;quint64 size;header>>version>>size;require(version>=1&&version<=3,"Unsupported scene package version");
        QByteArray expected(32,'\0');require(header.readRawData(expected.data(),32)==32,"Missing scene package checksum");check(header);
        require(size==quint64(input.size()-52)&&size<=kMaxPackage,"Scene package length mismatch");
        QCryptographicHash checksum(QCryptographicHash::Sha256);require(checksum.addData(&input),"Cannot verify scene package");require(checksum.result()==expected,"Scene package checksum mismatch");
        require(input.seek(52),"Cannot seek scene package payload");QDataStream stream(&input);configure(stream);Budget budget;
        WorkpieceSceneState candidate=getState(stream,budget,version);check(stream);require(input.atEnd(),"Unexpected trailing package payload");state=std::move(candidate);return true;
    }catch(const std::exception&e){error=QString::fromUtf8(e.what());return false;}catch(...){error=QStringLiteral("Unexpected scene package read failure");return false;}
}
} }
