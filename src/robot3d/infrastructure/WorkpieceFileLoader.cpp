#include "robot3d/infrastructure/WorkpieceFileLoader.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>
#include <QByteArray>
#include <QStringList>
#include "assimp/cimport.h"
#include "assimp/cfileio.h"
#include <exception>
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/material.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <sstream>
#ifdef ROBOT3D_WITH_OCCT
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_ExternFile.hxx>
#include <STEPConstruct_ExternRefs.hxx>
#include <XSControl_WorkSession.hxx>
#include <TCollection_HAsciiString.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <TDocStd_Document.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDF_Tool.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <IMeshTools_Parameters.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <Interface_Static.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <Message_ProgressIndicator.hxx>
#include <TColStd_SequenceOfAsciiString.hxx>
#include <Quantity_Color.hxx>
#include <Precision.hxx>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <thread>
#endif
#endif

namespace robot3d { namespace infrastructure {
namespace {
using namespace domain;
QString utf8(const std::string &s) { return QString::fromUtf8(s.data(), int(s.size())); }
std::string bytes(const QString &s) { const auto b=s.toUtf8(); return std::string(b.constData(),std::size_t(b.size())); }
struct LoadFailure : std::runtime_error {
    explicit LoadFailure(const QString &s) : std::runtime_error(bytes(s)) {}
};
void checkCancelled(const WorkpieceLoadOptions &o) {
    if(o.cancellation && o.cancellation->load()) throw LoadFailure(QString::fromUtf8("加载已取消"));
}
void progress(const WorkpieceLoadOptions &o,const QString &s,int p=-1) {
    checkCancelled(o); if(o.progress)o.progress(s,p);
}
bool finite(const Vec3d &v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3d minus(const Vec3d&a,const Vec3d&b){return Vec3d(a.x-b.x,a.y-b.y,a.z-b.z);}
Vec3d cross(const Vec3d&a,const Vec3d&b){return Vec3d(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
Vec3d normal(const Vec3d&v){double n=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return n>1e-30?Vec3d(v.x/n,v.y/n,v.z/n):Vec3d(0,0,1);}
QString detectFormat(const QByteArray &data,const QString &suffix) {
    QByteArray t=data.left(4096).trimmed();
    if(t.startsWith("\xef\xbb\xbf"))t=t.mid(3).trimmed();
    if(t.startsWith("ISO-10303-21"))return "STEP";
    if(t.startsWith("ply\n")||t.startsWith("ply\r"))return "PLY";
    if(data.size()>=80 && data[72]=='S')return "IGES";
    if(data.size()>=84) {
        const auto *b=reinterpret_cast<const unsigned char*>(data.constData()+80);
        const std::uint64_t n=std::uint64_t(b[0])|(std::uint64_t(b[1])<<8)|(std::uint64_t(b[2])<<16)|(std::uint64_t(b[3])<<24);
        if(n>0 && 84+n*50==std::uint64_t(data.size()))return "STL";
    }
    if(t.startsWith("solid") && (t.contains("facet")||data.contains("facet normal")))return "STL";
    if(suffix=="obj" && (QRegularExpression("(^|[\\r\\n])\\s*v\\s+").match(QString::fromUtf8(data.left(65536))).hasMatch()))return "OBJ";
    if(suffix=="step"||suffix=="stp")return "STEP";
    if(suffix=="iges"||suffix=="igs")return "IGES";
    if(suffix=="stl")return "STL";
    if(suffix=="ply")return "PLY";
    throw LoadFailure(QString::fromUtf8("不支持或无法识别的模型格式。支持 STEP、IGES、STL、OBJ、含面的 PLY。"));
}
class Snapshot {
public:
    Snapshot(const QString &p,const WorkpieceLoadOptions &o,WorkpieceAsset &a)
        :main(QDir::cleanPath(QFileInfo(p).absoluteFilePath())),base(QFileInfo(p).absolutePath()),options(o),asset(a){}
    Snapshot(const WorkpieceAsset &stored,const WorkpieceLoadOptions &o,WorkpieceAsset &a)
        :Snapshot(stored.sourcePath.empty()?QDir::root().filePath("__robot3d_source_snapshot__/model."+utf8(stored.format).toLower()):utf8(stored.sourcePath),o,a){
        memoryOnly=true;
        const auto capture=[&](const std::vector<std::uint8_t> &value,const QString &path){
            checkCancelled(options);
            if(value.size()>options.maxFileBytes||value.size()>std::size_t((std::numeric_limits<int>::max)())||
               total+std::uint64_t(value.size())>options.maxDependencyBytes)
                throw LoadFailure(QString::fromUtf8("源快照或依赖超出容量限制：")+path);
            QByteArray data(reinterpret_cast<const char*>(value.data()),int(value.size()));
            if(data.size()>=3&&static_cast<unsigned char>(data[0])==0x88&&static_cast<unsigned char>(data[1])==0x7d&&static_cast<unsigned char>(data[2])==0x1c)
                throw LoadFailure(QString::fromUtf8("源快照包含无法解析的加密数据：")+path);
            const auto inserted=cache.insert(std::make_pair(path,data));
            if(!inserted.second&&inserted.first->second!=data)throw LoadFailure(QString::fromUtf8("源快照路径指向冲突内容：")+path);
            if(inserted.second)total+=std::uint64_t(value.size());
            return data;
        };
        capture(stored.sourceBytes,main);
        if(stored.dependencies.size()>10000)throw LoadFailure(QString::fromUtf8("源快照依赖数量超限"));
        for(const auto &dependency:stored.dependencies){
            const QString path=resolve(dependency.relativePath.c_str());
            const QByteArray data=capture(dependency.bytes,path);
            if(dependency.sha256!=QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex().toStdString())
                throw LoadFailure(QString::fromUtf8("源依赖快照 SHA-256 校验失败：")+utf8(dependency.relativePath));
        }
        asset.dependencies=stored.dependencies;
    }
    bool exists(const QString &path)const {
        const QString full=QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        return cache.count(full)!=0||(!memoryOnly&&QFileInfo(full).isFile());
    }
    QByteArray read(const QString &path,bool required) {
        checkCancelled(options);
        const QString full=QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        auto found=cache.find(full);
        if(found!=cache.end())return found->second;
        if(memoryOnly){
            if(required)throw LoadFailure(QString::fromUtf8("自包含源快照缺少必要依赖：")+full);
            return QByteArray();
        }
        QFileInfo before(full); QFile file(full);
        if(!before.isFile() || !file.open(QIODevice::ReadOnly)) {
            if(required)throw LoadFailure(QString::fromUtf8("无法读取模型或几何依赖：")+full+"; "+file.errorString());
            return QByteArray();
        }
        const qint64 size=before.size();
        if(size<0 || std::uint64_t(size)>options.maxFileBytes || total+std::uint64_t(size)>options.maxDependencyBytes)
            throw LoadFailure(QString::fromUtf8("文件或依赖总量超出容量限制：")+full);
        QByteArray value; value.reserve(int(size));
        while(!file.atEnd()){
            checkCancelled(options);
            QByteArray part=file.read(1024*1024);
            if(part.isEmpty() && file.error()!=QFile::NoError)throw LoadFailure(QString::fromUtf8("读取失败：")+full);
            value.append(part);
            if(std::uint64_t(value.size())>options.maxFileBytes)throw LoadFailure(QString::fromUtf8("文件大小超限：")+full);
        }
        QFileInfo after(full);
        if(value.size()!=size||after.size()!=size||after.lastModified()!=before.lastModified())
            throw LoadFailure(QString::fromUtf8("文件在读取期间发生变化，请重新加载：")+full);
        if(value.size()>=3 && static_cast<unsigned char>(value[0])==0x88 && static_cast<unsigned char>(value[1])==0x7d && static_cast<unsigned char>(value[2])==0x1c)
            throw LoadFailure(QString::fromUtf8("检测到透明加密文件，当前应用进程没有读取明文权限。请将应用加入企业允许的读取进程后重试：")+full);
        total+=std::uint64_t(value.size());cache[full]=value;
        if(full!=main){
            WorkpieceSourceFile dep;
            dep.relativePath=bytes(QDir(base).relativeFilePath(full));
            dep.bytes.assign(value.begin(),value.end());
            dep.sha256=QCryptographicHash::hash(value,QCryptographicHash::Sha256).toHex().toStdString();
            asset.dependencies.push_back(std::move(dep));
        }
        return value;
    }
    QString resolve(const char *p)const {
        QString q=QString::fromUtf8(p).replace('\\','/');
        if(QFileInfo(q).isAbsolute())return QDir::cleanPath(q);
        return QDir::cleanPath(QDir(base).absoluteFilePath(q));
    }
    QString main,base;
    const WorkpieceLoadOptions &options; WorkpieceAsset &asset;
    std::map<QString,QByteArray> cache;
    std::uint64_t total=0;bool memoryOnly=false;
};
// Only POD aiFileIO/aiFile structures cross the Release-only Assimp DLL ABI.
// Its C++ IOSystem owns std::vector<std::string> and directory methods carrying
// std::string references, which are incompatible with /MDd iterator-debug builds.
class SnapshotIO {
    struct File {
        aiFile api={};SnapshotIO *owner;QByteArray data;std::size_t offset=0;
        File(SnapshotIO *io,const QByteArray &bytes):owner(io),data(bytes){
            api.UserData=reinterpret_cast<aiUserData>(this);
            api.ReadProc=&SnapshotIO::read;api.WriteProc=&SnapshotIO::write;
            api.TellProc=&SnapshotIO::tell;api.FileSizeProc=&SnapshotIO::size;
            api.SeekProc=&SnapshotIO::seek;api.FlushProc=&SnapshotIO::flush;
        }
    };
public:
    explicit SnapshotIO(Snapshot &source):snapshot(source){
        api.OpenProc=&SnapshotIO::open;api.CloseProc=&SnapshotIO::close;api.UserData=reinterpret_cast<aiUserData>(this);
    }
    ~SnapshotIO(){for(auto *file:opened)delete file;}
    void rethrowFailure()const{if(failure)std::rethrow_exception(failure);}
    aiFileIO api={};Snapshot &snapshot;std::set<QString> missing;
private:
    template<class T,class F>T guard(T fallback,const F &callback){
        if(failure)return fallback;
        try{return callback();}catch(...){failure=std::current_exception();return fallback;}
    }
    static File *file(aiFile *value){return reinterpret_cast<File*>(value->UserData);}
    static aiFile *open(aiFileIO *value,const char *path,const char *mode){
        auto *io=reinterpret_cast<SnapshotIO*>(value->UserData);
        return io->guard<aiFile*>(nullptr,[&]()->aiFile*{
            checkCancelled(io->snapshot.options);
            if(std::strchr(mode,'w')||std::strchr(mode,'a'))return nullptr;
            const QString full=io->snapshot.resolve(path);
            if(!io->snapshot.exists(full)){io->missing.insert(full);return nullptr;}
            std::unique_ptr<File> stream(new File(io,io->snapshot.read(full,true)));
            io->opened.insert(stream.get());auto *api=&stream->api;stream.release();return api;
        });
    }
    static void close(aiFileIO *value,aiFile *stream){
        auto *io=reinterpret_cast<SnapshotIO*>(value->UserData);auto *item=file(stream);
        io->opened.erase(item);delete item;
    }
    static size_t read(aiFile *stream,char *out,size_t size,size_t count){
        auto *item=file(stream);auto *io=item->owner;
        return io->guard<size_t>(0,[&]()->size_t{
            checkCancelled(io->snapshot.options);if(!size)return 0;
            const std::size_t n=std::min(count,(std::size_t(item->data.size())-item->offset)/size);
            if(n)std::memcpy(out,item->data.constData()+item->offset,n*size);
            item->offset+=n*size;
            if(io->snapshot.options.progress)io->snapshot.options.progress(QString::fromUtf8("解析网格数据"),
                item->data.isEmpty()?100:int(item->offset*100/std::size_t(item->data.size())));
            checkCancelled(io->snapshot.options);return n;
        });
    }
    static size_t write(aiFile*,const char*,size_t,size_t){return 0;}
    static size_t tell(aiFile *stream){return file(stream)->offset;}
    static size_t size(aiFile *stream){return std::size_t(file(stream)->data.size());}
    static aiReturn seek(aiFile *stream,size_t position,aiOrigin origin){
        auto *item=file(stream);return item->owner->guard<aiReturn>(aiReturn_FAILURE,[&]()->aiReturn{
            checkCancelled(item->owner->snapshot.options);const auto length=std::size_t(item->data.size());std::size_t target=position;
            if(origin==aiOrigin_CUR){if(position>length-item->offset)return aiReturn_FAILURE;target=item->offset+position;}
            else if(origin==aiOrigin_END){if(position>length)return aiReturn_FAILURE;target=length-position;}
            if(target>length)return aiReturn_FAILURE;item->offset=target;return aiReturn_SUCCESS;
        });
    }
    static void flush(aiFile*){}
    std::set<File*> opened;std::exception_ptr failure;
};
Matrix4d fromAi(const aiMatrix4x4 &m) {
    Matrix4d r;r.values={{m.a1,m.a2,m.a3,m.a4,m.b1,m.b2,m.b3,m.b4,m.c1,m.c2,m.c3,m.c4,m.d1,m.d2,m.d3,m.d4}};return r;
}
void visitMeshNode(const aiScene *scene,const aiNode*node,const aiMatrix4x4 &parent,const std::string &parentId,
                   WorkpieceAsset &asset,const WorkpieceLoadOptions &options,std::uint64_t &vertexCount,std::uint64_t &triCount) {
    checkCancelled(options);
    if(asset.components.size()>=100000)throw LoadFailure(QString::fromUtf8("模型组件数量超过上限"));
    aiMatrix4x4 accumulated=parent*node->mTransformation;
    WorkpieceComponent component;
    component.id="node_"+std::to_string(asset.components.size());
    component.parentId=parentId;component.name=parentId.empty()?"Root":node->mName.C_Str();
    if(component.name.empty())component.name=component.id;
    component.transform=fromAi(accumulated);
    if(!finiteMatrix(component.transform))throw LoadFailure(QString::fromUtf8("组件变换含非法数值"));
    for(unsigned j=0;j<node->mNumMeshes;++j) {
        if(node->mMeshes[j]>=scene->mNumMeshes)throw LoadFailure(QString::fromUtf8("组件网格索引非法"));
        const aiMesh*m=scene->mMeshes[node->mMeshes[j]];
        if(!m->HasPositions())throw LoadFailure(QString::fromUtf8("网格缺少顶点"));
        vertexCount+=m->mNumVertices;
        if(vertexCount>options.maxVertices)throw LoadFailure(QString::fromUtf8("模型顶点数量超限"));
        WorkpieceMesh mesh;mesh.vertices.reserve(m->mNumVertices);
        aiColor3D diffuse(.7f,.7f,.7f);
        if(m->mMaterialIndex<scene->mNumMaterials)scene->mMaterials[m->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE,diffuse);
        for(unsigned k=0;k<m->mNumVertices;++k) {
            const auto &v=m->mVertices[k];WorkpieceVertex out;out.position=Vec3d(v.x,v.y,v.z);
            if(m->HasNormals()){const auto&n=m->mNormals[k];out.normal=Vec3d(n.x,n.y,n.z);}
            out.color={{diffuse.r,diffuse.g,diffuse.b}};
            if(m->HasVertexColors(0)){const auto&c=m->mColors[0][k];out.color={{c.r,c.g,c.b}};}
            if(!finite(out.position)||!finite(out.normal))throw LoadFailure(QString::fromUtf8("网格坐标/法向含 NaN 或 Inf"));
            mesh.vertices.push_back(out);
        }
        std::uint64_t skipped=0;
        for(unsigned k=0;k<m->mNumFaces;++k) {
            if((k&1023)==0)checkCancelled(options);
            const aiFace &f=m->mFaces[k];
            if(f.mNumIndices!=3){++skipped;continue;}
            for(unsigned n=0;n<3;++n)if(f.mIndices[n]>=m->mNumVertices)throw LoadFailure(QString::fromUtf8("网格存在越界面索引"));
            const auto&a=mesh.vertices[f.mIndices[0]].position;const auto&b=mesh.vertices[f.mIndices[1]].position;const auto&c=mesh.vertices[f.mIndices[2]].position;
            Vec3d n=cross(minus(b,a),minus(c,a));double sq=n.x*n.x+n.y*n.y+n.z*n.z;
            if(sq<=1e-30){++skipped;continue;}
            if(++triCount>options.maxTriangles)throw LoadFailure(QString::fromUtf8("模型三角面数超限"));
            for(unsigned n=0;n<3;++n)mesh.indices.push_back(f.mIndices[n]);
            mesh.triangleFaceIds.push_back(k);
            if(!m->HasNormals())for(unsigned l=0;l<3;++l)mesh.vertices[f.mIndices[l]].normal=normal(n);
        }
        if(skipped)asset.warnings.push_back(bytes(QString::fromUtf8("已跳过退化或非表面面：")+QString::number(qulonglong(skipped))));
        if(!mesh.indices.empty())component.meshes.push_back(std::move(mesh));
    }
    const std::string id=component.id;asset.components.push_back(std::move(component));
    for(unsigned j=0;j<node->mNumChildren;++j)visitMeshNode(scene,node->mChildren[j],accumulated,id,asset,options,vertexCount,triCount);
}
void readMesh(Snapshot &snapshot,const QString &format,WorkpieceAsset &asset,const WorkpieceLoadOptions &options) {
    // Assimp 3.2's C error string is process-global, not thread-local. Serialize
    // our import/error/release session; waiting sessions remain cancellable.
    static QMutex meshMutex;
    while(!meshMutex.tryLock(50))checkCancelled(options);
    struct Unlock{QMutex &mutex;~Unlock(){mutex.unlock();}} unlock={meshMutex};
    checkCancelled(options);
    SnapshotIO io(snapshot);const auto file=snapshot.main.toUtf8();
    progress(options,QString::fromUtf8("解析网格"),-1);
    const aiScene*scene=aiImportFileExWithProperties(file.constData(),
        aiProcess_Triangulate|aiProcess_GenSmoothNormals|aiProcess_ValidateDataStructure,&io.api,nullptr);
    struct SceneOwner{const aiScene *scene;~SceneOwner(){aiReleaseImport(scene);}} owner={scene};
    io.rethrowFailure();checkCancelled(options);
    if(!scene||!scene->mRootNode)throw LoadFailure(QString::fromUtf8("网格解析失败：")+QString::fromUtf8(aiGetErrorString()));
    progress(options,QString::fromUtf8("提取网格与组件"),-1);
    std::uint64_t vertices=0,triangles=0;
    visitMeshNode(scene,scene->mRootNode,aiMatrix4x4(),"",asset,options,vertices,triangles);
    if(!triangles)throw LoadFailure(format=="PLY"?QString::fromUtf8("该 PLY 只有顶点，没有有效面。请使用点云查看器，或导出包含面拓扑的模型。"):QString::fromUtf8("模型不包含有效表面"));
    for(const auto&m:io.missing)asset.warnings.push_back(bytes(QString::fromUtf8("缺少材质资源，使用基础颜色：")+m));
    for(unsigned i=0;i<scene->mNumMaterials;++i) {
        aiMaterial*mat=scene->mMaterials[i];
        for(unsigned t=0;t<mat->GetTextureCount(aiTextureType_DIFFUSE);++t) {
            aiString name;mat->GetTexture(aiTextureType_DIFFUSE,t,&name);
            if(name.length&&name.C_Str()[0]!='*') {
                const QString path=snapshot.resolve(name.C_Str());
                if(snapshot.exists(path))snapshot.read(path,false);
                else asset.warnings.push_back(bytes(QString::fromUtf8("贴图缺失：")+path));
            }
        }
    }
    asset.sourceUnit="unknown";asset.unitKnown=false;asset.sourceUnitToMm=1;
    asset.adapterVersion="Assimp-3.2-workpiece-1";
}
#ifdef ROBOT3D_WITH_OCCT
void readCad(Snapshot &,const QString &,WorkpieceAsset &,const WorkpieceLoadOptions &);
#endif
}

#ifdef ROBOT3D_WITH_OCCT
namespace {
class CadInputBuffer : public std::streambuf {
public:
    CadInputBuffer(const QByteArray &d,const WorkpieceLoadOptions &o):data(d),options(o){}
protected:
    pos_type seekoff(off_type distance,std::ios_base::seekdir origin,std::ios_base::openmode mode) override {
        if(!(mode&std::ios_base::in)||(options.cancellation&&options.cancellation->load()))return pos_type(off_type(-1));
        const off_type size=off_type(data.size());
        off_type current=off_type(offset);
        if(gptr()&&egptr())current-=egptr()-gptr();
        const off_type base=origin==std::ios_base::beg?0:(origin==std::ios_base::cur?current:size);
        if(distance < -base || distance > size-base)return pos_type(off_type(-1));
        offset=std::size_t(base+distance);setg(nullptr,nullptr,nullptr);return pos_type(off_type(offset));
    }
    pos_type seekpos(pos_type position,std::ios_base::openmode mode) override {
        return seekoff(off_type(position),std::ios_base::beg,mode);
    }
    int_type underflow() override {
        if(options.cancellation&&options.cancellation->load())return traits_type::eof();
        if(offset>=std::size_t(data.size()))return traits_type::eof();
        const std::size_t count=std::min(std::size_t(4096),std::size_t(data.size())-offset);
        char *start=const_cast<char*>(data.constData())+offset;
        setg(start,start,start+count);offset+=count;
        if(options.progress && ((offset&((1024u*1024u)-1u))==0 || offset==std::size_t(data.size())))
            options.progress(QString::fromUtf8("读取 CAD 数据"),int(offset*100/std::size_t(data.size())));
        return traits_type::to_int_type(*gptr());
    }
private:
    const QByteArray &data;const WorkpieceLoadOptions &options;std::size_t offset=0;
};
#ifdef Q_OS_WIN
// OCCT 7.9.1's IGES library accepts FILE* paths only and its inherited
// ReadStream always fails. A local byte pipe supplies the already captured
// immutable source without creating a plaintext file, including on recovery.
class IgesSnapshotPipe {
public:
    IgesSnapshotPipe(const QByteArray &data,const WorkpieceLoadOptions &options)
        :data_(data),options_(options),name_(QString("\\\\.\\pipe\\robot3d-iges-")+QUuid::createUuid().toString().mid(1,36)){
        pipe_=CreateNamedPipeW(reinterpret_cast<LPCWSTR>(name_.utf16()),
            PIPE_ACCESS_OUTBOUND|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,0,0,nullptr);
        if(pipe_==INVALID_HANDLE_VALUE)throw LoadFailure(QString::fromUtf8("无法创建 IGES 内存快照通道"));
        event_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!event_){CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;throw LoadFailure(QString::fromUtf8("无法初始化 IGES 内存快照通道"));}
        try{writer_=std::thread([this]{try{write();}catch(...){DisconnectNamedPipe(pipe_);}});}catch(...){CloseHandle(event_);CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;throw;}
    }
    ~IgesSnapshotPipe(){
        stop_.store(true);CancelIoEx(pipe_,nullptr);
        if(writer_.joinable()){CancelSynchronousIo(writer_.native_handle());writer_.join();}
        CloseHandle(event_);if(pipe_!=INVALID_HANDLE_VALUE)CloseHandle(pipe_);
    }
    QByteArray name()const{return name_.toUtf8();}
private:
    bool stopped()const{return stop_.load()||(options_.cancellation&&options_.cancellation->load());}
    bool finish(OVERLAPPED &operation,DWORD &amount){
        for(;;){
            DWORD state=WaitForSingleObject(operation.hEvent,50);
            if(state==WAIT_OBJECT_0)return GetOverlappedResult(pipe_,&operation,&amount,FALSE)!=FALSE;
            if(state!=WAIT_TIMEOUT||stopped()){
                CancelIoEx(pipe_,&operation);GetOverlappedResult(pipe_,&operation,&amount,TRUE);return false;
            }
        }
    }
    void write(){
        const HANDLE event=event_;
        OVERLAPPED operation={};operation.hEvent=event;DWORD amount=0;
        BOOL connected=ConnectNamedPipe(pipe_,&operation);
        if(!connected){DWORD error=GetLastError();if(error==ERROR_PIPE_CONNECTED)connected=TRUE;else if(error==ERROR_IO_PENDING)connected=finish(operation,amount);}
        if(!connected||stopped())return;
        std::size_t offset=0;
        while(offset<std::size_t(data_.size())&&!stopped()){
            ResetEvent(event);operation=OVERLAPPED();operation.hEvent=event;
            const DWORD count=DWORD(std::min(std::size_t(65536),std::size_t(data_.size())-offset));
            BOOL written=WriteFile(pipe_,data_.constData()+offset,count,&amount,&operation);
            if(!written){if(GetLastError()!=ERROR_IO_PENDING||!finish(operation,amount))break;}
            if(!amount)break;offset+=amount;
            if(options_.progress)options_.progress(QString::fromUtf8("读取 IGES 快照"),int(offset*100/std::size_t(data_.size())));
        }
        if(offset==std::size_t(data_.size())&&!stopped())FlushFileBuffers(pipe_);
        DisconnectNamedPipe(pipe_);
    }
    const QByteArray &data_;const WorkpieceLoadOptions &options_;QString name_;
    HANDLE pipe_=INVALID_HANDLE_VALUE,event_=nullptr;std::thread writer_;std::atomic_bool stop_{false};
};
#endif
class CadProgress : public Message_ProgressIndicator {
public:
    explicit CadProgress(const WorkpieceLoadOptions &o):options(o){}
    Standard_Boolean UserBreak() override {
        return options.cancellation && options.cancellation->load();
    }
protected:
    void Show(const Message_ProgressScope &, const Standard_Boolean) override {
        if(options.progress)options.progress(QString::fromUtf8("转换/离散 CAD"),int(GetPosition()*100.0));
    }
private:
    const WorkpieceLoadOptions &options;
};
Matrix4d fromTrsf(const gp_Trsf &t) {
    Matrix4d m;
    for(int r=1;r<=3;++r)for(int c=1;c<=4;++c)m.values[(r-1)*4+c-1]=t.Value(r,c);
    return m;
}
std::string labelName(const TDF_Label &label) {
    Handle(TDataStd_Name) name;
    if(label.FindAttribute(TDataStd_Name::GetID(),name)){
        const auto &v=name->Get();
        return bytes(QString::fromUtf16(reinterpret_cast<const ushort*>(v.ToExtString()),v.Length()));
    }
    return std::string();
}
std::array<float,3> labelColor(const Handle(XCAFDoc_ColorTool)&colors,const TDF_Label &label,
                               const std::array<float,3>&fallback) {
    Quantity_Color c;
    if(colors->GetColor(label,XCAFDoc_ColorSurf,c)||colors->GetColor(label,XCAFDoc_ColorGen,c))
        return {{float(c.Red()),float(c.Green()),float(c.Blue())}};
    return fallback;
}
void cadNode(const TDF_Label &input,const std::string &parentId,const Matrix4d &parent,
             const Handle(XCAFDoc_ShapeTool)&shapes,const Handle(XCAFDoc_ColorTool)&colors,
             const std::array<float,3> &inherited,WorkpieceAsset &asset,
             const WorkpieceLoadOptions &options,const Handle(Message_ProgressIndicator)&indicator,
             std::uint64_t &vertexCount,std::uint64_t &triangleCount,std::uint32_t &faceId,int depth) {
    checkCancelled(options);
    if(depth>128||asset.components.size()>=100000)throw LoadFailure(QString::fromUtf8("CAD 装配层级或组件数量超限"));
    TDF_Label label=input,referred;
    Matrix4d world=parent;
    if(shapes->GetReferredShape(input,referred)){
        world=multiply(world,fromTrsf(shapes->GetLocation(input).Transformation()));label=referred;
    }
    TopoDS_Shape shape=shapes->GetShape(label);
    if(shape.IsNull())throw LoadFailure(QString::fromUtf8("CAD 组件缺少有效几何"));
    world=multiply(world,fromTrsf(shape.Location().Transformation()));
    shape.Location(TopLoc_Location());
    WorkpieceComponent component;
    TCollection_AsciiString entry;TDF_Tool::Entry(input,entry);
    component.id=parentId+"/"+entry.ToCString();
    component.parentId=parentId;component.name=labelName(input);
    if(component.name.empty())component.name=labelName(label);
    if(component.name.empty())component.name=component.id;
    component.transform=world;
    auto baseColor=labelColor(colors,input,labelColor(colors,label,inherited));
    TDF_LabelSequence children;shapes->GetComponents(label,children,Standard_False);
    if(children.IsEmpty()){
        progress(options,QString::fromUtf8("离散组件：")+utf8(component.name));
        IMeshTools_Parameters params;params.Deflection=options.linearDeflectionMm;
        params.Angle=options.angularDeflectionDeg*3.14159265358979323846/180.0;
        params.Relative=Standard_False;params.InParallel=Standard_False;
        BRepMesh_IncrementalMesh mesher(shape,params,indicator->Start());
        checkCancelled(options);
        std::size_t faces=0;
        for(TopExp_Explorer it(shape,TopAbs_FACE);it.More();it.Next()){
            checkCancelled(options);++faces;
            const TopoDS_Face face=TopoDS::Face(it.Current());TopLoc_Location location;
            Handle(Poly_Triangulation) tri=BRep_Tool::Triangulation(face,location);
            if(tri.IsNull()||tri->NbTriangles()==0)throw LoadFailure(QString::fromUtf8("CAD 曲面离散失败，不能完整导入组件：")+utf8(component.name));
            vertexCount+=std::uint64_t(tri->NbNodes());
            triangleCount+=std::uint64_t(tri->NbTriangles());
            if(vertexCount>options.maxVertices||triangleCount>options.maxTriangles)
                throw LoadFailure(QString::fromUtf8("CAD 离散网格超过容量限制，请降低离散质量"));
            Quantity_Color faceColor;
            std::array<float,3> color=baseColor;
            if(colors->GetColor(face,XCAFDoc_ColorSurf,faceColor)||colors->GetColor(face,XCAFDoc_ColorGen,faceColor))
                color={{float(faceColor.Red()),float(faceColor.Green()),float(faceColor.Blue())}};
            WorkpieceMesh mesh;mesh.vertices.reserve(tri->NbNodes());
            for(Standard_Integer n=1;n<=tri->NbNodes();++n){
                gp_Pnt p=tri->Node(n).Transformed(location.Transformation());
                WorkpieceVertex v;v.position=Vec3d(p.X(),p.Y(),p.Z());v.color=color;
                if(!finite(v.position))throw LoadFailure(QString::fromUtf8("CAD 曲面包含非法坐标"));
                mesh.vertices.push_back(v);
            }
            for(Standard_Integer n=1;n<=tri->NbTriangles();++n){
                if((n&1023)==0)checkCancelled(options);
                Standard_Integer a,b,c;tri->Triangle(n).Get(a,b,c);
                if(face.Orientation()==TopAbs_REVERSED)std::swap(b,c);
                if(a<1||b<1||c<1||a>tri->NbNodes()||b>tri->NbNodes()||c>tri->NbNodes())
                    throw LoadFailure(QString::fromUtf8("CAD 离散面索引非法"));
                Vec3d norm=cross(minus(mesh.vertices[b-1].position,mesh.vertices[a-1].position),
                                 minus(mesh.vertices[c-1].position,mesh.vertices[a-1].position));
                for(Standard_Integer v:{a,b,c}){
                    mesh.indices.push_back(std::uint32_t(v-1));
                    auto &target=mesh.vertices[v-1].normal;target.x+=norm.x;target.y+=norm.y;target.z+=norm.z;
                }
                mesh.triangleFaceIds.push_back(faceId);
            }
            for(auto &v:mesh.vertices)v.normal=normal(v.normal);
            component.meshes.push_back(std::move(mesh));++faceId;
        }
        if(!faces)throw LoadFailure(QString::fromUtf8("CAD 组件只有线框或点，没有可显示曲面：")+utf8(component.name));
    }
    const std::string id=component.id;asset.components.push_back(std::move(component));
    for(Standard_Integer i=1;i<=children.Length();++i)
        cadNode(children.Value(i),id,world,shapes,colors,baseColor,asset,options,indicator,vertexCount,triangleCount,faceId,depth+1);
}
// OCCT 7.9.1 has no stream callback for external CAF documents. Populate its
// external-file table with fully converted immutable streams before the main
// transfer. ExternFiles exposes a const view of this non-const reader's table;
// the localized cast is valid and confined to this pinned-version adapter.
class SnapshotCadReader : public STEPCAFControl_Reader {
public:
    void importDependency(const char *name,STEPControl_Reader &reader,const Handle(TDocStd_Document)&doc,
                          const WorkpieceLoadOptions &options,const Handle(Message_ProgressIndicator)&indicator){
        TDF_LabelSequence labels;
        const bool ok=STEPCAFControl_Reader::Transfer(reader,0,doc,labels,Standard_False,indicator->Start());
        checkCancelled(options);
        if(!ok||labels.IsEmpty())throw LoadFailure(QString::fromUtf8("STEP 外部几何转换失败：")+QString::fromUtf8(name));
        Handle(STEPCAFControl_ExternFile) external=new STEPCAFControl_ExternFile;
        external->SetWS(reader.WS());external->SetName(new TCollection_HAsciiString(name));
        external->SetLoadStatus(IFSelect_RetDone);external->SetTransferStatus(Standard_True);external->SetLabel(labels.Value(1));
        typedef NCollection_DataMap<TCollection_AsciiString,Handle(STEPCAFControl_ExternFile)> Files;
        Files &files=const_cast<Files&>(ExternFiles());
        if(!files.IsBound(name))files.Bind(name,external);
    }
};
// Inspect STEP entities instead of arbitrary quoted strings: FILE_NAME and product
// names are metadata, even when their text happens to end in .stp/.step.
void snapshotCadDependencies(SnapshotCadReader &owner,STEPControl_Reader &reader,const QString &fileName,Snapshot &snapshot,
                             const WorkpieceLoadOptions &options,const Handle(TDocStd_Document)&doc,const Handle(Message_ProgressIndicator)&indicator,std::set<QString> &active,
                             std::set<QString> &complete,std::map<std::string,QString> &resolved,int depth) {
    checkCancelled(options);
    if(depth>128)throw LoadFailure(QString::fromUtf8("STEP 外部依赖层级超限"));
    STEPConstruct_ExternRefs refs(reader.WS());refs.LoadExternRefs();
    for(Standard_Integer i=1;i<=refs.NbExternRefs();++i){
        checkCancelled(options);
        if(refs.ProdDef(i).IsNull())continue;
        const char *name=refs.FileName(i);
        if(!name||!name[0])throw LoadFailure(QString::fromUtf8("STEP 几何外部引用缺少文件名"));
        const auto format=refs.Format(i);
        if(!format.IsNull()&&QString::fromUtf8(format->ToCString()).compare("STEP AP203",Qt::CaseInsensitive)!=0&&QString::fromUtf8(format->ToCString()).compare("STEP AP214",Qt::CaseInsensitive)!=0)
            throw LoadFailure(QString::fromUtf8("不支持的 STEP 外部几何格式：")+QString::fromUtf8(format->ToCString()));
        const QString full=QFileInfo(QDir(QFileInfo(fileName).absolutePath()).absoluteFilePath(QString::fromUtf8(name).replace('\\','/'))).absoluteFilePath();
        const auto entry=resolved.insert(std::make_pair(std::string(name),full));
        if(!entry.second&&entry.first->second.compare(full,Qt::CaseInsensitive)!=0)
            throw LoadFailure(QString::fromUtf8("不同目录的 STEP 外部引用重名，无法可靠转换：")+QString::fromUtf8(name));
        if(active.count(full))throw LoadFailure(QString::fromUtf8("STEP 外部依赖存在循环引用：")+full);
        if(complete.count(full))continue;
        if(snapshot.asset.dependencies.size()>=10000)throw LoadFailure(QString::fromUtf8("STEP 外部依赖数量超限"));
        const QByteArray data=snapshot.read(full,true);
        STEPControl_Reader dependency;
        CadInputBuffer buffer(data,options);std::istream input(&buffer);const QByteArray path=full.toUtf8();
        const auto status=dependency.ReadStream(path.constData(),input);checkCancelled(options);
        if(status!=IFSelect_RetDone)throw LoadFailure(QString::fromUtf8("STEP 外部几何读取失败：")+full);
        dependency.SetSystemLengthUnit(1.0);active.insert(full);
        snapshotCadDependencies(owner,dependency,full,snapshot,options,doc,indicator,active,complete,resolved,depth+1);
        owner.importDependency(name,dependency,doc,options,indicator);
        active.erase(full);complete.insert(full);
    }
}
void readCad(Snapshot &snapshot,const QString &format,WorkpieceAsset &asset,const WorkpieceLoadOptions &options) {
    static QMutex cadMutex;
    while(!cadMutex.tryLock(50))checkCancelled(options);
    struct Unlock{QMutex&m;~Unlock(){m.unlock();}} unlock={cadMutex};
    checkCancelled(options);
    Interface_Static::SetCVal("xstep.cascade.unit","MM");
    Handle(TDocStd_Document) doc=new TDocStd_Document(TCollection_ExtendedString("BinXCAF"));
    Handle(Message_ProgressIndicator) indicator=new CadProgress(options);
    const QByteArray source=snapshot.cache[snapshot.main];
    CadInputBuffer input(source,options);std::istream stream(&input);
    const QByteArray name=snapshot.main.toUtf8();
    if(format=="STEP"){
        SnapshotCadReader reader;reader.SetColorMode(Standard_True);reader.SetNameMode(Standard_True);reader.SetLayerMode(Standard_True);
        const auto readStatus=reader.ReadStream(name.constData(),stream);checkCancelled(options);
        if(readStatus!=IFSelect_RetDone)
            throw LoadFailure(QString::fromUtf8("STEP 文件读取失败"));
        std::set<QString> active,complete;std::map<std::string,QString> resolved;
        active.insert(snapshot.main);
        snapshotCadDependencies(reader,reader.ChangeReader(),snapshot.main,snapshot,options,doc,indicator,active,complete,resolved,0);
        reader.ChangeReader().SetSystemLengthUnit(1.0);
        TColStd_SequenceOfAsciiString lengths,angles,solidAngles;
        reader.ChangeReader().FileUnits(lengths,angles,solidAngles);
        if(!reader.Transfer(doc,indicator->Start()))throw LoadFailure(QString::fromUtf8("STEP 几何转换失败或已取消"));
        const auto &externFiles=reader.ExternFiles();
        for(NCollection_DataMap<TCollection_AsciiString,Handle(STEPCAFControl_ExternFile)>::Iterator it(externFiles);it.More();it.Next()){
            checkCancelled(options);const auto &external=it.Value();
            if(external.IsNull()||external->GetLoadStatus()!=IFSelect_RetDone||!external->GetTransferStatus()||external->GetLabel().IsNull())
                throw LoadFailure(QString::fromUtf8("STEP 外部几何转换失败，已拒绝不完整导入：")+QString::fromUtf8(it.Key().ToCString()));
        }
        if(lengths.Length()){
            QStringList units;for(int i=1;i<=lengths.Length();++i)units<<QString::fromUtf8(lengths.Value(i).ToCString());
            asset.warnings.push_back(bytes(QString::fromUtf8("STEP 声明单位：")+units.join(",")+QString::fromUtf8("；OCCT 转换后的几何与组件位置已统一为 mm。")));
        }
        if(!snapshot.memoryOnly)for(const auto &dep:asset.dependencies){
            const QString p=QDir(snapshot.base).absoluteFilePath(utf8(dep.relativePath));
            QFile f(p);
            if(!f.open(QIODevice::ReadOnly)||QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256).toHex().toStdString()!=dep.sha256)
                throw LoadFailure(QString::fromUtf8("外部几何依赖在转换期间变化，请重新加载：")+p);
        }
    }else{
        IGESCAFControl_Reader reader;reader.SetColorMode(Standard_True);reader.SetNameMode(Standard_True);reader.SetLayerMode(Standard_True);
#ifdef Q_OS_WIN
        IgesSnapshotPipe pipe(source,options);const QByteArray pipeName=pipe.name();
        const auto readStatus=reader.ReadFile(pipeName.constData());checkCancelled(options);
#else
        const auto readStatus=reader.ReadStream(name.constData(),stream);checkCancelled(options);
#endif
        if(readStatus!=IFSelect_RetDone)
            throw LoadFailure(QString::fromUtf8("IGES 快照读取失败"));
        if(!reader.Transfer(doc,indicator->Start()))throw LoadFailure(QString::fromUtf8("IGES 几何转换失败或已取消"));
        asset.warnings.push_back(bytes(QString::fromUtf8("IGES 几何与组件位置由 OCCT 按文件单位转换为 mm。")));
    }
    checkCancelled(options);
    const auto shapes=XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    const auto colors=XCAFDoc_DocumentTool::ColorTool(doc->Main());
    TDF_LabelSequence roots;shapes->GetFreeShapes(roots);
    if(roots.IsEmpty())throw LoadFailure(QString::fromUtf8("CAD 文件没有可用几何"));
    std::uint64_t vertices=0,triangles=0;std::uint32_t face=0;
    for(Standard_Integer i=1;i<=roots.Length();++i)
        cadNode(roots.Value(i),"",Matrix4d(),shapes,colors,{{.7f,.7f,.7f}},asset,options,indicator,vertices,triangles,face,0);
    if(!triangles)throw LoadFailure(QString::fromUtf8("CAD 文件没有可用曲面"));
    asset.sourceUnit="mm";asset.sourceUnitToMm=1.0;asset.unitKnown=true;
    asset.adapterVersion="OCCT-" OCC_VERSION_COMPLETE "-workpiece-1";
}
}
#endif
namespace {
bool loadWorkpieceInput(const QString &path,const WorkpieceAsset *stored,const WorkpieceLoadOptions &requested,
                        std::shared_ptr<WorkpieceAsset>&result,QString&error) {
    try {
        WorkpieceLoadOptions options=requested;
        if(stored){options.linearDeflectionMm=stored->linearDeflectionMm;options.angularDeflectionDeg=stored->angularDeflectionDeg;}
        if(!std::isfinite(options.linearDeflectionMm)||options.linearDeflectionMm<=0||!std::isfinite(options.angularDeflectionDeg)||options.angularDeflectionDeg<=0||options.angularDeflectionDeg>90)
            throw LoadFailure(QString::fromUtf8("离散质量参数不合法"));
        progress(options,QString::fromUtf8(stored?"读取包内源快照":"读取文件"),0);
        auto candidate=std::make_shared<WorkpieceAsset>();
        std::unique_ptr<Snapshot> input;
        if(stored)input.reset(new Snapshot(*stored,options,*candidate));else input.reset(new Snapshot(path,options,*candidate));
        Snapshot &snapshot=*input;
        const QByteArray source=snapshot.read(snapshot.main,true);
        if(source.isEmpty())throw LoadFailure(QString::fromUtf8("模型文件为空"));
        const QString format=detectFormat(source,QFileInfo(snapshot.main).suffix().toLower());
        if(stored&&format!=utf8(stored->format))throw LoadFailure(QString::fromUtf8("源快照格式与保存的元数据不一致"));
        candidate->id=QCryptographicHash::hash(source,QCryptographicHash::Sha256).toHex().toStdString();
        candidate->name=stored?stored->name:bytes(QFileInfo(path).completeBaseName());
        candidate->sourcePath=stored?stored->sourcePath:bytes(snapshot.main);candidate->format=bytes(format);
        candidate->sourceBytes.assign(source.begin(),source.end());
        candidate->linearDeflectionMm=options.linearDeflectionMm;candidate->angularDeflectionDeg=options.angularDeflectionDeg;
        progress(options,QString::fromUtf8("解析模型"),-1);
        if(format=="STEP"||format=="IGES") {
#ifdef ROBOT3D_WITH_OCCT
            readCad(snapshot,format,*candidate,options);
#else
            throw LoadFailure(QString::fromUtf8("此构建未启用 OCCT，无法导入或重建 STEP/IGES。请安装完整 CAD 功能版本。"));
#endif
        }else readMesh(snapshot,format,*candidate,options);
        std::sort(candidate->dependencies.begin(),candidate->dependencies.end(),[](const WorkpieceSourceFile&a,const WorkpieceSourceFile&b){return a.relativePath<b.relativePath;});
        QCryptographicHash identity(QCryptographicHash::Sha256);identity.addData(source);
        identity.addData(candidate->adapterVersion.data(),int(candidate->adapterVersion.size()));
        for(const auto&dep:candidate->dependencies){identity.addData(dep.relativePath.data(),int(dep.relativePath.size()));identity.addData(dep.sha256.data(),int(dep.sha256.size()));}
        candidate->id=identity.result().toHex().toStdString();
        progress(options,QString::fromUtf8("检查几何"),95);
        candidate->warnings.push_back(bytes(QString::fromUtf8("封闭性、非流形及自交：未检查；显示成功不代表可用于工程计算。")));
        checkCancelled(options);
        progress(options,QString::fromUtf8("预览数据就绪"),100);result=candidate;error.clear();return true;
    }catch(const LoadFailure&e){error=QString::fromUtf8(e.what());}
#ifdef ROBOT3D_WITH_OCCT
    catch(const Standard_Failure&e){error=QString::fromUtf8("OCCT 解析失败：")+QString::fromUtf8(e.GetMessageString()?e.GetMessageString():"unknown failure");}
#endif
    catch(const std::bad_alloc&){error=QString::fromUtf8("内存不足，已取消模型加载。请降低离散精度或减少组件。");}
    catch(const std::exception&e){error=QString::fromUtf8("模型加载失败：")+QString::fromUtf8(e.what());}
    return false;
}
}
bool loadWorkpieceFile(const QString &path,const WorkpieceLoadOptions &options,std::shared_ptr<WorkpieceAsset>&result,QString&error) {
    return loadWorkpieceInput(path,nullptr,options,result,error);
}
bool rebuildWorkpieceAsset(const WorkpieceAsset &stored,const WorkpieceLoadOptions &options,std::shared_ptr<WorkpieceAsset>&result,QString&error) {
    return loadWorkpieceInput(QString(),&stored,options,result,error);
}
QString workpieceAdapterVersion(const QString &format) {
    const QString type=format.toUpper();
    if(type=="STEP"||type=="IGES"){
#ifdef ROBOT3D_WITH_OCCT
        return "OCCT-" OCC_VERSION_COMPLETE "-workpiece-1";
#else
        return QString();
#endif
    }
    return (type=="STL"||type=="OBJ"||type=="PLY")?QString("Assimp-3.2-workpiece-1"):QString();
}
QString workpieceImportCapabilities() {
#ifdef ROBOT3D_WITH_OCCT
    return QString::fromUtf8("STEP/IGES (OCCT " OCC_VERSION_COMPLETE "), STL/OBJ/PLY");
#else
    return QString::fromUtf8("STL/OBJ/PLY；本构建未启用 OCCT");
#endif
}
} }
