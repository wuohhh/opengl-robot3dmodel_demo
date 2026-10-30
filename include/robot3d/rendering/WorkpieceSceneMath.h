#pragma once
#include "robot3d/scene/RobotSceneSnapshot.h"
#include "robot3d/rendering/CameraState.h"
#include "robot3d/rendering/SceneUnits.h"
#include <QMatrix4x4>
#include <QPointF>
#include <QSize>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
namespace robot3d { namespace rendering { namespace workpieceqt {
inline QMatrix4x4 matrix(const domain::Matrix4d &value, bool scaledTranslation = false)
{
    QMatrix4x4 m;
    for (int r=0;r<4;++r) for (int c=0;c<4;++c)
        m(r,c)=float(value.values[std::size_t(r*4+c)]);
    if (scaledTranslation) for(int r=0;r<3;++r) m(r,3)*=float(kMillimetersToModelUnits);
    return m;
}
inline domain::Matrix4d baseMatrix(const scene::RobotSceneSnapshot &s, const domain::WorkpieceInstance &i)
{
    return domain::multiply(domain::rigidInverse(domain::poseMatrix(s.state.baseInWorld)),
                            domain::workpieceWorldMatrix(i,s.state));
}
inline QMatrix4x4 coordinate(const scene::RobotSceneSnapshot &s)
{
    QMatrix4x4 m;
    if (s.options.install) return m;
    const auto &p=s.coordinatePose;
    const QVector3D axis(float(p.Rx),float(p.Ry),float(p.Rz));
    if(axis.length()>1e-8f) m.rotate(axis.length(),axis.normalized());
    m.setColumn(3,QVector4D(float(p.X*kMillimetersToModelUnits),float(p.Y*kMillimetersToModelUnits),
                           float(p.Z*kMillimetersToModelUnits),1));
    return m;
}
inline QMatrix4x4 root(const scene::RobotSceneSnapshot &s,const CameraState &camera)
{
    const QMatrix4x4 coord=coordinate(s);
    QQuaternion rotation=camera.rotation;
    if(s.options.install) rotation=rotation*QQuaternion::fromEulerAngles(0,-s.installationTiltDeg,-s.installationBaseDeg);
    QMatrix4x4 m;
    if(camera.focusOverride)
    {
        const QVector3D pivot=coord.inverted().map(camera.focusPoint);
        m.translate(camera.translation);
        m.translate(pivot); m.scale(float(camera.scale)); m.rotate(rotation); m.translate(-pivot);
        return m*coord.inverted();
    }
    m.scale(float(camera.scale));
    m=coord.inverted()*m;
    m.translate(QVector3D(coord*QVector4D(camera.translation,0))/float(camera.scale));
    m.translate(camera.rotationCenter); m.rotate(rotation); m.translate(-camera.rotationCenter);
    return m;
}
inline QMatrix4x4 view(const scene::RobotSceneSnapshot &s,const CameraState &camera,
                      const QVector3D &defaultEye,const QVector3D &defaultFocus)
{
    QMatrix4x4 m;
    if(camera.focusOverride)
    {
        const QVector3D focus=coordinate(s).inverted().map(camera.focusPoint);
        m.lookAt(focus+QVector3D(float(camera.viewDistance),0,0),focus,QVector3D(0,0,1));
    }
    else m.lookAt(defaultEye,defaultFocus,QVector3D(0,0,1));
    return m;
}
inline QMatrix4x4 projection(const CameraState &camera,double aspect)
{
    QMatrix4x4 p;
    if(camera.focusOverride)
    {
        const double radius=std::max(0.01,camera.sceneRadius*std::max(1.0,camera.scale));
        const double distance=std::max(0.02,camera.viewDistance);
        p.perspective(45,float(aspect),float(std::max(0.001,distance-radius*2.5)),
                      float(std::max(distance+radius*4.0,distance+1.0)));
    }
    else p.perspective(45,float(aspect),30,400);
    return p;
}

inline std::pair<double,double> clipRange(const scene::RobotSceneSnapshot &s,const CameraState &camera,
                                          const QVector3D &eye,const QVector3D &target)
{
    if(camera.focusOverride)
    {
        const double r=std::max(0.01,camera.sceneRadius*std::max(1.0,camera.scale));
        return std::make_pair(std::max(0.001,camera.viewDistance-r*2.5),std::max(camera.viewDistance+r*4.0,camera.viewDistance+1.0));
    }
    double nearPlane=30,farPlane=400;bool behind=false,front=false;
    const auto clipView=view(s,camera,eye,target)*root(s,camera);
    for(const auto &i:s.workpieces)
    {
        if(!i.visible||!i.asset)continue;
        const auto frame=baseMatrix(s,i);
        for(int k=0;k<8;++k)
        {
            const auto &a=*i.asset;
            auto p=domain::transformPoint(frame,domain::Vec3d((k&1)?a.boundsMax.x:a.boundsMin.x,
                (k&2)?a.boundsMax.y:a.boundsMin.y,(k&4)?a.boundsMax.z:a.boundsMin.z));
            const auto q=clipView.map(QVector3D(float(p.x*kMillimetersToModelUnits),float(p.y*kMillimetersToModelUnits),float(p.z*kMillimetersToModelUnits)));
            const double depth=-q.z();
            if(depth<=0){behind=true;continue;}
            front=true;nearPlane=std::min(nearPlane,std::max(0.001,depth*0.5));farPlane=std::max(farPlane,depth*1.5);
        }
    }
    if(front&&behind)nearPlane=0.001;
    return std::make_pair(nearPlane,farPlane);
}
inline QMatrix4x4 sceneProjection(const scene::RobotSceneSnapshot &s,const CameraState &camera,double aspect,
                                  const QVector3D &eye,const QVector3D &target)
{
    const auto range=clipRange(s,camera,eye,target);
    QMatrix4x4 p;p.perspective(45,float(aspect),float(range.first),float(range.second));return p;
}

inline bool bounds(const scene::RobotSceneSnapshot &s,const std::string &id,bool includeRobot,
                   QVector3D &lo,QVector3D &hi)
{
    const float inf=std::numeric_limits<float>::max();
    lo=QVector3D(inf,inf,inf);hi=-lo;bool any=false;
    const auto add=[&](const QVector3D &p){ for(int k=0;k<3;++k){lo[k]=std::min(lo[k],p[k]);hi[k]=std::max(hi[k],p[k]);} any=true; };
    if(includeRobot && s.state.configuration)
    {
        double reach=0;
        for(const auto &d:s.state.configuration->dh) reach+=std::abs(d.a)+std::abs(d.d);
        const float r=float(std::max(100.0,reach)*kMillimetersToModelUnits);
        add(QVector3D(-r,-r,-r));add(QVector3D(r,r,r));
    }
    for(const auto &i:s.workpieces)
    {
        if(!i.asset || !i.visible || (!id.empty()&&i.id!=id)) continue;
        const auto frame=baseMatrix(s,i);
        const auto &a=*i.asset;
        for(int corner=0;corner<8;++corner)
        {
            domain::Vec3d p((corner&1)?a.boundsMax.x:a.boundsMin.x,
                            (corner&2)?a.boundsMax.y:a.boundsMin.y,
                            (corner&4)?a.boundsMax.z:a.boundsMin.z);
            p=domain::transformPoint(frame,p);
            add(QVector3D(float(p.x*kMillimetersToModelUnits),float(p.y*kMillimetersToModelUnits),float(p.z*kMillimetersToModelUnits)));
        }
    }
    return any;
}
inline bool focus(const scene::RobotSceneSnapshot &s,const std::string &id,bool all,double aspect,CameraState &camera)
{
    QVector3D lo,hi;if(!bounds(s,id,all,lo,hi))return false;
    camera.focusOverride=true;camera.focusPoint=(lo+hi)*0.5f;camera.rotationCenter=camera.focusPoint;
    camera.translation=QVector3D();camera.rotation=QQuaternion();camera.scale=1;
    camera.sceneRadius=std::max(0.05,double((hi-lo).length())*0.5);
    const double angle=22.5*3.14159265358979323846/180.0;
    camera.viewDistance=camera.sceneRadius/std::sin(angle)*1.15/std::min(1.0,std::max(0.1,aspect));
    return true;
}
inline bool hitTriangle(const QVector3D &origin,const QVector3D &dir,const domain::Vec3d &ap,
                        const domain::Vec3d &bp,const domain::Vec3d &cp,double &t)
{
    const QVector3D a(float(ap.x),float(ap.y),float(ap.z)),b(float(bp.x),float(bp.y),float(bp.z)),c(float(cp.x),float(cp.y),float(cp.z));
    const QVector3D e1=b-a,e2=c-a,h=QVector3D::crossProduct(dir,e2);
    const double det=QVector3D::dotProduct(e1,h);
    if(std::abs(det)<1e-12)return false;
    const double inv=1.0/det;
    const QVector3D delta=origin-a;
    const double u=inv*QVector3D::dotProduct(delta,h);if(u<0||u>1)return false;
    const QVector3D q=QVector3D::crossProduct(delta,e1);
    const double v=inv*QVector3D::dotProduct(dir,q);if(v<0||u+v>1)return false;
    t=inv*QVector3D::dotProduct(e2,q);return t>=0;
}
inline std::string pick(const scene::RobotSceneSnapshot &s,const CameraState &camera,
                        const QMatrix4x4 &projectionMatrix,const QMatrix4x4 &viewMatrix,
                        const QSize &size,const QPointF &pixel)
{
    if(size.width()<1||size.height()<1)return std::string();
    bool ok=false;const auto inverse=(projectionMatrix*viewMatrix*root(s,camera)).inverted(&ok);
    if(!ok)return std::string();
    const float x=float(2*pixel.x()/size.width()-1),y=float(1-2*pixel.y()/size.height());
    QVector4D a=inverse*QVector4D(x,y,-1,1),b=inverse*QVector4D(x,y,1,1);
    if(std::abs(a.w())<1e-12||std::abs(b.w())<1e-12)return std::string();
    const QVector3D from=a.toVector3DAffine()/float(kMillimetersToModelUnits);
    const QVector3D to=b.toVector3DAffine()/float(kMillimetersToModelUnits);
    const QVector3D direction=to-from;
    double nearest=std::numeric_limits<double>::max();std::string id;
    for(const auto &i:s.workpieces)
    {
        if(!i.visible||!i.asset)continue;
        const auto frame=baseMatrix(s,i);
        for(const auto &component:i.asset->components)
        {
            if(!component.selected)continue;
            bool invertible=false;const auto local=matrix(domain::multiply(frame,component.transform)).inverted(&invertible);
            if(!invertible)continue;
            const QVector3D o=local.map(from),d=local.mapVector(direction);
            for(const auto &mesh:component.meshes)
                for(std::size_t n=0;n+2<mesh.indices.size();n+=3)
                {
                    const auto i0=mesh.indices[n],i1=mesh.indices[n+1],i2=mesh.indices[n+2];
                    if(i0>=mesh.vertices.size()||i1>=mesh.vertices.size()||i2>=mesh.vertices.size())continue;
                    double t;
                    if(hitTriangle(o,d,mesh.vertices[i0].position,mesh.vertices[i1].position,mesh.vertices[i2].position,t)&&t<nearest)
                    {nearest=t;id=i.id;}
                }
        }
    }
    return id;
}
} } }
