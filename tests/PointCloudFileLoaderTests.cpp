#include "robot3d/infrastructure/PointCloudFileLoader.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void require(bool ok,const char* message) { ++checks; if(!ok) throw std::runtime_error(message); }
QString save(const QDir& dir,const QString& name,const QByteArray& bytes) {
    const auto path=dir.filePath(name); QFile file(path);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()) throw std::runtime_error("write fixture");
    return path;
}
void append32(QByteArray& bytes,quint32 value,bool little=true) {
    uchar data[4]; if(little) qToLittleEndian(value,data); else qToBigEndian(value,data);
    bytes.append(reinterpret_cast<const char*>(data),4);
}
void appendFloat(QByteArray& bytes,float value,bool little=true) { quint32 bits; std::memcpy(&bits,&value,4); append32(bytes,bits,little); }
QByteArray pcdHeader(const QByteArray& encoding,int count=2) {
    return "# .PCD v0.7\nVERSION .7\nFIELDS x y z rgb\nSIZE 4 4 4 4\nTYPE F F F U\nCOUNT 1 1 1 1\nWIDTH "+QByteArray::number(count)+"\nHEIGHT 1\nPOINTS "+QByteArray::number(count)+"\nDATA "+encoding+"\n";
}
QByteArray plyHeader(const QByteArray& encoding) {
    return "ply\nformat "+encoding+" 1.0\nelement vertex 2\nproperty uchar blue\nproperty float z\nproperty float x\nproperty uchar red\nproperty float y\nproperty uchar green\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n";
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        QTemporaryDir temp; require(temp.isValid(),"temporary fixtures");
        QDir dir(temp.path());
        using robot3d::infrastructure::loadPointCloudFile;
        auto result=loadPointCloudFile(save(dir,QString::fromUtf8("中文点云.xyz"),"# comment\n1, 2, 3\n4;5;6\nnan 1 2\n"));
        require(result.ok()&&result.cloud->points.size()==2&&result.cloud->skippedPoints==1,"Unicode XYZ path and invalid coordinate removal");
        require(result.cloud->points[1].x==4&&!result.cloud->hasColors,"XYZ coordinates");
        const QByteArray bom=QByteArray::fromHex("efbbbf");
        require(loadPointCloudFile(save(dir,"bom.xyz",bom+"1 2 3\r\n")).ok(),"UTF-8 BOM text");
        require(loadPointCloudFile(save(dir,"bom.ply",bom+plyHeader("ascii")+"128 3 1 255 2 0\n64 6 4 0 5 255\n")).ok(),"UTF-8 BOM PLY");
        require(loadPointCloudFile(save(dir,"bom.pcd",bom+pcdHeader("ascii")+"1 2 3 16711680\n4 5 6 65280\n")).ok(),"UTF-8 BOM PCD");
        result=loadPointCloudFile(save(dir,"color.TXT","1 2 3 255 0 128\n4 5 6 0 255 64\n"));
        require(result.ok()&&result.cloud->hasColors&&result.cloud->points[0].color[2]==128,"XYZ RGB and uppercase extension");
        for(const auto& text:QList<QByteArray>{"", "nan inf -inf\n", "1 2\n", "1 2 invalid\n", "1 2 3 256 0 0\n", "1 2 3\n4 5 6 7 8 9\n"}) {
            result=loadPointCloudFile(save(dir,"invalid.xyz",text)); require(!result.ok()&&!result.error.isEmpty(),"invalid text fails with reason");
        }
        result=loadPointCloudFile(save(dir,"ascii.pcd",pcdHeader("ascii")+"1 2 3 16711680\n4 5 6 65280\n"));
        require(result.ok()&&result.cloud->points.size()==2&&result.cloud->hasColors,"ASCII PCD");
        require(result.cloud->points[0].color[0]==255&&result.cloud->points[1].color[1]==255,"packed unsigned RGB");
        result=loadPointCloudFile(save(dir,"reordered.pcd","FIELDS intensity z x normal y\nSIZE 4 8 8 4 8\nTYPE F F F F F\nCOUNT 1 1 1 3 1\nWIDTH 1\nHEIGHT 1\nDATA ascii\n8 3 1 0 1 0 2\n"));
        require(result.ok()&&result.cloud->points[0].x==1&&result.cloud->points[0].z==3,"reordered fields and multi-component properties");
        auto floatHeader=pcdHeader("ascii",1); floatHeader.replace("TYPE F F F U","TYPE F F F F");
        result=loadPointCloudFile(save(dir,"float-rgb.pcd",floatHeader+"1 2 3 2.341805152028776e-38\n"));
        require(result.ok()&&result.cloud->points[0].color[0]==255,"packed float RGB");
        QByteArray binary;
        appendFloat(binary,1); appendFloat(binary,2); appendFloat(binary,3); append32(binary,0xff0000);
        appendFloat(binary,4); appendFloat(binary,5); appendFloat(binary,6); append32(binary,0x00ff00);
        result=loadPointCloudFile(save(dir,"binary.pcd",pcdHeader("binary")+binary));
        require(result.ok()&&result.cloud->points[1].z==6&&result.cloud->points[1].color[1]==255,"binary PCD");
        QByteArray planar;
        for(const float n:{1.f,4.f,2.f,5.f,3.f,6.f}) appendFloat(planar,n);
        append32(planar,0xff0000); append32(planar,0x00ff00);
        QByteArray compressed; compressed.append(char(31)); compressed.append(planar); // LZF literal run, field-major data.
        QByteArray compressedSizes; append32(compressedSizes,quint32(compressed.size())); append32(compressedSizes,quint32(planar.size()));
        result=loadPointCloudFile(save(dir,"compressed.pcd",pcdHeader("binary_compressed")+compressedSizes+compressed));
        require(result.ok()&&result.cloud->points[1].x==4&&result.cloud->points[0].color[0]==255,"compressed PCD field ordering");
        QByteArray zeros; zeros.append(char(0)); zeros.append(char(0)); zeros.append(char(224)); zeros.append(char(22)); zeros.append(char(0));
        QByteArray zeroSizes; append32(zeroSizes,quint32(zeros.size())); append32(zeroSizes,32);
        result=loadPointCloudFile(save(dir,"backref.pcd",pcdHeader("binary_compressed")+zeroSizes+zeros));
        require(result.ok()&&result.cloud->points.size()==2&&result.cloud->points[1].z==0,"LZF overlapping back reference");
        result=loadPointCloudFile(save(dir,"truncated.pcd",pcdHeader("binary")+binary.left(10)));
        require(!result.ok(),"truncated binary PCD rejected");
        result=loadPointCloudFile(save(dir,"corrupt.pcd",pcdHeader("binary_compressed")+compressedSizes+compressed.left(10)));
        require(!result.ok(),"truncated compressed PCD rejected");
        compressed[0]=char(255);
        result=loadPointCloudFile(save(dir,"corrupt-lzf.pcd",pcdHeader("binary_compressed")+compressedSizes+compressed));
        require(!result.ok(),"invalid LZF back reference rejected");
        result=loadPointCloudFile(save(dir,"bad-size.pcd","FIELDS x y z\nSIZE 4 4 4\nTYPE F F F\nWIDTH 2\nPOINTS 1\nDATA ascii\n1 2 3\n"));
        require(!result.ok(),"inconsistent PCD dimensions rejected");
        result=loadPointCloudFile(save(dir,"missing-field.pcd","FIELDS x y\nSIZE 4 4\nTYPE F F\nPOINTS 1\nDATA ascii\n1 2\n"));
        require(!result.ok(),"missing coordinate rejected");
        result=loadPointCloudFile(save(dir,"ascii.ply",plyHeader("ascii")+"128 3 1 255 2 0\n64 6 4 0 5 255\n3 0 1 0\n"));
        require(result.ok()&&result.cloud->hasColors&&result.cloud->points[1].y==5&&result.cloud->points[0].color[2]==128,"ASCII PLY property order and colors");
        for(const bool little:{true,false}) {
            QByteArray bytes;
            bytes.append(char(128)); appendFloat(bytes,3,little); appendFloat(bytes,1,little); bytes.append(char(255)); appendFloat(bytes,2,little); bytes.append(char(0));
            bytes.append(char(64)); appendFloat(bytes,6,little); appendFloat(bytes,4,little); bytes.append(char(0)); appendFloat(bytes,5,little); bytes.append(char(255));
            result=loadPointCloudFile(save(dir,"binary.ply",plyHeader(little?"binary_little_endian":"binary_big_endian")+bytes));
            require(result.ok()&&result.cloud->points[1].x==4&&result.cloud->points[0].color[2]==128,"binary PLY byte order");
            result=loadPointCloudFile(save(dir,"truncated.ply",plyHeader(little?"binary_little_endian":"binary_big_endian")+bytes.left(4)));
            require(!result.ok(),"truncated PLY rejected");
        }
        result=loadPointCloudFile(save(dir,"prefix-list.ply","ply\nformat ascii 1.0\nelement face 1\nproperty list uchar int vertex_indices\nelement vertex 1\nproperty double x\nproperty double y\nproperty double z\nend_header\n3 0 0 0\n1 2 3\n"));
        require(result.ok()&&result.cloud->points[0].z==3,"PLY list elements before vertices");
        result=loadPointCloudFile(save(dir,"invalid.ply","ply\nformat ascii 1.0\nend_header\n")); require(!result.ok(),"PLY missing vertices");
        result=loadPointCloudFile(save(dir,"huge.pcd",pcdHeader("ascii",10000001))); require(!result.ok(),"point limit enforced before allocation");
        require(!loadPointCloudFile(dir.filePath("missing.pcd")).ok(),"missing file");
        require(!loadPointCloudFile(save(dir,"unsupported.dat","1 2 3\n")).ok(),"unsupported extension");
        std::cout<<"Point cloud file regression: "<<checks<<" checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
