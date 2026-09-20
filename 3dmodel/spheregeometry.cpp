#include"spheregeometry.h"

SphereGeometry::SphereGeometry(SphereMsg sphereMsgIn):BaseGeometry()
{
    this->sphereMsg = sphereMsgIn;

    GenerateSphereData(sphereMsgIn.center,sphereMsgIn.radius);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(vertex_data.data(), num_vertex_data*sizeof(FeatureVertexData));
    vertex_data_Buf.release();

}

SphereGeometry::SphereGeometry(QVector<SphereMsg> sphereMsgsIn,int maxNum):BaseGeometry()
{
    if(sphereMsgsIn.count()>0)
    {
        this->sphereMsg=sphereMsgsIn[0];
    }
    this->maxNum=maxNum;
    GenerateSpheresData(sphereMsgsIn);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(nullptr,sizeof(FeatureVertexData)*numVertex*maxNum);
    vertex_data_Buf.release();
}

SphereGeometry::~SphereGeometry()
{

}

void SphereGeometry::SetSphereData(QVector<SphereMsg> sphereMsgsIn)
{
    if(sphereMsgsIn.count()>0)
     {
         this->sphereMsg=sphereMsgsIn[0];
     }
    GenerateSpheresData(sphereMsgsIn);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    if(g_openglVersion>2)
    {
      auto ptrVertex=vertex_data_Buf.map(QOpenGLBuffer::WriteOnly);
      memcpy(ptrVertex,vertex_data.data(),sizeof(FeatureVertexData)*num_vertex_data);
      vertex_data_Buf.unmap();
      vertex_data_Buf.release();
     }
    else
    {
       vertex_data_Buf.allocate(vertex_data.data(), num_vertex_data * sizeof(FeatureVertexData));
    }

}

void SphereGeometry::SetSphereData(QVector3D center,float radius)
{

    GenerateSphereData(center,radius);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    if(g_openglVersion>2)
    {
        auto ptrVertex=vertex_data_Buf.map(QOpenGLBuffer::WriteOnly);
        memcpy(ptrVertex,vertex_data.data(),sizeof(FeatureVertexData)*num_vertex_data);
        vertex_data_Buf.unmap();
        vertex_data_Buf.release();
    }
    else
    {
        vertex_data_Buf.allocate(vertex_data.data(), num_vertex_data * sizeof(FeatureVertexData));
    }
}

void SphereGeometry::AddDeleteSphereData(QVector<SphereMsg> sphereMsgsIn)
{
    if(sphereMsgsIn.count()>0)
    {
        this->sphereMsg=sphereMsgsIn[0];
    }
    GenerateSpheresData(sphereMsgsIn);
    num_vertex_data = vertex_data.size();
    if(g_openglVersion>2)
    {
        vertex_data_Buf.bind();
        auto ptrVertex = vertex_data_Buf.map(QOpenGLBuffer::WriteOnly);
        memcpy(ptrVertex,vertex_data.data(),sizeof(FeatureVertexData)*num_vertex_data);
        vertex_data_Buf.unmap();
        vertex_data_Buf.release();
    }
    else
    {
        vertex_data_Buf.destroy();
        vertex_data_Buf.create();
        vertex_data_Buf.bind();
        vertex_data_Buf.allocate(vertex_data.data(), num_vertex_data * sizeof(FeatureVertexData));
  }
}

bool SphereGeometry::initShaders()
{
    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/line_vertex_shader.glsl"))
        return false;

    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/line_fragment_shader.glsl"))
        return false;

    if (!program.link())
        return false;

    if (!program.bind())
        return false;

    return  true;
}

void SphereGeometry::drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
{
    program.bind();
    program.setUniformValue("lightColor", QVector3D(1.0,1.0,1.0));
    program.setUniformValue("toyColor", QVector3D(this->sphereMsg.color.red()/255.0, this->sphereMsg.color.green()/255.0, this->sphereMsg.color.blue()/255.0));
    program.setUniformValue("mvp_matrix", mvpMatrix);

    vertex_data_Buf.bind();

    quintptr offset = 0;
    int vertexLocation = program.attributeLocation("a_position");
    program.enableAttributeArray(vertexLocation);
    program.setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(FeatureVertexData));

   glDrawArrays(GL_TRIANGLES, 0,num_vertex_data);
}

void SphereGeometry::GenerateSphereData(QVector3D center,float radius)
{
    vertex_data.clear();     //顶点数据需要清空，插入新的顶点数据
    int angleSpan = 30;   // 将球进行单位切分的角度
    for (int vAngle = -90; vAngle <90; vAngle = vAngle + angleSpan)// 垂直方向angleSpan度一份
    {
        for (int hAngle = 0; hAngle < 360; hAngle = hAngle + angleSpan)// 水平方向angleSpan度一份
        {
            float x0 = center.x()+radius* static_cast<float>(qCos(M_PI/180*vAngle)) * static_cast<float>(qSin(M_PI/180*hAngle));
            float y0=  center.y()+radius* static_cast<float>(qSin(M_PI/180*vAngle));
            float z0 = center.z()+radius* static_cast<float>(qCos(M_PI/180*vAngle)) * static_cast<float>(qCos(M_PI/180*hAngle));

            float x1 = center.x()+radius* static_cast<float>(qCos(M_PI/180*vAngle)) * static_cast<float>(qSin(M_PI/180*(hAngle+angleSpan)));
            float y1=  center.y()+radius* static_cast<float>(qSin(M_PI/180*vAngle));
            float z1 = center.z()+radius* static_cast<float>(qCos(M_PI/180*vAngle)) * static_cast<float>(qCos(M_PI/180*(hAngle+angleSpan)));


            float x2 = center.x()+radius* static_cast<float>(qCos(M_PI/180*(vAngle + angleSpan))) * static_cast<float>(qSin(M_PI/180*(hAngle+angleSpan)));
            float y2 =  center.y()+radius* static_cast<float>(qSin(M_PI/180*(vAngle + angleSpan)));
            float z2 = center.z()+radius* static_cast<float>(qCos(M_PI/180*(vAngle + angleSpan))) * static_cast<float>(qCos(M_PI/180*(hAngle+angleSpan)));

            float x3 = center.x()+radius* static_cast<float>(qCos(M_PI/180*(vAngle + angleSpan))) * static_cast<float>(qSin(M_PI/180*(hAngle)));
            float y3=  center.y()+radius* static_cast<float>(qSin(M_PI/180*(vAngle + angleSpan)));
            float z3 = center.z()+radius* static_cast<float>(qCos(M_PI/180*(vAngle + angleSpan))) * static_cast<float>(qCos(M_PI/180*(hAngle)));


            FeatureVertexData temp0;
            temp0.position = QVector3D(x0,y0,z0)*G_SCALE;

            FeatureVertexData temp1;
            temp1.position = QVector3D(x1,y1,z1)*G_SCALE;

            FeatureVertexData temp2;
            temp2.position = QVector3D(x2,y2,z2)*G_SCALE;

            FeatureVertexData temp3;
            temp3.position = QVector3D(x3,y3,z3)*G_SCALE;

            vertex_data.push_back(temp0);
            vertex_data.push_back(temp1);
            vertex_data.push_back(temp2);
            vertex_data.push_back(temp0);
            vertex_data.push_back(temp2);
            vertex_data.push_back(temp3);
        }
    }
}
void SphereGeometry::GenerateSpheresData(QVector<SphereMsg> sphereMsgsIn)
{
    vertex_data.clear();      //顶点数据需要清空，插入新的顶点数据
    for(int sphereIndex=0; (sphereIndex<sphereMsgsIn.size()) && (sphereIndex<this->maxNum); sphereIndex++)
    {
        int angleSpan = 30;      // 将球进行单位切分的角度
        for (int vAngle = -90; vAngle < 90; vAngle = vAngle + angleSpan)// 垂直方向angleSpan度一份
        {
            for (int hAngle = 0; hAngle < 360; hAngle = hAngle + angleSpan)// 水平方向angleSpan度一份
            {
                float x0 = sphereMsgsIn[sphereIndex].center.x()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qCos(M_PI/180*vAngle)) * static_cast<float>(qSin(M_PI/180*hAngle));
                float y0=  sphereMsgsIn[sphereIndex].center.y()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qSin(M_PI/180*vAngle));
                float z0 = sphereMsgsIn[sphereIndex].center.z()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qCos(M_PI/180*vAngle)) * static_cast<float>(qCos(M_PI/180*hAngle));

                float x1 = sphereMsgsIn[sphereIndex].center.x()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qCos(M_PI/180*vAngle)) * static_cast<float>(qSin(M_PI/180*(hAngle+angleSpan)));
                float y1=  sphereMsgsIn[sphereIndex].center.y()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qSin(M_PI/180*vAngle));
                float z1 = sphereMsgsIn[sphereIndex].center.z()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qCos(M_PI/180*vAngle)) * static_cast<float>(qCos(M_PI/180*(hAngle+angleSpan)));

                float x2 = sphereMsgsIn[sphereIndex].center.x()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qCos(M_PI/180*(vAngle + angleSpan))) * static_cast<float>(qSin(M_PI/180*(hAngle+angleSpan)));
                float y2=  sphereMsgsIn[sphereIndex].center.y()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qSin(M_PI/180*(vAngle + angleSpan)));
                float z2 = sphereMsgsIn[sphereIndex].center.z()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qCos(M_PI/180*(vAngle + angleSpan))) * static_cast<float>(qCos(M_PI/180*(hAngle+angleSpan)));

                float x3 = sphereMsgsIn[sphereIndex].center.x()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qCos(M_PI/180*(vAngle + angleSpan))) * static_cast<float>(qSin(M_PI/180*(hAngle)));
                float y3=  sphereMsgsIn[sphereIndex].center.y()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qSin(M_PI/180*(vAngle + angleSpan)));
                float z3 = sphereMsgsIn[sphereIndex].center.z()+sphereMsgsIn[sphereIndex].radius* static_cast<float>(qCos(M_PI/180*(vAngle + angleSpan))) * static_cast<float>(qCos(M_PI/180*(hAngle)));

                FeatureVertexData temp0;
                temp0.position = QVector3D(x0,y0,z0)*G_SCALE;

                FeatureVertexData temp1;
                temp1.position = QVector3D(x1,y1,z1)*G_SCALE;

                FeatureVertexData temp2;
                temp2.position = QVector3D(x2,y2,z2)*G_SCALE;

                FeatureVertexData temp3;
                temp3.position = QVector3D(x3,y3,z3)*G_SCALE;

                vertex_data.push_back(temp0);
                vertex_data.push_back(temp1);
                vertex_data.push_back(temp2);
                vertex_data.push_back(temp0);
                vertex_data.push_back(temp2);
                vertex_data.push_back(temp3);
                //1 3 0；1 2 3
            }
        }
    }
}
