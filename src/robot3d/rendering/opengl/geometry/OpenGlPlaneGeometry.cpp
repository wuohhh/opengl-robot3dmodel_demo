#include"robot3d/rendering/opengl/geometry/OpenGlPlaneGeometry.h"

OpenGlPlaneGeometry::OpenGlPlaneGeometry(OpenGlPlaneData planeMsgIn):OpenGlGeometryBase()
{
    this->planeMsg = planeMsgIn;
   // SetPlaneData(planeMsgIn);
    GeneratePlaneData(planeMsgIn);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(vertex_data.data(), num_vertex_data*sizeof(OpenGlPositionVertex));
    vertex_data_Buf.release();
}

OpenGlPlaneGeometry::OpenGlPlaneGeometry(QVector<OpenGlPlaneData> planeMsgsIn,int numMax):OpenGlGeometryBase()
{
    this->maxNum=numMax;
    this->planeMsgs.clear();
    this->planeMsgs.append(planeMsgsIn);

    if(planeMsgsIn.count()>0)
        this->planeMsg=planeMsgsIn[0];

    GeneratePlanesData(planeMsgsIn);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(nullptr,sizeof(OpenGlPositionVertex)*numVertex*numMax);
    vertex_data_Buf.release();

}

OpenGlPlaneGeometry::~OpenGlPlaneGeometry()
{

}

void OpenGlPlaneGeometry::SetPlaneData(OpenGlPlaneData planeMsgIn)
{
    planeMsg=planeMsgIn; GeneratePlaneData(planeMsgIn); uploadVertices();
}

void OpenGlPlaneGeometry::SetPlaneData(QVector<OpenGlPlaneData> planeMsgsIn)
{
    planeMsgs=planeMsgsIn; if(!planeMsgsIn.isEmpty()) planeMsg=planeMsgsIn[0];
    GeneratePlanesData(planeMsgsIn); uploadVertices();
}
void OpenGlPlaneGeometry::AddDeletePlaneData(QVector<OpenGlPlaneData> planeMsgsIn)
{
    SetPlaneData(planeMsgsIn);
}

bool OpenGlPlaneGeometry::initShaders()
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

void OpenGlPlaneGeometry::drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
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
    program.setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(OpenGlPositionVertex));

    //设置透明
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
//    glBlendColor(0.0f,0.5f,1.0f,0.3f);
// glDrawElements(GL_TRIANGLES, num_vertex_index, GL_UNSIGNED_INT, 0);
    glDrawArrays(GL_TRIANGLES, 0,num_vertex_data);
}

void OpenGlPlaneGeometry::GeneratePlaneData(OpenGlPlaneData planeMsgIn)
{
    vertex_data.clear();

    OpenGlPositionVertex temp0;
    temp0.position = planeMsgIn.firstPt*robot3d::rendering::kMillimetersToModelUnits;

    OpenGlPositionVertex temp1;
    temp1.position = planeMsgIn.secondPt*robot3d::rendering::kMillimetersToModelUnits;

    OpenGlPositionVertex temp2;
    temp2.position = planeMsgIn.thirdPt*robot3d::rendering::kMillimetersToModelUnits;

    OpenGlPositionVertex temp3;
    temp3.position = planeMsgIn.fourPt*robot3d::rendering::kMillimetersToModelUnits;

    vertex_data.push_back(temp0);
    vertex_data.push_back(temp1);
    vertex_data.push_back(temp2);
    vertex_data.push_back(temp0);
    vertex_data.push_back(temp2);
    vertex_data.push_back(temp3);

}
void OpenGlPlaneGeometry::GeneratePlanesData(QVector<OpenGlPlaneData> planeMsgsIn)
{
    vertex_data.clear();
    for(int index=0;index<planeMsgsIn.size();index++)
    {
        OpenGlPositionVertex temp0;
        temp0.position = planeMsgsIn[index].firstPt*robot3d::rendering::kMillimetersToModelUnits;

        OpenGlPositionVertex temp1;
        temp1.position = planeMsgsIn[index].secondPt*robot3d::rendering::kMillimetersToModelUnits;

        OpenGlPositionVertex temp2;
        temp2.position = planeMsgsIn[index].thirdPt*robot3d::rendering::kMillimetersToModelUnits;

        OpenGlPositionVertex temp3;
        temp3.position = planeMsgsIn[index].fourPt*robot3d::rendering::kMillimetersToModelUnits;

        vertex_data.push_back(temp0);
        vertex_data.push_back(temp1);
        vertex_data.push_back(temp2);
        vertex_data.push_back(temp0);
        vertex_data.push_back(temp2);
        vertex_data.push_back(temp3);

    }
}

