#include "robotgeometry.h"
#include <QFileInfo>   // [demo 适配] 原项目由 basedefine.h 间接引入
#include "demo_support.h"

#define mmin(x,y) ((x)<(y)?(x):(y))

RobotGeometry::RobotGeometry() //: indexBuf(QOpenGLBuffer::IndexBuffer)
{
    initializeOpenGLFunctions();
    numJoints = 6;
    numTool = 0;

//    WholeDH stdRobotWholeDH1[6]={ {0,        0,   161.2, 0,        0},
//                              {M_PI/2.0, 0,   0, M_PI/2.0, 0},
//                              {0,        425,  0,    0,        0},
//                              {0,        393,  114.3,    -M_PI/2.0,0},
//                              {-M_PI/2.0, 0,  99,    0,        0},
//                              {M_PI/2.0,  0,  95.3,  0,        0} };
 //   this->ChangeRobot3D("G6",stdRobotWholeDH1);

    this->ChangeRobot3D(CommDataController::GetInstance()->robotType.robotTypeParaStr,CommDataController::GetInstance()->stdRobotWholeDH);
//    QString appPath = "./"; //= QCoreApplication::applicationDirPath();
//    //加载多个模型文件
//    QString file[7]={
//        appPath+"/robotType/G6/model3d/base.obj",
//        appPath+"/robotType/G6/model3d/joint1.obj",
//        appPath+"/robotType/G6/model3d/joint2.obj",
//        appPath+"/robotType/G6/model3d/joint3.obj",
//        appPath+"/robotType/G6/model3d/joint4.obj",
//        appPath+"/robotType/G6/model3d/joint5.obj",
//        appPath+"/robotType/G6/model3d/joint6.obj"
//    };

//    for(int i=0;i<7;++i)
//    {
//        MeshEntry meshModel;
//        bool result = LoadMesh(file[i].toStdString(), meshModel,i);
//        if(result)
//        {
//            entriesData.push_back(meshModel);
//        }
//        else
//        {
//            qDebug()<<("load 3D model fail!");
//        }
//    }

//    //entriesDataBackup=entriesData;
//    CalRobotViewTargetPos();

//    QMatrix4x4 identify;

//    multiJointData joint1={ QVector3D(0.0f,0.0f,0.0f)*G_SCALE, QVector3D(0,0,1),identify};
//    multiJointData joint2={ QVector3D(0.0f,0.0f,161.2f)*G_SCALE, QVector3D(0,-1,0),identify};
//    multiJointData joint3={ QVector3D(0.0f,0.0f,586.2f)*G_SCALE,  QVector3D(0,-1,0),identify};
//    multiJointData joint4={ QVector3D(0.0f,0.0f,979.2f)*G_SCALE,  QVector3D(0,-1,0),identify};
//    multiJointData joint5={ QVector3D(0.0f,-113.3f,0.0f)*G_SCALE, QVector3D(0,0,1),identify};
//    multiJointData joint6={ QVector3D(0.0f,0.0f,1078.2f)*G_SCALE, QVector3D(0,-1,0),identify};

//    jointData.push_back(joint1);
//    jointData.push_back(joint2);
//    jointData.push_back(joint3);
//    jointData.push_back(joint4);
//    jointData.push_back(joint5);
//    jointData.push_back(joint6);
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

RobotGeometry::~RobotGeometry()
{
    for(int numModel=0;numModel<entriesData.size();numModel++)
    {
        entriesData[numModel].VB.destroy();
        delete  entriesData[numModel].verticesData;
        delete  entriesData[numModel].verticesDataBackup;
    }
}


bool RobotGeometry::LoadMesh(const std::string& Filename, MeshEntry &meshModel,int joint)
{

    Assimp::Importer Importer;
    const aiScene* pScene = Importer.ReadFile(Filename.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);
    //三角化，每个面的时候平滑，UV翻转（纹理）

    QMatrix4x4 scaleMatrix;
    scaleMatrix.scale(G_SCALE);

    if (pScene)
    {
        //求每个模型文件的顶点数
        for (int numMesh = 0; numMesh< pScene->mNumMeshes; numMesh++)
        {
            const aiMesh* paiMesh = pScene->mMeshes[numMesh];
            meshModel.numVertices+=paiMesh->mNumVertices;
        }

        meshModel.verticesData =new VertexData[meshModel.numVertices];
        int num=0;
        for (int numMesh = 0; numMesh< pScene->mNumMeshes; numMesh++)
        {
            const aiMesh* paiMesh = pScene->mMeshes[numMesh];

            aiColor3D color;
            aiMaterial* material = pScene->mMaterials[paiMesh->mMaterialIndex];
            material->Get(AI_MATKEY_COLOR_DIFFUSE, color);

            for (unsigned int i = 0; i < paiMesh->mNumVertices; i++)
            {
                const aiVector3D* pPos = &(paiMesh->mVertices[i]);
                const aiVector3D* pNormal = &(paiMesh->mNormals[i]);

                QVector4D data_ver(pPos->x,pPos->y,pPos->z,1.0);
                QVector4D data_ver_transform=scaleMatrix*data_ver;

                meshModel.verticesData[i+num].position.setX(data_ver_transform.x());
                meshModel.verticesData[i+num].position.setY(data_ver_transform.y());
                meshModel.verticesData[i+num].position.setZ(data_ver_transform.z());

                meshModel.verticesData[i+num].material.setX(color.r);
                meshModel.verticesData[i+num].material.setY(color.g);
                meshModel.verticesData[i+num].material.setZ(color.b);

                meshModel.verticesData[i+num].normal.setX(pNormal->x);
                meshModel.verticesData[i+num].normal.setY(pNormal->y);
                meshModel.verticesData[i+num].normal.setZ(pNormal->z);

            }//获取一个mesh对应点的几何信息
             num=num+paiMesh->mNumVertices;
        }

        //对数据进行备份
        meshModel.verticesDataBackup =new VertexData[meshModel.numVertices];
        memcpy(meshModel.verticesDataBackup, meshModel.verticesData, sizeof(VertexData)*meshModel.numVertices);

        //根据顶点序列构建buffer
        meshModel.VB.create();
        meshModel.VB.bind();
        meshModel.VB.allocate(meshModel.verticesData,sizeof(VertexData)*meshModel.numVertices);
        meshModel.VB.release();

        return true;
    }
    else
    {
        qDebug()<<QString("Error parsing %1:%2\n").arg(Filename.c_str()).arg(Importer.GetErrorString());
        return false;
    }
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
            if(g_openglVersion>2)
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

    for(int num_model=0;num_model<1+numJoints+mmin(numTool,showEndToolStatus);num_model++) // base joints tool
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

    for(int num_model=0;num_model<1+numJoints+mmin(numTool,showEndToolStatus);num_model++) // base joints tool
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

void RobotGeometry::ChangeRobot3D(QString name, WholeDH stdRobotWholeDH[6])
{
    this->entriesData.clear();
    this->jointData.clear();

    QString appPath = "./";
    //加载多个模型文件
    QString file[8]={
        appPath+QString("/robotType/%1/model3d/base.obj").arg(name),
        appPath+QString("/robotType/%1/model3d/joint1.obj").arg(name),
        appPath+QString("/robotType/%1/model3d/joint2.obj").arg(name),
        appPath+QString("/robotType/%1/model3d/joint3.obj").arg(name),
        appPath+QString("/robotType/%1/model3d/joint4.obj").arg(name),
        appPath+QString("/robotType/%1/model3d/joint5.obj").arg(name),
        appPath+QString("/robotType/%1/model3d/joint6.obj").arg(name),
        appPath+QString("/robotType/%1/model3d/tool.obj").arg(name),
    };

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
            qDebug()<<("load 3D model fail!");
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
        showEndToolStatus = 0;
    }

}
