#include"planegeometry.h"

PlaneGeometry::PlaneGeometry(PlaneMsg planeMsgIn):BaseGeometry()
{
    this->planeMsg = planeMsgIn;
   // SetPlaneData(planeMsgIn);
    GeneratePlaneData(planeMsgIn);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(vertex_data.data(), num_vertex_data*sizeof(FeatureVertexData));
    vertex_data_Buf.release();
}

PlaneGeometry::PlaneGeometry(QVector<PlaneMsg> planeMsgsIn,int numMax):BaseGeometry()
{
    this->maxNum=numMax;
    this->planeMsgs.clear();
    this->planeMsgs.append(planeMsgsIn);

    if(planeMsgsIn.count()>0)
        this->planeMsg=planeMsgsIn[0];

    GeneratePlanesData(planeMsgsIn);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(nullptr,sizeof(FeatureVertexData)*numVertex*numMax);
    vertex_data_Buf.release();

}

PlaneGeometry::~PlaneGeometry()
{

}

void PlaneGeometry::SetPlaneData(PlaneMsg planeMsgIn)
{
    this->planeMsg = planeMsgIn;
    GeneratePlaneData(planeMsgIn);
    num_vertex_data = vertex_data.size();
    //绑定顶点数据
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

void PlaneGeometry::SetPlaneData(QVector<PlaneMsg> planeMsgsIn)
{

    GeneratePlanesData(planeMsgsIn);
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
void PlaneGeometry::AddDeletePlaneData(QVector<PlaneMsg> planeMsgsIn)
{
    GeneratePlanesData(planeMsgsIn);
    num_vertex_data = vertex_data.size();
    if(g_openglVersion>2)
    {
        vertex_data_Buf.bind();
        //vertex_data_Buf.allocate(nullptr,sizeof(FeatureVertexData)*num_vertex_data);
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

bool PlaneGeometry::initShaders()
{

    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/plane_vertex_shader.glsl"))
        return false;

    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/plane_fragment_shader.glsl"))
        return false;

    if (!program.link())
        return false;

    if (!program.bind())
        return false;

    return true;
}

void PlaneGeometry::drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
{

    program.bind();
    program.setUniformValue("lightColor", QVector3D(1.0,1.0,1.0));
    program.setUniformValue("toyColor", QVector3D(this->planeMsg.color.red()/255.0, this->planeMsg.color.green()/255.0, this->planeMsg.color.blue()/255.0));
    program.setUniformValue("mvp_matrix",mvpMatrix);
    program.setUniformValue("alpha",this->planeMsg.alpha);

    vertex_data_Buf.bind();

    quintptr offset = 0;
    int vertexLocation = program.attributeLocation("a_position");
    program.enableAttributeArray(vertexLocation);
    program.setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(FeatureVertexData));

    //设置透明
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
//    glBlendColor(0.0f,0.5f,1.0f,0.3f);
// glDrawElements(GL_TRIANGLES, num_vertex_index, GL_UNSIGNED_INT, 0);
    glDrawArrays(GL_TRIANGLES, 0,num_vertex_data);
}

void PlaneGeometry::GeneratePlaneData(PlaneMsg planeMsgIn)
{
    vertex_data.clear();

    FeatureVertexData temp0;
    temp0.position = planeMsgIn.firstPt*G_SCALE;

    FeatureVertexData temp1;
    temp1.position = planeMsgIn.secondPt*G_SCALE;

    FeatureVertexData temp2;
    temp2.position = planeMsgIn.thirdPt*G_SCALE;

    FeatureVertexData temp3;
    temp3.position = planeMsgIn.fourPt*G_SCALE;

    vertex_data.push_back(temp0);
    vertex_data.push_back(temp1);
    vertex_data.push_back(temp2);
    vertex_data.push_back(temp0);
    vertex_data.push_back(temp2);
    vertex_data.push_back(temp3);

}
void PlaneGeometry::GeneratePlanesData(QVector<PlaneMsg> planeMsgsIn)
{
    vertex_data.clear();
    for(int index=0;index<planeMsgsIn.size();index++)
    {
        FeatureVertexData temp0;
        temp0.position = planeMsgsIn[index].firstPt*G_SCALE;

        FeatureVertexData temp1;
        temp1.position = planeMsgsIn[index].secondPt*G_SCALE;

        FeatureVertexData temp2;
        temp2.position = planeMsgsIn[index].thirdPt*G_SCALE;

        FeatureVertexData temp3;
        temp3.position = planeMsgsIn[index].fourPt*G_SCALE;

        vertex_data.push_back(temp0);
        vertex_data.push_back(temp1);
        vertex_data.push_back(temp2);
        vertex_data.push_back(temp0);
        vertex_data.push_back(temp2);
        vertex_data.push_back(temp3);

    }
}

