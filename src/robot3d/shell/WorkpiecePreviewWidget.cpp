#include "robot3d/shell/WorkpiecePreviewWidget.h"
#include <QOpenGLFunctions_2_1>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QQuaternion>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <set>
#include <map>
#include <limits>
#include <functional>
namespace {
class PreviewOverlay : public QWidget {
public:
    std::function<void(QPainter &)> draw;
    explicit PreviewOverlay(QWidget *parent):QWidget(parent){setAttribute(Qt::WA_TransparentForMouseEvents);setAttribute(Qt::WA_NoSystemBackground);setAttribute(Qt::WA_TranslucentBackground);}
protected:
    void paintEvent(QPaintEvent *) override{QPainter painter(this);if(draw)draw(painter);}
};
struct Vertex {float p[3],n[3],c[3];};
struct Batch {
    QOpenGLBuffer vb,ib; int count=0;std::string componentId;
    Batch():vb(QOpenGLBuffer::VertexBuffer),ib(QOpenGLBuffer::IndexBuffer){}
};
}
struct WorkpiecePreviewWidget::Impl {
    robot3d::domain::WorkpieceAssetPtr asset;
    std::unique_ptr<QOpenGLShaderProgram> shader;
    std::vector<std::unique_ptr<Batch>> batches;
    bool pending=true,ready=false,doubleSided=true,bounds=true,uniform=false;
    int mode=2;std::set<std::string> highlighted;
    QColor color=QColor(165,180,200);
    QPoint last;QQuaternion rotation;QVector3D center,pan;
    float radius=100,distance=400;
    PreviewOverlay *overlay=nullptr;QMatrix4x4 mvp;
};
WorkpiecePreviewWidget::WorkpiecePreviewWidget(QWidget *parent):QOpenGLWidget(parent),d_(new Impl) {
    setObjectName("workpiecePreview");setMinimumSize(320,260);setFocusPolicy(Qt::StrongFocus);
    setToolTip(QString::fromUtf8("左键旋转；中键或 Shift+左键平移；滚轮缩放。观察不改变工件坐标。"));
    d_->overlay=new PreviewOverlay(this);d_->overlay->setObjectName("workpiecePreviewOverlay");d_->overlay->draw=[this](QPainter &p){drawOverlay(p);};d_->overlay->setGeometry(rect());d_->overlay->raise();
}
WorkpiecePreviewWidget::~WorkpiecePreviewWidget(){if(context()){disconnect(context(),nullptr,this,nullptr);makeCurrent();release();doneCurrent();}}
void WorkpiecePreviewWidget::release(){d_->batches.clear();d_->shader.reset();}
void WorkpiecePreviewWidget::setAsset(robot3d::domain::WorkpieceAssetPtr a){
    d_->asset=std::move(a);d_->pending=true;d_->ready=false;d_->highlighted.clear();
    if(d_->asset){auto a=d_->asset->boundsMin,b=d_->asset->boundsMax;
        d_->center=QVector3D(float((a.x+b.x)/2),float((a.y+b.y)/2),float((a.z+b.z)/2));
        d_->radius=std::max(.001f,float(std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)+(b.z-a.z)*(b.z-a.z))/2));}
    resetCamera();update();
}
bool WorkpiecePreviewWidget::ready()const{return d_->ready;}
void WorkpiecePreviewWidget::focusComponent(const QString &id){
    if(!d_->asset)return;std::string target=id.toUtf8().toStdString();
    std::map<std::string,std::string> parents;for(const auto &c:d_->asset->components)parents[c.id]=c.parentId;
    d_->highlighted.clear();double inf=std::numeric_limits<double>::max();
    robot3d::domain::Vec3d low(inf,inf,inf),high(-inf,-inf,-inf);bool found=false;
    for(const auto &c:d_->asset->components){
        std::string current=c.id;bool inside=false;
        for(std::size_t i=0;i<=parents.size()&&!current.empty();++i){if(current==target){inside=true;break;}auto p=parents.find(current);if(p==parents.end())break;current=p->second;}
        if(!inside||!c.selected)continue;d_->highlighted.insert(c.id);
        for(const auto &mesh:c.meshes)for(const auto &v:mesh.vertices){auto p=robot3d::domain::transformPoint(c.transform,v.position);low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);low.z=std::min(low.z,p.z);high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);high.z=std::max(high.z,p.z);found=true;}
    }
    if(found){d_->center=QVector3D(float((low.x+high.x)/2),float((low.y+high.y)/2),float((low.z+high.z)/2));d_->radius=std::max(.001f,float(std::sqrt((high.x-low.x)*(high.x-low.x)+(high.y-low.y)*(high.y-low.y)+(high.z-low.z)*(high.z-low.z))/2));d_->distance=d_->radius*3.2f;d_->pan=QVector3D();}
    update();
}
void WorkpiecePreviewWidget::setDisplayMode(int m){d_->mode=m;update();}
void WorkpiecePreviewWidget::setUniformColor(bool u,const QColor &c){d_->uniform=u;d_->color=c;update();}
void WorkpiecePreviewWidget::setDoubleSided(bool v){d_->doubleSided=v;update();}
void WorkpiecePreviewWidget::setBoundsVisible(bool v){d_->bounds=v;update();}
void WorkpiecePreviewWidget::resetCamera(){
    if(d_->asset){auto a=d_->asset->boundsMin,b=d_->asset->boundsMax;d_->center=QVector3D(float((a.x+b.x)/2),float((a.y+b.y)/2),float((a.z+b.z)/2));d_->radius=std::max(.001f,float(std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)+(b.z-a.z)*(b.z-a.z))/2));}d_->highlighted.clear();standardView(0);
}
void WorkpiecePreviewWidget::standardView(int i){
    QVector3D dir(1,-1,.8f),up(0,0,1);
    if(i==1)dir=QVector3D(1,0,0);if(i==2)dir=QVector3D(-1,0,0);
    if(i==3)dir=QVector3D(0,-1,0);if(i==4)dir=QVector3D(0,1,0);
    if(i==5){dir=QVector3D(0,0,1);up=QVector3D(0,1,0);}if(i==6){dir=QVector3D(0,0,-1);up=QVector3D(0,1,0);}
    QMatrix4x4 v;v.lookAt(dir.normalized(),QVector3D(),up);d_->rotation=QQuaternion::fromRotationMatrix(v.normalMatrix());
    d_->pan=QVector3D();d_->distance=d_->radius*3.2f;update();
}
void WorkpiecePreviewWidget::initializeGL(){
    auto *f=context()->versionFunctions<QOpenGLFunctions_2_1>();
    if(!f||!f->initializeOpenGLFunctions()){emit renderError(QString::fromUtf8("预览需要桌面 OpenGL 兼容上下文"));return;}
    d_->shader.reset(new QOpenGLShaderProgram);
    const char *vs="#version 120\nattribute vec3 pos;attribute vec3 normal;attribute vec3 color;uniform mat4 mvp;uniform mat3 nmat;varying vec3 n;varying vec3 c;void main(){gl_Position=mvp*vec4(pos,1.);n=nmat*normal;c=color;}";
    const char *fs="#version 120\nvarying vec3 n;varying vec3 c;uniform vec3 overrideColor;uniform int useOverride;uniform int unlit;void main(){vec3 col=useOverride!=0?overrideColor:c;float l=unlit!=0?1.:.3+.7*abs(dot(normalize(n),normalize(vec3(.4,.5,1.))));gl_FragColor=vec4(col*l,1.);}";
    if(!d_->shader->addShaderFromSourceCode(QOpenGLShader::Vertex,vs)||!d_->shader->addShaderFromSourceCode(QOpenGLShader::Fragment,fs)||!d_->shader->link()){emit renderError(d_->shader->log());d_->shader.reset();return;}
    connect(context(),&QOpenGLContext::aboutToBeDestroyed,this,[this]{makeCurrent();release();doneCurrent();d_->pending=true;d_->ready=false;},Qt::DirectConnection);d_->pending=true;
}
void WorkpiecePreviewWidget::paintGL(){
    auto *f=context()->versionFunctions<QOpenGLFunctions_2_1>();if(!f)return;
    f->glViewport(0,0,int(width()*devicePixelRatioF()),int(height()*devicePixelRatioF()));f->glClearColor(.94f,.96f,.98f,1);f->glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(!d_->shader){d_->overlay->update();return;}
    if(d_->pending){d_->pending=false;d_->batches.clear();try{
        if(d_->asset)for(const auto &component:d_->asset->components){if(!component.selected)continue;for(const auto &mesh:component.meshes){if(mesh.indices.empty())continue;
            std::vector<Vertex> data;data.reserve(mesh.vertices.size());for(const auto &v:mesh.vertices){
                auto p=robot3d::domain::transformPoint(component.transform,v.position),n=robot3d::domain::transformDirection(component.transform,v.normal);
                Vertex out={{float(p.x),float(p.y),float(p.z)},{float(n.x),float(n.y),float(n.z)},{v.color[0],v.color[1],v.color[2]}};data.push_back(out);}
            std::unique_ptr<Batch>b(new Batch);b->componentId=component.id;if(!b->vb.create()||!b->ib.create())throw std::runtime_error("Cannot allocate preview buffers");
            b->vb.bind();b->vb.allocate(data.data(),int(data.size()*sizeof(Vertex)));b->vb.release();b->ib.bind();b->ib.allocate(mesh.indices.data(),int(mesh.indices.size()*sizeof(std::uint32_t)));b->ib.release();b->count=int(mesh.indices.size());d_->batches.push_back(std::move(b));
        }}if(f->glGetError()!=GL_NO_ERROR)throw std::runtime_error("Preview GPU allocation failed");
        d_->ready=d_->asset&&!d_->batches.empty();if(d_->ready)emit assetReady();
    }catch(const std::exception &e){d_->ready=false;d_->batches.clear();emit renderError(QString::fromUtf8(e.what()));}}
    if(!d_->asset||!d_->ready){d_->overlay->update();return;}
    f->glEnable(GL_DEPTH_TEST);if(d_->doubleSided)f->glDisable(GL_CULL_FACE);else{f->glEnable(GL_CULL_FACE);f->glCullFace(GL_BACK);}
    QMatrix4x4 projection,view,model;projection.perspective(45,float(width())/std::max(1,height()),std::max(.00001f,d_->radius*.0001f),std::max(d_->distance+d_->radius*10,d_->radius*20));
    view.translate(d_->pan.x(),d_->pan.y(),-d_->distance);view.rotate(d_->rotation);model.translate(-d_->center);auto mvp=projection*view*model;d_->mvp=mvp;
    d_->shader->bind();d_->shader->setUniformValue("mvp",mvp);d_->shader->setUniformValue("nmat",view.normalMatrix());
    auto draw=[this,f](bool lines){f->glPolygonMode(GL_FRONT_AND_BACK,lines?GL_LINE:GL_FILL);d_->shader->setUniformValue("useOverride",int(lines||d_->uniform));QColor col=lines?QColor(55,70,90):d_->color;
        d_->shader->setUniformValue("overrideColor",QVector3D(float(col.redF()),float(col.greenF()),float(col.blueF())));d_->shader->setUniformValue("unlit",int(lines));
        int p=d_->shader->attributeLocation("pos"),n=d_->shader->attributeLocation("normal"),c=d_->shader->attributeLocation("color");
        for(const auto &b:d_->batches){bool highlighted=d_->highlighted.count(b->componentId)!=0;
            d_->shader->setUniformValue("useOverride",int(lines||d_->uniform||highlighted));
            QColor part=highlighted?QColor(70,150,245):col;
            d_->shader->setUniformValue("overrideColor",QVector3D(float(part.redF()),float(part.greenF()),float(part.blueF())));
            b->vb.bind();b->ib.bind();d_->shader->enableAttributeArray(p);d_->shader->enableAttributeArray(n);d_->shader->enableAttributeArray(c);
            d_->shader->setAttributeBuffer(p,GL_FLOAT,0,3,sizeof(Vertex));d_->shader->setAttributeBuffer(n,GL_FLOAT,3*sizeof(float),3,sizeof(Vertex));d_->shader->setAttributeBuffer(c,GL_FLOAT,6*sizeof(float),3,sizeof(Vertex));
            f->glDrawElements(GL_TRIANGLES,b->count,GL_UNSIGNED_INT,nullptr);d_->shader->disableAttributeArray(p);d_->shader->disableAttributeArray(n);d_->shader->disableAttributeArray(c);b->ib.release();b->vb.release();}};
    if(d_->mode!=1){if(d_->mode==2){f->glEnable(GL_POLYGON_OFFSET_FILL);f->glPolygonOffset(1,1);}draw(false);f->glDisable(GL_POLYGON_OFFSET_FILL);}if(d_->mode!=0)draw(true);
    f->glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);d_->shader->release();f->glDisable(GL_CULL_FACE);f->glDisable(GL_DEPTH_TEST);d_->overlay->update();
}
void WorkpiecePreviewWidget::resizeGL(int,int){d_->overlay->setGeometry(rect());}
void WorkpiecePreviewWidget::drawOverlay(QPainter &p){
    if(!d_->asset||!d_->ready){p.setPen(QColor(90,105,125));p.drawText(rect(),Qt::AlignCenter,QString::fromUtf8("选择文件并加载预览"));return;}
    const auto &mvp=d_->mvp;p.setRenderHint(QPainter::Antialiasing);p.setPen(QColor(45,65,85));p.drawText(12,22,QString::fromUtf8("工件局部坐标 · mm"));auto a=d_->asset->boundsMin,b=d_->asset->boundsMax;
    if(d_->bounds){p.drawText(12,height()-12,QString::fromUtf8("尺寸：%1 × %2 × %3 mm").arg(b.x-a.x,0,'g',7).arg(b.y-a.y,0,'g',7).arg(b.z-a.z,0,'g',7));
        QPointF pts[8];for(int i=0;i<8;++i){auto q=mvp*QVector4D(float(i&1?b.x:a.x),float(i&2?b.y:a.y),float(i&4?b.z:a.z),1);pts[i]=QPointF((q.x()/q.w()+1)*width()/2,(1-q.y()/q.w())*height()/2);}
        p.setPen(QPen(QColor(120,145,165,170),1,Qt::DashLine));for(int i=0;i<8;++i)for(int j=0;j<3;++j)if(!(i&(1<<j)))p.drawLine(pts[i],pts[i|(1<<j)]);}
    QPointF origin(width()-55,height()-55);QVector3D axes[]={QVector3D(1,0,0),QVector3D(0,1,0),QVector3D(0,0,1)};QColor colors[]={QColor(205,60,60),QColor(40,155,85),QColor(55,90,210)};
    for(int i=0;i<3;++i){auto v=d_->rotation.rotatedVector(axes[i]);auto tip=origin+QPointF(v.x()*30,-v.y()*30);p.setPen(QPen(colors[i],2));p.drawLine(origin,tip);p.drawText(tip+QPointF(3,0),QString("XYZ").mid(i,1));}
}
void WorkpiecePreviewWidget::mousePressEvent(QMouseEvent *e){d_->last=e->pos();setFocus();}
void WorkpiecePreviewWidget::mouseMoveEvent(QMouseEvent *e){QPoint delta=e->pos()-d_->last;d_->last=e->pos();
    if((e->buttons()&Qt::MiddleButton)||((e->buttons()&Qt::LeftButton)&&(e->modifiers()&Qt::ShiftModifier))){float s=2*d_->distance*.4142f/std::max(1,height());d_->pan+=QVector3D(delta.x()*s,-delta.y()*s,0);}
    else if(e->buttons()&Qt::LeftButton){auto q=QQuaternion::fromAxisAndAngle(QVector3D(delta.y(),delta.x(),0),float(std::sqrt(double(delta.x()*delta.x()+delta.y()*delta.y())))*.5f);d_->rotation=(q*d_->rotation).normalized();}update();}
void WorkpiecePreviewWidget::wheelEvent(QWheelEvent *e){d_->distance=std::max(d_->radius*.02f,std::min(d_->radius*1000,d_->distance*float(std::pow(.85,e->angleDelta().y()/120.))));update();e->accept();}
