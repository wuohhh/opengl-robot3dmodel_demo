#include "robotgeometry.h"
#include <QFileInfo>   // [demo 适配] 原项目由 basedefine.h 间接引入
#include "robot3d/rendering/RobotMeshLoader.h"
#include <QOpenGLContext>
#include <QDir>

#define mmin(x,y) ((x)<(y)?(x):(y))

RobotGeometry::RobotGeometry() {
    initializeOpenGLFunctions();
    m_glMajor=QOpenGLContext::currentContext()->format().majorVersion();
    numJoints=6; numTool=0;
}

void RobotGeometry::CalRobotViewTargetPos()
{
    if((entriesData.count()>0) && (entriesData.at(0).numVertices>0))
    {
        float min_x= entriesData.at(0).verticesData[0].position.x();
        float max_x= entriesData.at(0).verticesData[0].position.x();
        float min_y= entriesData.at(0).verticesData[0].position.y();
        float max_y= entriesData.at(0).verticesData[0].position.y();
        float min_z= entriesData.at(0).verticesData[0].position.z();
        float max_z= entriesData.at(0).verticesData[0].position.z();

        for(int entryIndex = 0; entryIndex < entriesData.count(); entryIndex++)
        {
            for(int verIndex = 0; verIndex <  entriesData.at(entryIndex).numVertices; verIndex++)
            {
                if(entriesData.at(entryIndex).verticesData[verIndex].position.x()  <min_x)
                    min_x = entriesData.at(entryIndex).verticesData[verIndex].position.x();

                if(entriesData.at(entryIndex).verticesData[verIndex].position.x()>max_x)
                    max_x= entriesData.at(entryIndex).verticesData[verIndex].position.x();

                if(entriesData.at(entryIndex).verticesData[verIndex].position.y()<min_y)
                    min_y = entriesData.at(entryIndex).verticesData[verIndex].position.y();

                if(entriesData.at(entryIndex).verticesData[verIndex].position.y()>max_y)
                    max_y = entriesData.at(entryIndex).verticesData[verIndex].position.y();

                if(entriesData.at(entryIndex).verticesData[verIndex].position.z()<min_z)
                    min_z = entriesData.at(entryIndex).verticesData[verIndex].position.z();

                if(entriesData.at(entryIndex).verticesData[verIndex].position.z()>max_z)
                    max_z = entriesData.at(entryIndex).verticesData[verIndex].position.z();
            }

        }

        //获取多个model之间的 x y z 方向最值
        QVector3D wholeModelMax=QVector3D(max_x, max_y, max_z);
        QVector3D wholeModelMin=QVector3D(min_x, min_y, min_z);

        Target = QVector3D((wholeModelMin.x()+wholeModelMax.x())*0.5, (wholeModelMin.y()+wholeModelMax.y())*0.5, (wholeModelMin.z()+wholeModelMax.z())*0.5);
        Pos = wholeModelMax;
        Pos= QVector3D(Target.x()+150,Target.y(), Target.z());
    }
    else
    {
        Target = QVector3D(0,0,0);
        Pos = QVector3D(10,10,10);
    }
}

RobotGeometry::~RobotGeometry() { clearMesh(); }
void RobotGeometry::clearMesh() {
    for(auto& entry:entriesData) {
        entry.VB.destroy(); delete[] entry.verticesData; delete[] entry.verticesDataBackup;
    }
    entriesData.clear(); jointData.clear();
}
bool RobotGeometry::LoadMesh(const std::string& file,MeshEntry& entry,int joint) {
    Q_UNUSED(joint)
    robot3d::rendering::CpuRobotMesh mesh; QString error;
    if(!robot3d::rendering::loadRobotMesh(QString::fromStdString(file),mesh,error)) { qWarning()<<error; return false; }
    entry.numVertices=mesh.vertices.size();
    entry.verticesData=new VertexData[entry.numVertices];
    entry.verticesDataBackup=new VertexData[entry.numVertices];
    for(unsigned i=0;i<entry.numVertices;++i) {
        entry.verticesData[i]={mesh.vertices[int(i)].position*G_SCALE,mesh.vertices[int(i)].normal,mesh.vertices[int(i)].material};
        entry.verticesDataBackup[i]=entry.verticesData[i];
    }
    if(!entry.VB.create() || !entry.VB.bind()) { delete[] entry.verticesData; delete[] entry.verticesDataBackup; entry.verticesData=nullptr; entry.verticesDataBackup=nullptr; return false; }
    entry.VB.allocate(entry.verticesData,sizeof(VertexData)*entry.numVertices); entry.VB.release();
    return true;
}

QMatrix4x4 RobotGeometry::setTransformMatrix(QVector3D center, QVector3D normal,float degree)
{
    float a=center.x();
    float b=center.y();
    float c=center.z();

    float u=normal.x();
    float v=normal.y();
    float w=normal.z();

    float a11=u*u+(v*v+w*w)*(float)(qCos(M_PI/180*degree));
    float a12=u*v*(1-(float)(qCos(M_PI/180*degree)))-w*(float)(qSin(M_PI/180*degree));
    float a13=u*w*(1-(float)(qCos(M_PI/180*degree)))+v*(float)(qSin(M_PI/180*degree));
    float a14=(a*(v*v+w*w)-u*(b*v+c*w))*(1-(float)(qCos(M_PI/180*degree)))+(b*w-c*v)*(float)(qSin(M_PI/180*degree));

    float a21=u*v*(1-(float)(qCos(M_PI/180*degree)))+w*(float)(qSin(M_PI/180*degree));
    float a22=v*v+(u*u+w*w)*(float)(qCos(M_PI/180*degree));
    float a23=v*w*(1-(float)(qCos(M_PI/180*degree)))-u*(float)(qSin(M_PI/180*degree));
    float a24=(b*(u*u+w*w)-v*(a*u+c*w))*(1-(float)(qCos(M_PI/180*degree)))+(c*u-a*w)*(float)(qSin(M_PI/180*degree));

    float a31=u*w*(1-(float)(qCos(M_PI/180*degree)))-v*(float)(qSin(M_PI/180*degree));
    float a32=v*w*(1-(float)(qCos(M_PI/180*degree)))+u*(float)(qSin(M_PI/180*degree));
    float a33=w*w+(u*u+v*v)*(float)(qCos(M_PI/180*degree));
    float a34=(c*(u*u+v*v)-w*(a*u+b*v))*(1-(float)(qCos(M_PI/180*degree)))+(a*v-b*u)*(float)(qSin(M_PI/180*degree));

    QMatrix4x4 rotateMatrix={a11,  a12,   a13,  a14,
                             a21,  a22,   a23,  a24,
                             a31,  a32,   a33,  a34,
                              0,    0,     0,    1};
    return rotateMatrix;
}


//该函数需放在multjointTransform之后
void RobotGeometry::set_rotate_center(QVector3D &rotate_center)
{
    int rotateJointIndex = 0;
    //求取模型旋转中心点,第3关节旋转中心点，作为机器人整体旋转中心
    QVector4D centerRotateVec = jointData[rotateJointIndex].transform*QVector4D(jointData[rotateJointIndex].center,1);
    rotate_center = centerRotateVec.toVector3D();
}

//多关节运动
void RobotGeometry::multjointTransform2(double array[6])
{

    //保证每次变换的数据都是相对于原始的数据
    for(int joint=1;joint<entriesData.size();++joint)
    {
       memcpy(entriesData[joint].verticesData, entriesData[joint].verticesDataBackup, sizeof(VertexData)*entriesData[joint].numVertices);
    }

    for(int i=0;i<numJoints+numTool;i++)
    {
        jointData[i].transform=setTransformMatrix(jointData[i].center,jointData[i].normal, i < numJoints ? array[i] : 0);
        while(i>0)
        {
            jointData[i].transform=jointData[i-1].transform*jointData[i].transform;
            break;
        }
    }

   for(int joint=1;joint<entriesData.size();++joint)
     {
         float *matrixPtr=jointData[joint-1].transform.data();
             for(int numVer=0;numVer<entriesData[joint].numVertices;++numVer)
             {
                 float ver[3]={entriesData[joint].verticesData[numVer].position.x(),
                               entriesData[joint].verticesData[numVer].position.y(),
                               entriesData[joint].verticesData[numVer].position.z()};
                 entriesData[joint].verticesData[numVer].position.setX(matrixPtr[0]*ver[0]+matrixPtr[4]*ver[1]+matrixPtr[8]*ver[2]+matrixPtr[12]);
                 entriesData[joint].verticesData[numVer].position.setY(matrixPtr[1]*ver[0]+matrixPtr[5]*ver[1]+matrixPtr[9]*ver[2]+matrixPtr[13]);
                 entriesData[joint].verticesData[numVer].position.setZ(matrixPtr[2]*ver[0]+matrixPtr[6]*ver[1]+matrixPtr[10]*ver[2]+matrixPtr[14]);
                 //  MatrixMultiVector3D(matrixPtr,ver, m_Entries[joint][num_mesh].Vertices_meshEntry[num_ver].position);
             }

            //对更新后的顶点数据重新绑定
            entriesData[joint].VB.bind();
            entriesData[joint].VB.allocate(entriesData[joint].verticesData,sizeof(VertexData)*entriesData[joint].numVertices);
     }
}


//多关节运动
void RobotGeometry::multjointTransform(double array[6])
{

    //保证每次变换的数据都是相对于原始的数据
    for(int joint=1;joint<entriesData.size();++joint)
    {
       memcpy(entriesData[joint].verticesData, entriesData[joint].verticesDataBackup, sizeof(VertexData)*entriesData[joint].numVertices);
    }

    for(int i=0;i<numJoints+numTool;i++)
    {
        jointData[i].transform=setTransformMatrix(jointData[i].center,jointData[i].normal,i < numJoints ? array[i] : 0);
        while(i>0)
        {
            jointData[i].transform=jointData[i-1].transform*jointData[i].transform;
            break;
        }
    }

   for(int joint=1;joint<entriesData.size();++joint)
     {
         float *matrixPtr=jointData[joint-1].transform.data();
             for(int numVer=0;numVer<entriesData[joint].numVertices;++numVer)
             {
                 float ver[3]={entriesData[joint].verticesData[numVer].position.x(),
                               entriesData[joint].verticesData[numVer].position.y(),
                               entriesData[joint].verticesData[numVer].position.z()};
                 entriesData[joint].verticesData[numVer].position.setX(matrixPtr[0]*ver[0]+matrixPtr[4]*ver[1]+matrixPtr[8]*ver[2]+matrixPtr[12]);
                 entriesData[joint].verticesData[numVer].position.setY(matrixPtr[1]*ver[0]+matrixPtr[5]*ver[1]+matrixPtr[9]*ver[2]+matrixPtr[13]);
                 entriesData[joint].verticesData[numVer].position.setZ(matrixPtr[2]*ver[0]+matrixPtr[6]*ver[1]+matrixPtr[10]*ver[2]+matrixPtr[14]);
                 //  MatrixMultiVector3D(matrixPtr,ver, m_Entries[joint][num_mesh].Vertices_meshEntry[num_ver].position);
             }

            //对更新后的顶点数据重新绑定
            int num_ver=entriesData[joint].numVertices;
             entriesData[joint].VB.bind();
            if(m_glMajor>2)
            {
            auto ptr =entriesData[joint].VB.map(QOpenGLBuffer::WriteOnly);
            memcpy(ptr, entriesData[joint].verticesData,sizeof(VertexData)*num_ver);
            entriesData[joint].VB.unmap();
            entriesData[joint].VB.release();           
            }
            else
            {

              entriesData[joint].VB.allocate(entriesData[joint].verticesData,sizeof(VertexData)*entriesData[joint].numVertices);
            }
     }
}


bool RobotGeometry::initShaders()
{
    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/robot_vertex_shader.glsl"))
        return false;

    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/robot_fragment_shader.glsl"))
        return false;

    if (!program.link())
        return false;

    if (!program.bind())
        return false;

    return  true;
}

void RobotGeometry::drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
{
    program.bind();

    program.setUniformValue("mvp_matrix", mvpMatrix);
    program.setUniformValue("model", model);

    //更改着色器
    //第一个平行光设置
    program.setUniformValue("light1.ambient",QVector3D(0.7f,0.7f,0.7f));
    program.setUniformValue("light1.diffuse",QVector3D(0.5f,0.5f,0.5f));
    program.setUniformValue("light1.specular",QVector3D(0.2f,0.2f,0.2f));

    QVector3D direction=this->Target-this->Pos+QVector3D(2,2,2);

    //program.setUniformValue("light1.direction",direction);

    direction.setX((lightMatrix*QVector4D(direction,1.0)).x());
    direction.setY((lightMatrix*QVector4D(direction,1.0)).y());
    direction.setZ((lightMatrix*QVector4D(direction,1.0)).z());
    program.setUniformValue("light1.direction",direction);

    program.setUniformValue("material.ambient",QVector3D(0.8f, 0.8f, 0.8f));
    program.setUniformValue("material.diffuse",QVector3D(0.9f, 0.9f, 0.9f));
    program.setUniformValue("material.specular",QVector3D(0.5f,0.5f,0.5f));
    program.setUniformValue("material.shininess",32.0f);

    program.setUniformValue("viewPos",this->Pos);

    if(entriesData.size() < 1)  //未加载模型
        return;

    for(int num_model=0;num_model<1+numJoints+mmin(numTool,int(endWorkpieceVisible_));num_model++) // base joints tool
    {

            entriesData[num_model].VB.bind();

            quintptr offset = 0;
            int vertexLocation = program.attributeLocation("a_position");
            program.enableAttributeArray(vertexLocation);
            program.setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(VertexData));

            offset += sizeof(QVector3D);
            int normalLocation = program.attributeLocation("a_normal");
            program.enableAttributeArray(normalLocation);
            program.setAttributeBuffer(normalLocation,GL_FLOAT,offset,3,sizeof(VertexData));

            offset +=sizeof(QVector3D);
            int materialLocation = program.attributeLocation("a_material");
            program.enableAttributeArray(materialLocation);
            program.setAttributeBuffer(materialLocation,GL_FLOAT,offset,3,sizeof(VertexData));

            // Draw cube geometry using indices from VBO 1
            //glDrawElements(GL_TRIANGLES, entriesData[num_model].indicesMeshEntry.size(), GL_UNSIGNED_SHORT, 0);
            glDrawArrays(GL_TRIANGLES, 0,entriesData[num_model].numVertices);
    }
}

//第二个机器人的渲染函数
void RobotGeometry::drawSecondGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
{
    program.bind();
    program.setUniformValue("mvp_matrix", mvpMatrix);
    program.setUniformValue("model", model);

    //更改着色器
    //第一个平行光设置
//    program.setUniformValue("light1.ambient", QVector3D(0.9f,0.9f,0.9f));
//    program.setUniformValue("light1.diffuse", QVector3D(0.7f,0.7f,0.7f));
//    program.setUniformValue("light1.specular",QVector3D(0.7f,0.7f,0.7f));
    program.setUniformValue("light1.ambient", QVector3D(0.1f,0.73f,1.0f));
    program.setUniformValue("light1.diffuse", QVector3D(0.1f,0.73f,1.0f));
    program.setUniformValue("light1.specular",QVector3D(0.1f,0.73f,1.0f));

    QVector3D direction=this->Target-this->Pos+QVector3D(2,2,2);

    program.setUniformValue("light1.direction",direction);

    program.setUniformValue("material.ambient",QVector3D(0.9f,0.9f,0.9f));
    program.setUniformValue("material.diffuse",QVector3D(0.8f,0.8f,0.8f));
    program.setUniformValue("material.specular",QVector3D(0.8f,0.8f,0.8f));
    program.setUniformValue("material.shininess",32.0f);

    program.setUniformValue("viewPos",this->Pos);

    if(entriesData.size() < 1)  //未加载模型
        return;

    for(int num_model=0;num_model<1+numJoints+mmin(numTool,int(endWorkpieceVisible_));num_model++) // base joints tool
    {
            entriesData[num_model].VB.bind();

            quintptr offset = 0;
            int vertexLocation = program.attributeLocation("a_position");
            program.enableAttributeArray(vertexLocation);
            program.setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(VertexData));

            offset += sizeof(QVector3D);
            int normalLocation = program.attributeLocation("a_normal");
            program.enableAttributeArray(normalLocation);
            program.setAttributeBuffer(normalLocation,GL_FLOAT,offset,3,sizeof(VertexData));

            offset +=sizeof(QVector3D);
            int materialLocation = program.attributeLocation("a_material");
            program.enableAttributeArray(materialLocation);
            program.setAttributeBuffer(materialLocation,GL_FLOAT,offset,3,sizeof(VertexData));

            // Draw cube geometry using indices from VBO 1
           // glDrawElements(GL_TRIANGLES, entriesData[num_model].indicesMeshEntry.size(), GL_UNSIGNED_SHORT, 0);
            glDrawArrays(GL_TRIANGLES, 0,entriesData[num_model].numVertices);
    }
}
void RobotGeometry::MatrixMultiVector3D(float *matrix4x4,float *vector3d, QVector3D &result)
{
   result.setX((*(matrix4x4+0))*(*(vector3d+0))+(*(matrix4x4+4))*(*(vector3d+1))+(*(matrix4x4+8))*(*(vector3d+2))+(*(matrix4x4+12)));
   result.setY((*(matrix4x4+1))*(*(vector3d+0))+(*(matrix4x4+5))*(*(vector3d+1))+(*(matrix4x4+9))*(*(vector3d+2))+(*(matrix4x4+13)));
   result.setZ((*(matrix4x4+2))*(*(vector3d+0))+(*(matrix4x4+6))*(*(vector3d+1))+(*(matrix4x4+10))*(*(vector3d+2))+(*(matrix4x4+14)));
}

bool RobotGeometry::ChangeRobot3D(QString directory, const WholeDH stdRobotWholeDH[6])
{
    clearMesh();
    QDir root(directory);
    QString file[8];
    file[0]=root.filePath("base.obj");
    for(int i=1;i<=6;++i) file[i]=root.filePath(QString("joint%1.obj").arg(i));
    file[7]=root.filePath("tool.obj");

    QFileInfo toolFile(file[7]);
    numTool = toolFile.isFile() ? 1 : 0;

    for(int i=0;i<1+numJoints+numTool;++i) // base joints tool
    {
        MeshEntry meshModel;
        bool result = LoadMesh(file[i].toStdString(), meshModel,i);
        if(result)
        {
            entriesData.push_back(meshModel);
        }
        else
        {
            clearMesh(); return false;
        }
    }
    CalRobotViewTargetPos();

    QMatrix4x4 identify;

    multiJointData joint1={ QVector3D(0.0f,0.0f,0.0f)*G_SCALE, QVector3D(0,0,1),identify};
    multiJointData joint2={ QVector3D(0.0f,0.0f,(float)(stdRobotWholeDH[0].d))*G_SCALE, QVector3D(0,-1,0),identify};
    multiJointData joint3={ QVector3D(0.0f,0.0f,(float)(stdRobotWholeDH[0].d+stdRobotWholeDH[2].a))*G_SCALE,  QVector3D(0,-1,0),identify};
    multiJointData joint4={ QVector3D(0.0f,0.0f,(float)(stdRobotWholeDH[0].d+stdRobotWholeDH[2].a+stdRobotWholeDH[3].a))*G_SCALE,QVector3D(0,-1,0),identify};
    multiJointData joint5={ QVector3D(0.0f,(float)(stdRobotWholeDH[3].d*(-1)),0.0f)*G_SCALE, QVector3D(0,0,1),identify};
    multiJointData joint6={ QVector3D(0.0f,0.0f,(float)(stdRobotWholeDH[0].d+stdRobotWholeDH[2].a+stdRobotWholeDH[3].a+stdRobotWholeDH[4].d))*G_SCALE, QVector3D(0,-1,0),identify};

    jointData.push_back(joint1);
    jointData.push_back(joint2);
    jointData.push_back(joint3);
    jointData.push_back(joint4);
    jointData.push_back(joint5);
    jointData.push_back(joint6);
    if (numTool)
    {
        multiJointData tool =
        { QVector3D(0.0f,0.0f,(float)(stdRobotWholeDH[0].d+stdRobotWholeDH[2].a+stdRobotWholeDH[3].a+stdRobotWholeDH[4].d))*G_SCALE,QVector3D(0,-1,0),identify};
        jointData.push_back(tool);
    }
    else
    {
        endWorkpieceVisible_ = false;
    }

    return true;
}
