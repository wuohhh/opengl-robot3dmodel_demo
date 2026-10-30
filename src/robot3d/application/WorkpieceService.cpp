#include "robot3d/application/WorkpieceService.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
namespace robot3d { namespace domain {
namespace {
const double kPi = 3.14159265358979323846;
bool finiteVec(const Vec3d &v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool finitePose(const EulerPoseDeg &p) {
    return std::isfinite(p.X)&&std::isfinite(p.Y)&&std::isfinite(p.Z)&&std::isfinite(p.Rx)&&std::isfinite(p.Ry)&&std::isfinite(p.Rz);
}
double dot(const Vec3d&a,const Vec3d&b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3d cross(const Vec3d&a,const Vec3d&b) { return Vec3d(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x); }
Vec3d subtract(const Vec3d&a,const Vec3d&b) { return Vec3d(a.x-b.x,a.y-b.y,a.z-b.z); }
Vec3d normalized(const Vec3d &v) { const double n=std::sqrt(dot(v,v)); return n>1e-24?Vec3d(v.x/n,v.y/n,v.z/n):Vec3d(); }
Vec3d axisVector(int a) { Vec3d v; double x=a>0?1.0:-1.0; if(std::abs(a)==1)v.x=x; else if(std::abs(a)==2)v.y=x; else if(std::abs(a)==3)v.z=x; return v; }
double determinant(const Matrix4d &m) {
    const auto &a=m.values;
    return a[0]*(a[5]*a[10]-a[6]*a[9])-a[1]*(a[4]*a[10]-a[6]*a[8])+a[2]*(a[4]*a[9]-a[5]*a[8]);
}
Vec3d transformNormal(const Matrix4d &m,const Vec3d &n) {
    const auto &a=m.values; const double d=determinant(m);
    if(std::fabs(d)<1e-24)return Vec3d();
    return normalized(Vec3d(
        ((a[5]*a[10]-a[6]*a[9])*n.x+(a[6]*a[8]-a[4]*a[10])*n.y+(a[4]*a[9]-a[5]*a[8])*n.z)/d,
        ((a[2]*a[9]-a[1]*a[10])*n.x+(a[0]*a[10]-a[2]*a[8])*n.y+(a[1]*a[8]-a[0]*a[9])*n.z)/d,
        ((a[1]*a[6]-a[2]*a[5])*n.x+(a[2]*a[4]-a[0]*a[6])*n.y+(a[0]*a[5]-a[1]*a[4])*n.z)/d));
}
std::string fnvId(const std::string &text) {
    std::uint64_t h=1469598103934665603ULL; for(unsigned char c:text){h^=c;h*=1099511628211ULL;}
    std::ostringstream o;o<<std::hex<<h;return o.str();
}
}
Matrix4d multiply(const Matrix4d &a,const Matrix4d &b) {
    Matrix4d r;
    for(int row=0;row<4;++row)for(int col=0;col<4;++col) {
        double v=0;for(int k=0;k<4;++k)v+=a.values[row*4+k]*b.values[k*4+col];
        r.values[row*4+col]=v;
    }
    return r;
}
Matrix4d rigidInverse(const Matrix4d &m) {
    Matrix4d r;for(int i=0;i<3;++i)for(int j=0;j<3;++j)r.values[i*4+j]=m.values[j*4+i];
    for(int i=0;i<3;++i)r.values[i*4+3]=-(r.values[i*4]*m.values[3]+r.values[i*4+1]*m.values[7]+r.values[i*4+2]*m.values[11]);
    return r;
}
Matrix4d poseMatrix(const EulerPoseDeg &p) {
    const double x=p.Rx*kPi/180.0,y=p.Ry*kPi/180.0,z=p.Rz*kPi/180.0;
    const double cx=std::cos(x),sx=std::sin(x),cy=std::cos(y),sy=std::sin(y),cz=std::cos(z),sz=std::sin(z);
    Matrix4d m;
    m.values={{cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx,p.X,
               sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx,p.Y,
               -sy,cy*sx,cy*cx,p.Z,0,0,0,1}};
    return m;
}
EulerPoseDeg matrixPose(const Matrix4d &m) {
    const auto&a=m.values;EulerPoseDeg p={a[3],a[7],a[11],0,0,0};
    const double sy=(std::max)(-1.0,(std::min)(1.0,-a[8]));
    const double y=std::asin(sy);
    double x=0,z=0;
    if(std::fabs(std::cos(y))>1e-10) {x=std::atan2(a[9],a[10]);z=std::atan2(a[4],a[0]);}
    else {x=0;z=std::atan2(-a[1],a[5]);}
    p.Rx=x*180/kPi;p.Ry=y*180/kPi;p.Rz=z*180/kPi;return p;
}
Vec3d transformPoint(const Matrix4d&m,const Vec3d&p) {
    const auto&a=m.values;return Vec3d(a[0]*p.x+a[1]*p.y+a[2]*p.z+a[3],a[4]*p.x+a[5]*p.y+a[6]*p.z+a[7],a[8]*p.x+a[9]*p.y+a[10]*p.z+a[11]);
}
Vec3d transformDirection(const Matrix4d&m,const Vec3d&p) {
    const auto&a=m.values;return Vec3d(a[0]*p.x+a[1]*p.y+a[2]*p.z,a[4]*p.x+a[5]*p.y+a[6]*p.z,a[8]*p.x+a[9]*p.y+a[10]*p.z);
}
bool finiteMatrix(const Matrix4d&m) {
    for(double v:m.values)if(!std::isfinite(v))return false;
    return std::fabs(m.values[12])<1e-12&&std::fabs(m.values[13])<1e-12&&std::fabs(m.values[14])<1e-12&&std::fabs(m.values[15]-1)<1e-12&&std::fabs(determinant(m))>1e-24;
}
bool parentWorldMatrix(const std::string&id,const RobotStateSnapshot&r,Matrix4d&out) {
    if(id=="world"||id=="world_0") {out=Matrix4d();return true;}
    if(id=="b_0") {out=poseMatrix(r.baseInWorld);return finiteMatrix(out);}
    if(id=="t_0"||id.empty())return false;
    for(const auto&c:r.coordinates)if(c.id==id) {
        if(!finitePose(c.pose))return false;
        out=id.compare(0,4,"wcs_")==0?poseMatrix(c.pose):multiply(poseMatrix(r.baseInWorld),poseMatrix(c.pose));
        return finiteMatrix(out);
    }
    return false;
}
Matrix4d workpieceWorldMatrix(const WorkpieceInstance&i,const RobotStateSnapshot&r) {
    Matrix4d parent;if(!parentWorldMatrix(i.parentCoordinateId,r,parent))return i.fallbackWorld;
    return multiply(parent,poseMatrix(i.pose));
}
bool sameWorkpieceAssetContent(const WorkpieceAsset&a,const WorkpieceAsset&b) {
    if(a.format!=b.format||a.adapterVersion!=b.adapterVersion||a.sourceUnit!=b.sourceUnit||
       a.sourceUnitToMm!=b.sourceUnitToMm||a.unitKnown!=b.unitKnown||a.normalized!=b.normalized||
       a.linearDeflectionMm!=b.linearDeflectionMm||a.angularDeflectionDeg!=b.angularDeflectionDeg||
       a.normalization.values!=b.normalization.values||a.sourceBytes!=b.sourceBytes||
       a.components.size()!=b.components.size()||a.dependencies.size()!=b.dependencies.size())return false;
    const auto sameVec=[](const Vec3d&x,const Vec3d&y){return x.x==y.x&&x.y==y.y&&x.z==y.z;};
    if(!sameVec(a.boundsMin,b.boundsMin)||!sameVec(a.boundsMax,b.boundsMax))return false;
    for(std::size_t n=0;n<a.dependencies.size();++n)if(a.dependencies[n].relativePath!=b.dependencies[n].relativePath||a.dependencies[n].bytes!=b.dependencies[n].bytes)return false;
    for(std::size_t n=0;n<a.components.size();++n) {
        const auto&x=a.components[n];const auto&y=b.components[n];
        if(x.id!=y.id||x.name!=y.name||x.parentId!=y.parentId||x.transform.values!=y.transform.values||x.selected!=y.selected||x.meshes.size()!=y.meshes.size())return false;
        for(std::size_t k=0;k<x.meshes.size();++k) {
            const auto&u=x.meshes[k];const auto&v=y.meshes[k];
            if(u.indices!=v.indices||u.triangleFaceIds!=v.triangleFaceIds||u.vertices.size()!=v.vertices.size())return false;
            for(std::size_t j=0;j<u.vertices.size();++j)if(!sameVec(u.vertices[j].position,v.vertices[j].position)||!sameVec(u.vertices[j].normal,v.vertices[j].normal)||u.vertices[j].color!=v.vertices[j].color)return false;
        }
    }
    return true;
}
std::uint64_t workpieceTriangleCount(const WorkpieceAsset&a) {
    std::uint64_t n=0;for(const auto&c:a.components)if(c.selected)for(const auto&m:c.meshes)n+=m.indices.size()/3;return n;
}
static core::Result<bool> validateWorkpieceAssetCancelable(const WorkpieceAsset&a,bool requireNormalized,const std::shared_ptr<std::atomic_bool>&cancellation) {
    if(cancellation&&cancellation->load())return core::Result<bool>::failure("Normalization cancelled");
    std::uint64_t progress=0;
    if(requireNormalized&&!a.normalized)return core::Result<bool>::failure("Workpiece is not normalized; confirm units and direction first");
    if(a.id.empty()&&a.normalized)return core::Result<bool>::failure("Normalized asset has no ID");
    if(a.normalized&&(!a.unitKnown||!std::isfinite(a.sourceUnitToMm)||a.sourceUnitToMm<=0||!finiteMatrix(a.normalization)||
        !finiteVec(a.boundsMin)||!finiteVec(a.boundsMax)||a.boundsMin.x>a.boundsMax.x||a.boundsMin.y>a.boundsMax.y||a.boundsMin.z>a.boundsMax.z))
        return core::Result<bool>::failure("Invalid normalization or bounds metadata");
    if(a.components.size()>100000)return core::Result<bool>::failure("Too many model components");
    std::uint64_t count=0,vertices=0,sourceBytes=a.sourceBytes.size();
    if(a.sourceBytes.size()>512ULL*1024*1024)return core::Result<bool>::failure("Source file exceeds 512 MiB");
    for(const auto&f:a.dependencies){sourceBytes+=f.bytes.size();if(sourceBytes>1024ULL*1024*1024)return core::Result<bool>::failure("Source dependencies exceed 1 GiB");}
    std::set<std::string> ids;
    for(const auto&c:a.components) {
        if(c.id.empty()||!ids.insert(c.id).second)return core::Result<bool>::failure("Empty or duplicated component ID");
        if(!finiteMatrix(c.transform))return core::Result<bool>::failure("Invalid component transform: "+c.name);
        if(!c.selected)continue;
        for(const auto&m:c.meshes) {
            if(m.indices.size()%3)return core::Result<bool>::failure("Triangle index count is invalid");
            if(!m.triangleFaceIds.empty()&&m.triangleFaceIds.size()!=m.indices.size()/3)return core::Result<bool>::failure("Face mapping is invalid");
            count+=m.indices.size()/3;vertices+=m.vertices.size();
            if(count>5000000||vertices>15000000)return core::Result<bool>::failure("Asset exceeds 5 million triangles or 15 million vertices");
            for(const auto&v:m.vertices) {
                if((++progress%4096)==0&&cancellation&&cancellation->load())return core::Result<bool>::failure("Normalization cancelled");
                if(!finiteVec(v.position)||!finiteVec(v.normal))return core::Result<bool>::failure("Non-finite vertex or normal");
                for(float color:v.color)if(!std::isfinite(color)||color<0||color>1)return core::Result<bool>::failure("Invalid vertex color");
            }
            for(auto index:m.indices)if(index>=m.vertices.size())return core::Result<bool>::failure("Triangle index is out of range");
        }
    }
    if(count==0)return core::Result<bool>::failure("Model has no selected surface triangles");
    return core::Result<bool>::success(true);
}
core::Result<bool> validateWorkpieceAsset(const WorkpieceAsset&a,bool requireNormalized) {
    return validateWorkpieceAssetCancelable(a,requireNormalized,std::shared_ptr<std::atomic_bool>());
}
core::Result<WorkpieceAssetPtr> normalizeWorkpieceAsset(const WorkpieceAsset&a,const WorkpieceNormalizeOptions&o) {
    const double scale=o.unitToMm>0?o.unitToMm:(a.unitKnown?a.sourceUnitToMm:0);
    if(!std::isfinite(scale)||scale<=0)return core::Result<WorkpieceAssetPtr>::failure("Source unit is unknown; explicitly select a positive conversion to millimeters");
    if(a.normalized)return core::Result<WorkpieceAssetPtr>::failure("Asset is already normalized; reload the source to change import parameters");
    if(std::abs(o.upAxis)<1||std::abs(o.upAxis)>3||std::abs(o.forwardAxis)<1||std::abs(o.forwardAxis)>3||std::abs(o.upAxis)==std::abs(o.forwardAxis))
        return core::Result<WorkpieceAssetPtr>::failure("Up and forward axes must be different valid signed axes");
    auto valid=validateWorkpieceAssetCancelable(a,false,o.cancellation);if(!valid.ok)return core::Result<WorkpieceAssetPtr>::failure(valid.error);
    Matrix4d normalization;
    const Vec3d x=axisVector(o.forwardAxis),z=axisVector(o.upAxis);
    Vec3d y=cross(z,x);if(o.leftHanded)y=Vec3d(-y.x,-y.y,-y.z);
    normalization.values={{x.x*scale,x.y*scale,x.z*scale,0,y.x*scale,y.y*scale,y.z*scale,0,z.x*scale,z.y*scale,z.z*scale,0,0,0,0,1}};
    if(o.cancellation&&o.cancellation->load())return core::Result<WorkpieceAssetPtr>::failure("Normalization cancelled");
    std::shared_ptr<WorkpieceAsset> out(new WorkpieceAsset);
    out->id=a.id;out->name=a.name;out->sourcePath=a.sourcePath;out->format=a.format;out->adapterVersion=a.adapterVersion;
    out->sourceUnit=a.sourceUnit;out->sourceUnitToMm=scale;out->unitKnown=true;out->normalized=true;
    out->sourceBytes=a.sourceBytes;out->dependencies=a.dependencies;out->warnings=a.warnings;
    out->linearDeflectionMm=a.linearDeflectionMm;out->angularDeflectionDeg=a.angularDeflectionDeg;
    std::set<std::string> selection(o.selectedComponentIds.begin(),o.selectedComponentIds.end());
    bool any=false;Vec3d lo,hi;std::uint64_t removed=0,progress=0;
    for(const auto&c:a.components) {
        WorkpieceComponent target;target.id=c.id;target.name=c.name;target.parentId=c.parentId;
        target.selected=o.selectAllComponents?c.selected:selection.count(c.id)!=0;
        if(target.selected) {
            const Matrix4d transform=multiply(normalization,c.transform);
            const bool reverse=determinant(transform)<0;
            for(const auto&m:c.meshes) {
                WorkpieceMesh mesh;
                mesh.vertices.reserve(m.vertices.size());mesh.indices.reserve(m.indices.size());mesh.triangleFaceIds.reserve(m.indices.size()/3);
                for(const auto&v:m.vertices) {
                    if((++progress%4096)==0&&o.cancellation&&o.cancellation->load())return core::Result<WorkpieceAssetPtr>::failure("Normalization cancelled");
                    WorkpieceVertex q=v;q.position=transformPoint(transform,v.position);q.normal=transformNormal(transform,v.normal);
                    if(!finiteVec(q.position))return core::Result<WorkpieceAssetPtr>::failure("Unit conversion overflowed model coordinates");
                    mesh.vertices.push_back(q);
                }
                for(std::size_t n=0;n<m.indices.size();n+=3) {
                    if((n%12288)==0&&o.cancellation&&o.cancellation->load())return core::Result<WorkpieceAssetPtr>::failure("Normalization cancelled");
                    const auto i0=m.indices[n],i1=m.indices[n+(reverse?2:1)],i2=m.indices[n+(reverse?1:2)];
                    const Vec3d normal=cross(subtract(mesh.vertices[i1].position,mesh.vertices[i0].position),subtract(mesh.vertices[i2].position,mesh.vertices[i0].position));
                    if(dot(normal,normal)<=1e-30) {++removed;continue;}
                    mesh.indices.push_back(i0);mesh.indices.push_back(i1);mesh.indices.push_back(i2);
                    mesh.triangleFaceIds.push_back(m.triangleFaceIds.empty()?static_cast<std::uint32_t>(n/3):m.triangleFaceIds[n/3]);
                    for(auto index:{i0,i1,i2})if(dot(mesh.vertices[index].normal,mesh.vertices[index].normal)<1e-24)mesh.vertices[index].normal=normalized(normal);
                }
                if(mesh.indices.empty())continue;
                for(auto index:mesh.indices) {
                    const auto&p=mesh.vertices[index].position;
                    if(!any) {lo=hi=p;any=true;}
                    else {lo.x=(std::min)(lo.x,p.x);lo.y=(std::min)(lo.y,p.y);lo.z=(std::min)(lo.z,p.z);
                          hi.x=(std::max)(hi.x,p.x);hi.y=(std::max)(hi.y,p.y);hi.z=(std::max)(hi.z,p.z);}
                }
                target.meshes.push_back(std::move(mesh));
            }
        }
        out->components.push_back(std::move(target));
    }
    if(!any)return core::Result<WorkpieceAssetPtr>::failure("No non-degenerate selected triangles remain");
    Vec3d origin;
    if(o.origin==WorkpieceOrigin::Center||o.origin==WorkpieceOrigin::BottomCenter) {
        origin=Vec3d((lo.x+hi.x)/2,(lo.y+hi.y)/2,o.origin==WorkpieceOrigin::Center?(lo.z+hi.z)/2:lo.z);
        for(auto&c:out->components)for(auto&m:c.meshes)for(auto&v:m.vertices)v.position=subtract(v.position,origin);
        lo=subtract(lo,origin);hi=subtract(hi,origin);
    }
    normalization.values[3]=-origin.x;normalization.values[7]=-origin.y;normalization.values[11]=-origin.z;
    out->normalization=normalization;out->boundsMin=lo;out->boundsMax=hi;
    if(removed) {std::ostringstream warning;warning<<"Skipped "<<removed<<" degenerate triangles";out->warnings.push_back(warning.str());}
    std::ostringstream key;key<<a.id<<std::setprecision(17)<<":"<<scale<<":"<<o.upAxis<<":"<<o.forwardAxis<<":"<<o.leftHanded<<":"<<static_cast<int>(o.origin)<<":"<<a.adapterVersion<<":"<<a.linearDeflectionMm<<":"<<a.angularDeflectionDeg;
    for(const auto&c:out->components)key<<":"<<c.id<<":"<<c.parentId<<":"<<c.name<<"="<<c.selected;
    if(a.id.empty())for(const auto&c:out->components)for(const auto&m:c.meshes)for(const auto&v:m.vertices)key<<v.position.x<<","<<v.position.y<<","<<v.position.z<<";";
    out->id="asset-"+fnvId(key.str());
    auto check=validateWorkpieceAssetCancelable(*out,true,o.cancellation);if(!check.ok)return core::Result<WorkpieceAssetPtr>::failure(check.error);
    return core::Result<WorkpieceAssetPtr>::success(out);
}
} }
namespace robot3d { namespace application {
using namespace domain;
namespace {
bool finitePoseValues(const EulerPoseDeg&p) {return std::isfinite(p.X)&&std::isfinite(p.Y)&&std::isfinite(p.Z)&&std::isfinite(p.Rx)&&std::isfinite(p.Ry)&&std::isfinite(p.Rz);}
std::string trimName(const std::string&n) {std::size_t a=n.find_first_not_of(" \t\r\n"),b=n.find_last_not_of(" \t\r\n");return a==std::string::npos?std::string():n.substr(a,b-a+1);}
std::string folded(std::string n) {for(auto&c:n)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return n;}
bool samePose(const EulerPoseDeg&a,const EulerPoseDeg&b) {return a.X==b.X&&a.Y==b.Y&&a.Z==b.Z&&a.Rx==b.Rx&&a.Ry==b.Ry&&a.Rz==b.Rz;}
bool sameInstance(const WorkpieceInstance&a,const WorkpieceInstance&b) {
    return a.asset==b.asset&&a.id==b.id&&a.name==b.name&&a.parentCoordinateId==b.parentCoordinateId&&samePose(a.pose,b.pose)&&
      a.visible==b.visible&&a.locked==b.locked&&a.colorOverride==b.colorOverride&&a.color==b.color&&a.opacity==b.opacity&&a.displayMode==b.displayMode&&a.showAxes==b.showAxes;
}
core::Result<bool> validateInstance(const WorkpieceInstance&i,const RobotStateSnapshot&r) {
    if(!i.asset)return core::Result<bool>::failure("Workpiece asset is missing");
    if(i.id.empty()||trimName(i.name).empty()||i.name.size()>1024)return core::Result<bool>::failure("Invalid workpiece ID or name");
    if(!finitePoseValues(i.pose)||!finiteMatrix(i.fallbackWorld))return core::Result<bool>::failure("Invalid workpiece pose");
    for(int row=0;row<3;++row)for(int col=0;col<3;++col) {
        double value=0;for(int k=0;k<3;++k)value+=i.fallbackWorld.values[row*4+k]*i.fallbackWorld.values[col*4+k];
        if(std::fabs(value-(row==col?1.0:0.0))>1e-7)return core::Result<bool>::failure("Stored workpiece world transform must be rigid");
    }
    if(!std::isfinite(i.opacity)||i.opacity<0||i.opacity>1)return core::Result<bool>::failure("Opacity must be between zero and one");
    for(float c:i.color)if(!std::isfinite(c)||c<0||c>1)return core::Result<bool>::failure("Invalid workpiece color");
    if(static_cast<int>(i.displayMode)<0||static_cast<int>(i.displayMode)>2)return core::Result<bool>::failure("Invalid display mode");
    Matrix4d parent;if(!parentWorldMatrix(i.parentCoordinateId,r,parent))return core::Result<bool>::failure("Parent coordinate is unavailable or dynamic");
    return core::Result<bool>::success(true);
}
}
WorkpieceService::WorkpieceService() {}
void WorkpieceService::remember() {
    History h={state_,contentVersion_};undo_.push_back(h);if(undo_.size()>20)undo_.pop_front();redo_.clear();
}
void WorkpieceService::changed() {contentVersion_=nextVersion_++;++state_.revision;}
void WorkpieceService::markSaved() {savedVersion_=contentVersion_;}
const WorkpieceInstance*WorkpieceService::find(const std::string&id)const {for(const auto&i:state_.instances)if(i.id==id)return &i;return nullptr;}
std::string WorkpieceService::uniqueName(const std::string&requested,const std::string&except)const {
    const std::string root=trimName(requested).empty()?"Workpiece":trimName(requested);
    std::string candidate=root;unsigned n=2;
    for(;;) {bool exists=false;for(const auto&i:state_.instances)if(i.id!=except&&folded(i.name)==folded(candidate)){exists=true;break;}if(!exists)return candidate;std::ostringstream s;s<<root<<" ("<<n++<<")";candidate=s.str();}
}
core::Result<std::string> WorkpieceService::add(WorkpieceAssetPtr asset,const std::string&name,const std::string&parentId,const EulerPoseDeg&pose,const RobotStateSnapshot&robot) {
    if(!asset)return core::Result<std::string>::failure("No workpiece asset");
    auto valid=validateWorkpieceAsset(*asset);if(!valid.ok)return core::Result<std::string>::failure(valid.error);
    if(state_.instances.size()>=50)return core::Result<std::string>::failure("Scene is limited to 50 workpiece instances");
    // Repeated imports share one immutable normalized asset, including across undo snapshots.
    for(const auto&i:state_.instances)if(i.asset->id==asset->id){if(!sameWorkpieceAssetContent(*i.asset,*asset))return core::Result<std::string>::failure("Asset ID conflicts with different geometry or source content");asset=i.asset;break;}
    std::set<const WorkpieceAsset*> retainedAssets;
    std::uint64_t retainedBytes=0;
    const auto retain=[&](const WorkpieceAssetPtr&a) {
        if(!a||!retainedAssets.insert(a.get()).second)return;
        retainedBytes+=a->sourceBytes.capacity();
        for(const auto&f:a->dependencies)retainedBytes+=f.bytes.capacity();
        for(const auto&c:a->components)for(const auto&m:c.meshes)retainedBytes+=m.vertices.capacity()*sizeof(WorkpieceVertex)+m.indices.capacity()*sizeof(std::uint32_t)+m.triangleFaceIds.capacity()*sizeof(std::uint32_t);
    };
    retain(asset);for(const auto&i:state_.instances)retain(i.asset);
    const std::size_t first=undo_.size()>19?undo_.size()-19:0;
    for(std::size_t n=first;n<undo_.size();++n)for(const auto&i:undo_[n].state.instances)retain(i.asset);
    if(retainedBytes>1536ULL*1024*1024)return core::Result<std::string>::failure("Workpiece and undo assets exceed the 1.5 GiB memory budget; save and reopen the scene before importing more assets");
    std::uint64_t visibleTriangles=workpieceTriangleCount(*asset),uniqueTriangles=visibleTriangles;
    std::set<std::string> assets;assets.insert(asset->id);
    for(const auto&i:state_.instances) {
        if(i.visible)visibleTriangles+=workpieceTriangleCount(*i.asset);
        if(assets.insert(i.asset->id).second)uniqueTriangles+=workpieceTriangleCount(*i.asset);
    }
    if(visibleTriangles>10000000||uniqueTriangles>10000000)return core::Result<std::string>::failure("Scene exceeds 10 million triangles");
    WorkpieceInstance instance;instance.asset=asset;instance.name=uniqueName(name.empty()?asset->name:name);instance.parentCoordinateId=parentId;instance.pose=pose;
    do {std::ostringstream id;id<<"workpiece-"<<nextId_++;instance.id=id.str();}while(find(instance.id));
    auto check=validateInstance(instance,robot);if(!check.ok)return core::Result<std::string>::failure(check.error);
    instance.fallbackWorld=workpieceWorldMatrix(instance,robot);
    remember();state_.instances.push_back(instance);state_.selectedId=instance.id;
    state_.robotId=robot.configuration?robot.configuration->id:std::string();state_.baseInWorld=robot.baseInWorld;
    changed();return core::Result<std::string>::success(instance.id);
}
core::Result<bool> WorkpieceService::update(const WorkpieceInstance&requested,const RobotStateSnapshot&robot) {
    const auto*old=find(requested.id);if(!old)return core::Result<bool>::failure("Unknown workpiece");
    if(old->asset!=requested.asset)return core::Result<bool>::failure("Asset replacement requires a new import");
    auto check=validateInstance(requested,robot);if(!check.ok)return check;
    if(old->locked&&(!samePose(old->pose,requested.pose)||old->parentCoordinateId!=requested.parentCoordinateId))
        return core::Result<bool>::failure("Unlock the workpiece before moving it");
    WorkpieceInstance candidate=requested;
    candidate.name=uniqueName(requested.name,requested.id);candidate.fallbackWorld=workpieceWorldMatrix(candidate,robot);
    if(sameInstance(*old,candidate))return core::Result<bool>::success(false);
    std::uint64_t visibleTriangles=0;for(const auto&i:state_.instances)if(i.id==candidate.id?candidate.visible:i.visible)visibleTriangles+=workpieceTriangleCount(*i.asset);
    if(visibleTriangles>10000000)return core::Result<bool>::failure("Visible scene exceeds 10 million triangles");
    remember();for(auto&i:state_.instances)if(i.id==candidate.id){i=candidate;break;}
    changed();return core::Result<bool>::success(true);
}
core::Result<bool> WorkpieceService::remove(const std::string&id) {
    const auto*old=find(id);if(!old)return core::Result<bool>::failure("Unknown workpiece");
    if(old->locked)return core::Result<bool>::failure("Unlock the workpiece before deleting it");
    remember();state_.instances.erase(std::remove_if(state_.instances.begin(),state_.instances.end(),[&](const WorkpieceInstance&i){return i.id==id;}),state_.instances.end());
    if(state_.selectedId==id)state_.selectedId.clear();changed();return core::Result<bool>::success(true);
}
bool WorkpieceService::select(const std::string&id) {
    if(!id.empty()&&!find(id))return false;
    if(state_.selectedId==id)return false;state_.selectedId=id;++state_.revision;return true;
}
bool WorkpieceService::undo() {
    if(undo_.empty())return false;History current={state_,contentVersion_};redo_.push_back(current);
    const auto old=undo_.back();undo_.pop_back();const auto revision=state_.revision;state_=old.state;contentVersion_=old.version;state_.revision=revision+1;return true;
}
bool WorkpieceService::redo() {
    if(redo_.empty())return false;History current={state_,contentVersion_};undo_.push_back(current);
    const auto next=redo_.back();redo_.pop_back();const auto revision=state_.revision;state_=next.state;contentVersion_=next.version;state_.revision=revision+1;return true;
}
core::Result<bool> WorkpieceService::replace(const WorkpieceSceneState&input,const RobotStateSnapshot&robot,bool keepWorldPositions) {
    if(input.instances.size()>50)return core::Result<bool>::failure("Scene is limited to 50 workpiece instances");
    WorkpieceSceneState candidate=input;std::set<std::string> ids,names;std::map<std::string,WorkpieceAssetPtr> assets;std::uint64_t uniqueTriangles=0,visibleTriangles=0;
    for(auto&i:candidate.instances) {
        if(!i.asset)return core::Result<bool>::failure("Scene has a missing asset");
        if(!ids.insert(i.id).second||!names.insert(folded(i.name)).second)return core::Result<bool>::failure("Scene has duplicated instance IDs or names");
        const auto asset=assets.insert(std::make_pair(i.asset->id,i.asset));
        if(asset.second) {
            auto valid=validateWorkpieceAsset(*i.asset);if(!valid.ok)return valid;uniqueTriangles+=workpieceTriangleCount(*i.asset);
        } else {
            if(!sameWorkpieceAssetContent(*asset.first->second,*i.asset))return core::Result<bool>::failure("Scene asset ID conflicts with different content");
            i.asset=asset.first->second;
        }
        if(i.visible)visibleTriangles+=workpieceTriangleCount(*i.asset);
        Matrix4d parent;
        if(keepWorldPositions||!parentWorldMatrix(i.parentCoordinateId,robot,parent)) {
            if(!finiteMatrix(i.fallbackWorld))return core::Result<bool>::failure("Stored world pose is invalid");
            if(!parentWorldMatrix(i.parentCoordinateId,robot,parent)) {
                i.parentCoordinateId="world";i.warning="Original parent coordinate is unavailable; preserved world placement";parent=Matrix4d();
            }
            i.pose=matrixPose(multiply(rigidInverse(parent),i.fallbackWorld));
        }
        auto valid=validateInstance(i,robot);if(!valid.ok)return valid;
        i.fallbackWorld=workpieceWorldMatrix(i,robot);
    }
    if(uniqueTriangles>10000000||visibleTriangles>10000000)return core::Result<bool>::failure("Scene exceeds 10 million triangles");
    if(!candidate.selectedId.empty()&&!ids.count(candidate.selectedId))candidate.selectedId.clear();
    candidate.robotId=robot.configuration?robot.configuration->id:std::string();candidate.baseInWorld=robot.baseInWorld;
    const auto revision=state_.revision;state_=candidate;state_.revision=revision+1;undo_.clear();redo_.clear();contentVersion_=nextVersion_++;savedVersion_=contentVersion_;
    return core::Result<bool>::success(true);
}
bool WorkpieceService::syncRobotContext(const RobotStateSnapshot&robot) {
    bool lost=false;
    for(const auto&i:state_.instances){Matrix4d parent;if(!parentWorldMatrix(i.parentCoordinateId,robot,parent)){lost=true;break;}}
    if(lost)remember();
    for(auto&i:state_.instances) {
        Matrix4d parent;
        if(!parentWorldMatrix(i.parentCoordinateId,robot,parent)) {i.parentCoordinateId="world";i.pose=matrixPose(i.fallbackWorld);i.warning="Original parent coordinate is unavailable; preserved world placement";}
        else i.fallbackWorld=multiply(parent,poseMatrix(i.pose));
    }
    state_.robotId=robot.configuration?robot.configuration->id:std::string();state_.baseInWorld=robot.baseInWorld;
    if(lost)changed();return lost;
}
} }
