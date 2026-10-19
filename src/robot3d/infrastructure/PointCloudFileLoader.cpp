#include "robot3d/infrastructure/PointCloudFileLoader.h"
#include <QFile>
#include <QFileInfo>
#include <QRegExp>
#include <QStringList>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace robot3d { namespace infrastructure {
namespace {
const qint64 kMaxPoints=10000000;
const qint64 kMaxPayload=512*1024*1024;
void fail(const QString& message) { throw std::runtime_error(message.toUtf8().constData()); }
QByteArray line(QFile& file) {
    const bool first=file.pos()==0;
    auto bytes=file.readLine(1024*1024);
    if(first&&bytes.startsWith("\xef\xbb\xbf")) bytes.remove(0,3);
    if(bytes.size()==1024*1024&&!bytes.endsWith('\n')) fail(QString::fromUtf8("文件行过长。"));
    return bytes.trimmed();
}
QStringList words(const QByteArray& value) {
    return QString::fromUtf8(value).trimmed().split(QRegExp("\\s+"),QString::SkipEmptyParts);
}
qint64 integer(const QString& value) {
    bool ok=false; const auto n=value.toLongLong(&ok);
    if(!ok||n<0) fail(QString::fromUtf8("无效的非负整数：%1").arg(value));
    return n;
}
QByteArray readBytes(QFile& file,qint64 count) {
    if(count<0||count>kMaxPayload) fail(QString::fromUtf8("点云数据超过 512 MiB 限制。"));
    auto data=file.read(count);
    if(data.size()!=count) fail(QString::fromUtf8("文件数据不完整或读取失败。"));
    return data;
}
struct Scalar { char kind; int size; };
bool valid(Scalar type) {
    return (type.kind=='F'&&(type.size==4||type.size==8)) ||
        ((type.kind=='I'||type.kind=='U')&&(type.size==1||type.size==2||type.size==4||type.size==8));
}
Scalar plyType(const QString& name) {
    if(name=="char"||name=="int8") return {'I',1};
    if(name=="uchar"||name=="uint8") return {'U',1};
    if(name=="short"||name=="int16") return {'I',2};
    if(name=="ushort"||name=="uint16") return {'U',2};
    if(name=="int"||name=="int32") return {'I',4};
    if(name=="uint"||name=="uint32") return {'U',4};
    if(name=="float"||name=="float32") return {'F',4};
    if(name=="double"||name=="float64") return {'F',8};
    fail(QString::fromUtf8("不支持的 PLY 属性类型：%1").arg(name)); return {'F',4};
}
struct Value { double number=0; quint32 bits=0; };
Value binaryValue(const char* bytes,Scalar type,bool little=true) {
    unsigned char buffer[8]={};
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    const bool reverse=!little;
#else
    const bool reverse=little;
#endif
    for(int i=0;i<type.size;++i) buffer[i]=static_cast<unsigned char>(bytes[reverse?type.size-1-i:i]);
    Value v;
    if(type.size==4) std::memcpy(&v.bits,buffer,4);
    if(type.kind=='F') {
        if(type.size==4) { float f; std::memcpy(&f,buffer,4); v.number=f; }
        else { double d; std::memcpy(&d,buffer,8); v.number=d; }
    } else if(type.kind=='I') {
        if(type.size==1) { qint8 n; std::memcpy(&n,buffer,1); v.number=n; }
        if(type.size==2) { qint16 n; std::memcpy(&n,buffer,2); v.number=n; }
        if(type.size==4) { qint32 n; std::memcpy(&n,buffer,4); v.number=n; }
        if(type.size==8) { qint64 n; std::memcpy(&n,buffer,8); v.number=double(n); }
    } else {
        if(type.size==1) { quint8 n; std::memcpy(&n,buffer,1); v.number=n; }
        if(type.size==2) { quint16 n; std::memcpy(&n,buffer,2); v.number=n; }
        if(type.size==4) { quint32 n; std::memcpy(&n,buffer,4); v.number=n; }
        if(type.size==8) { quint64 n; std::memcpy(&n,buffer,8); v.number=double(n); }
    }
    return v;
}
Value asciiValue(const QString& text,Scalar type) {
    bool ok=false; Value value; value.number=text.toDouble(&ok);
    if(!ok) fail(QString::fromUtf8("无法解析数值：%1").arg(text));
    if(type.kind!='F') {
        const double maxValue=type.kind=='U'?std::pow(2.0,8*type.size)-1:std::pow(2.0,8*type.size-1)-1;
        const double minValue=type.kind=='U'?0:-std::pow(2.0,8*type.size-1);
        if(!std::isfinite(value.number)||std::floor(value.number)!=value.number||value.number<minValue||value.number>maxValue)
            fail(QString::fromUtf8("整数属性超出范围：%1").arg(text));
    }
    if(type.size==4) {
        if(type.kind=='F') { const float f=float(value.number); std::memcpy(&value.bits,&f,4); }
        else if(type.kind=='U') value.bits=quint32(value.number);
        else value.bits=quint32(qint32(value.number));
    }
    return value;
}
unsigned char channel(double value) {
    if(!std::isfinite(value)||value<0||value>255||std::floor(value)!=value)
        fail(QString::fromUtf8("RGB 颜色必须为 0～255 的整数。"));
    return static_cast<unsigned char>(value);
}
struct Fields {
    int x=-1,y=-1,z=-1,r=-1,g=-1,b=-1,packed=-1;
    void add(const QString& name,int index) {
        if(name=="x") x=index; else if(name=="y") y=index; else if(name=="z") z=index;
        else if(name=="red"||name=="r") r=index;
        else if(name=="green"||name=="g") g=index;
        else if(name=="blue"||name=="b") b=index;
        else if(name=="rgb"||name=="rgba") packed=index;
    }
    bool colored() const { return packed>=0||(r>=0&&g>=0&&b>=0); }
    void validate() const { if(x<0||y<0||z<0) fail(QString::fromUtf8("点云缺少 x、y 或 z 坐标字段。")); }
    void append(const std::vector<Value>& values,domain::PointCloud& cloud) const {
        domain::CloudPoint p; p.x=values[x].number; p.y=values[y].number; p.z=values[z].number;
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) { ++cloud.skippedPoints; return; }
        if(packed>=0) { const auto n=values[packed].bits; p.color={{static_cast<unsigned char>(n>>16),static_cast<unsigned char>(n>>8),static_cast<unsigned char>(n)}}; }
        else if(colored()) p.color={{channel(values[r].number),channel(values[g].number),channel(values[b].number)}};
        cloud.points.push_back(p);
    }
};
void checkCount(qint64 count) {
    if(count<=0||count>kMaxPoints) fail(QString::fromUtf8("点数须为 1～1000 万。"));
}
void loadText(QFile& file,domain::PointCloud& cloud) {
    int columns=0,lineNumber=0; Fields fields; fields.x=0; fields.y=1; fields.z=2;
    while(!file.atEnd()) {
        ++lineNumber; auto text=line(file);
        if(text.isEmpty()||text.startsWith('#')||text.startsWith("//")) continue;
        const int comment=text.indexOf('#'); if(comment>=0) text.truncate(comment);
        text.replace(',',' '); text.replace(';',' ');
        const auto tokens=words(text);
        if((tokens.size()!=3&&tokens.size()!=6)||(columns&&columns!=tokens.size()))
            fail(QString::fromUtf8("第 %1 行：需要一致的 XYZ（三列）或 XYZ RGB（六列）数据。").arg(lineNumber));
        columns=tokens.size();
        if(columns==6) { fields.r=3; fields.g=4; fields.b=5; cloud.hasColors=true; }
        std::vector<Value> values;
        for(const auto& token:tokens) values.push_back(asciiValue(token,{'F',8}));
        fields.append(values,cloud);
        if(cloud.points.size()+cloud.skippedPoints>std::size_t(kMaxPoints)) fail(QString::fromUtf8("点数超过 1000 万限制。"));
    }
    if(file.error()!=QFile::NoError) fail(file.errorString());
}
QByteArray decompressLzf(const QByteArray& input,int size) {
    QByteArray output(size,Qt::Uninitialized); int src=0,dst=0;
    while(src<input.size()) {
        const auto control=static_cast<unsigned char>(input[src++]);
        if(control<32) {
            const int count=control+1;
            if(src+count>input.size()||dst+count>size) fail(QString::fromUtf8("PCD 压缩数据损坏。"));
            std::memcpy(output.data()+dst,input.constData()+src,count); src+=count; dst+=count;
        } else {
            int count=control>>5,offset=(control&31)<<8;
            if(count==7) { if(src>=input.size()) fail(QString::fromUtf8("PCD 压缩数据损坏。")); count+=static_cast<unsigned char>(input[src++]); }
            if(src>=input.size()) fail(QString::fromUtf8("PCD 压缩数据损坏。"));
            offset+=static_cast<unsigned char>(input[src++])+1; count+=2;
            if(offset>dst||dst+count>size) fail(QString::fromUtf8("PCD 压缩数据损坏。"));
            for(int i=0;i<count;++i) { output[dst]=output[dst-offset]; ++dst; }
        }
    }
    if(dst!=size) fail(QString::fromUtf8("PCD 解压长度不匹配。"));
    return output;
}
void loadPcd(QFile& file,domain::PointCloud& cloud) {
    QStringList names,sizes,types,counts; qint64 count=-1,width=-1,height=1; QString mode;
    for(int headerLines=0;mode.isEmpty();++headerLines) {
        if(file.atEnd()||headerLines>4096) fail(QString::fromUtf8("PCD 文件头不完整。"));
        auto tokens=words(line(file)); if(tokens.isEmpty()||tokens[0].startsWith('#')) continue;
        const auto key=tokens.takeFirst().toUpper();
        if(key=="FIELDS"||key=="FIELD") names=tokens;
        else if(key=="SIZE") sizes=tokens;
        else if(key=="TYPE") types=tokens;
        else if(key=="COUNT") counts=tokens;
        else if(key=="POINTS"&&tokens.size()==1) count=integer(tokens[0]);
        else if(key=="WIDTH"&&tokens.size()==1) width=integer(tokens[0]);
        else if(key=="HEIGHT"&&tokens.size()==1) height=integer(tokens[0]);
        else if(key=="DATA"&&tokens.size()==1) mode=tokens[0].toLower();
    }
    if(width>kMaxPoints||height>kMaxPoints) fail(QString::fromUtf8("PCD 尺寸超过限制。"));
    if(count<0&&width>=0) count=width*height;
    checkCount(count);
    if(width>=0&&count!=width*height) fail(QString::fromUtf8("PCD POINTS 与 WIDTH × HEIGHT 不一致。"));
    if(names.isEmpty()||names.size()!=sizes.size()||names.size()!=types.size()||(!counts.isEmpty()&&counts.size()!=names.size()))
        fail(QString::fromUtf8("PCD 字段定义不完整。"));
    if(names.size()>1024) fail(QString::fromUtf8("PCD 字段过多。"));
    Fields fields; std::vector<Scalar> scalar; std::vector<int> components,offsets,valueOffsets;
    int stride=0,valueCount=0;
    for(int i=0;i<names.size();++i) {
        const auto bytes=integer(sizes[i]); const auto n=counts.isEmpty()?1:integer(counts[i]);
        if(bytes>8||n<1||n>4096) fail(QString::fromUtf8("无效的 PCD 字段大小或 COUNT。"));
        const auto kind=types[i].toUpper().toLatin1(); Scalar s={kind.isEmpty()?'?':kind[0],int(bytes)};
        if(!valid(s)) fail(QString::fromUtf8("不支持的 PCD 数值类型。"));
        if((names[i]=="rgb"||names[i]=="rgba")&&s.size!=4) fail(QString::fromUtf8("PCD 打包颜色须为 4 字节。"));
        fields.add(names[i].toLower(),i); scalar.push_back(s); components.push_back(int(n)); offsets.push_back(stride); valueOffsets.push_back(valueCount);
        stride+=int(bytes*n); valueCount+=int(n);
    }
    fields.validate(); cloud.hasColors=fields.colored(); cloud.points.reserve(std::size_t(count));
    if(stride<=0||qint64(stride)*count>kMaxPayload) fail(QString::fromUtf8("PCD 数据超过 512 MiB 限制。"));
    const bool ascii=mode=="ascii",compressed=mode=="binary_compressed";
    QByteArray payload;
    if(compressed) {
        const auto header=readBytes(file,8);
        const auto compressedSize=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(header.constData()));
        const auto rawSize=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(header.constData()+4));
        if(rawSize!=qint64(stride)*count||compressedSize>kMaxPayload) fail(QString::fromUtf8("PCD 压缩长度无效。"));
        payload=decompressLzf(readBytes(file,compressedSize),int(rawSize));
    } else if(mode=="binary") payload=readBytes(file,qint64(stride)*count);
    else if(!ascii) fail(QString::fromUtf8("不支持的 PCD DATA：%1").arg(mode));
    std::vector<Value> values(names.size());
    for(qint64 row=0;row<count;++row) {
        QStringList tokens;
        if(ascii) {
            do { if(file.atEnd()) fail(QString::fromUtf8("PCD 点数据不足。")); const auto text=line(file); if(!text.startsWith('#')) tokens=words(text); } while(tokens.isEmpty());
            if(tokens.size()!=valueCount) fail(QString::fromUtf8("PCD 第 %1 个点的列数不匹配。").arg(row+1));
        }
        for(int i=0;i<names.size();++i) {
            if(ascii) {
                for(int k=0;k<components[i];++k) {
                    const auto value=asciiValue(tokens[valueOffsets[i]+k],scalar[i]); if(k==0) values[i]=value;
                }
            } else {
                const qint64 offset=compressed?qint64(offsets[i])*count+row*scalar[i].size*components[i]:row*stride+offsets[i];
                values[i]=binaryValue(payload.constData()+offset,scalar[i]);
            }
        }
        fields.append(values,cloud);
    }
}
struct Property { QString name; Scalar type; bool list=false; Scalar countType={'U',1}; };
struct Element { QString name; qint64 count=0; std::vector<Property> properties; };
void loadPly(QFile& file,domain::PointCloud& cloud) {
    if(line(file)!="ply") fail(QString::fromUtf8("无效的 PLY 文件标识。"));
    QString format; std::vector<Element> elements; bool ended=false;
    for(int n=0;n<4096&&!file.atEnd();++n) {
        const auto tokens=words(line(file)); if(tokens.isEmpty()) continue;
        if(tokens[0]=="end_header") { ended=true; break; }
        if(tokens[0]=="format"&&tokens.size()==3&&tokens[2]=="1.0") format=tokens[1];
        else if(tokens[0]=="element"&&tokens.size()==3) {
            Element e; e.name=tokens[1]; e.count=integer(tokens[2]);
            if(e.count>kMaxPoints) fail(QString::fromUtf8("PLY 元素数量超过 1000 万。"));
            elements.push_back(e);
        } else if(tokens[0]=="property") {
            if(elements.empty()) fail(QString::fromUtf8("PLY 属性缺少 element 定义。"));
            Property p;
            if(tokens.size()==3) { p.type=plyType(tokens[1]); p.name=tokens[2]; }
            else if(tokens.size()==5&&tokens[1]=="list") {
                p.list=true; p.countType=plyType(tokens[2]); p.type=plyType(tokens[3]); p.name=tokens[4];
                if(p.countType.kind=='F') fail(QString::fromUtf8("PLY 列表长度须为整数。"));
            } else fail(QString::fromUtf8("无效的 PLY 属性。"));
            elements.back().properties.push_back(p);
        }
    }
    if(!ended||(format!="ascii"&&format!="binary_little_endian"&&format!="binary_big_endian")) fail(QString::fromUtf8("PLY 文件头或编码无效。"));
    const bool ascii=format=="ascii",little=format!="binary_big_endian";
    for(const auto& e:elements) {
        const bool vertex=e.name=="vertex"; Fields fields;
        if(vertex) {
            checkCount(e.count);
            for(int i=0;i<int(e.properties.size());++i) {
                if(e.properties[i].list) fail(QString::fromUtf8("PLY 顶点不支持列表属性。"));
                fields.add(e.properties[i].name,i);
            }
            fields.validate(); cloud.hasColors=fields.colored(); cloud.points.reserve(std::size_t(e.count));
        }
        std::vector<Value> values(e.properties.size());
        for(qint64 row=0;row<e.count;++row) {
            QStringList tokens; int token=0;
            if(ascii) {
                do { if(file.atEnd()) fail(QString::fromUtf8("PLY 数据不完整。")); tokens=words(line(file)); } while(tokens.isEmpty());
            }
            const auto read=[&](Scalar type) {
                if(!ascii) { const auto bytes=readBytes(file,type.size); return binaryValue(bytes.constData(),type,little); }
                if(token>=tokens.size()) fail(QString::fromUtf8("PLY 数据列数不足。"));
                return asciiValue(tokens[token++],type);
            };
            for(int i=0;i<int(e.properties.size());++i) {
                const auto& p=e.properties[i];
                if(p.list) {
                    const double length=read(p.countType).number;
                    if(length<0||length>1000000) fail(QString::fromUtf8("PLY 列表长度无效。"));
                    if(!ascii) readBytes(file,qint64(length)*p.type.size);
                    else for(int k=0;k<int(length);++k) read(p.type);
                } else values[i]=read(p.type);
            }
            if(ascii&&token!=tokens.size()) fail(QString::fromUtf8("PLY 数据列数不匹配。"));
            if(vertex) fields.append(values,cloud);
        }
        // Only vertex data is needed; trailing face/edge data does not affect point display.
        if(vertex) return;
    }
    fail(QString::fromUtf8("PLY 缺少 vertex 元素。"));
}
}
PointCloudLoadResult loadPointCloudFile(const QString& path) {
    PointCloudLoadResult result;
    try {
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)) fail(QString::fromUtf8("无法打开文件：%1").arg(file.errorString()));
        auto cloud=std::make_shared<domain::PointCloud>();
        const auto suffix=QFileInfo(path).suffix().toLower();
        if(suffix=="pcd") loadPcd(file,*cloud);
        else if(suffix=="ply") loadPly(file,*cloud);
        else if(suffix=="xyz"||suffix=="txt") loadText(file,*cloud);
        else fail(QString::fromUtf8("请选择 PCD、PLY、XYZ 或 TXT 点云文件。"));
        if(cloud->points.empty()) fail(QString::fromUtf8("文件中没有有效点；请检查坐标是否为空或全部为 NaN/Inf。"));
        result.cloud=cloud;
    } catch(const std::bad_alloc&) { result.error=QString::fromUtf8("内存不足，请使用较小的点云文件。"); }
      catch(const std::exception& error) { result.error=QString::fromUtf8(error.what()); }
    return result;
}
} }
