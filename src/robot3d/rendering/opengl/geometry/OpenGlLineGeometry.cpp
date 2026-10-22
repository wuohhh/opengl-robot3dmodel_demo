#include"robot3d/rendering/opengl/geometry/OpenGlLineGeometry.h"

OpenGlLineGeometry::OpenGlLineGeometry(OpenGlLineData lineMsg):OpenGlGeometryBase()
{

    this->lineMsg = lineMsg;
    GenerateLineData(lineMsg.startPt, lineMsg.endPt);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(vertex_data.data(), num_vertex_data * sizeof(OpenGlPositionVertex));
    vertex_data_Buf.release();
}

OpenGlLineGeometry::OpenGlLineGeometry(QVector<QVector3D> points, QColor color,int maxNum):OpenGlGeometryBase()
{
     this->maxNum=maxNum;
    this->pointList.append(points);
     this->lineMsg.color=color;
     GenerateLinesData(points);
     num_vertex_data = vertex_data.size();
     vertex_data_Buf.bind();
     vertex_data_Buf.allocate(vertex_data.data(), maxNum * sizeof(OpenGlPositionVertex));
     vertex_data_Buf.release();
}

OpenGlLineGeometry::~OpenGlLineGeometry()
{

}

void OpenGlLineGeometry::SetLineData(QVector3D startPt, QVector3D endPt)
{
    lineMsg.startPt=startPt; lineMsg.endPt=endPt;
    GenerateLineData(startPt,endPt); uploadVertices();
}

void OpenGlLineGeometry::SetLineData(QVector<QVector3D> points)         //绘制线的顶点数不再增删，只修改线段组成的顶点
{
    pointList=points; GenerateLinesData(points); uploadVertices();
}

void OpenGlLineGeometry::AddDeleteLineData(QVector<QVector3D> points)
{
    pointList=points; GenerateLinesData(points); uploadVertices();
}

bool OpenGlLineGeometry::initShaders()
{
    // Compile vertex shader
    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/line_vertex_shader.glsl"))
        return false;

    // Compile fragment shader
    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/line_fragment_shader.glsl"))
        return false;

    // Link shader pipeline
    if (!program.link())
        return false;

    // Bind shader pipeline for use
    if (!program.bind())
        return false;

    return  true;
}



void OpenGlLineGeometry::drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
{
    program.bind();
    program.setUniformValue("lightColor", QVector3D(1.0,1.0,1.0));
    program.setUniformValue("toyColor", QVector3D(this->lineMsg.color.red()/255.0, this->lineMsg.color.green()/255.0, this->lineMsg.color.blue()/255.0));
    program.setUniformValue("mvp_matrix", mvpMatrix);

    vertex_data_Buf.bind();
    //vertex_index_Buf.bind();

    quintptr offset = 0;
    int vertexLocation = program.attributeLocation("a_position");
    program.enableAttributeArray(vertexLocation);
    program.setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(OpenGlPositionVertex));

    glLineWidth(3);
    //需要改成绘制直线GL_Lines GL_TRIANGLE_STRIP GL_LINES
    //glDrawElements(GL_LINE_STRIP, num_vertex_index, GL_UNSIGNED_INT, 0);
    glDrawArrays(GL_LINE_STRIP, 0,num_vertex_data);

}
void OpenGlLineGeometry::GenerateLineData(QVector3D pointStart,QVector3D pointEnd)
{
    vertex_data.clear();      //顶点数据需要清空，插入新的顶点数据
    OpenGlPositionVertex temp0;
    temp0.position = pointStart*robot3d::rendering::kMillimetersToModelUnits;
    OpenGlPositionVertex temp1;
    temp1.position = pointEnd*robot3d::rendering::kMillimetersToModelUnits;
    vertex_data.push_back(temp0);
    vertex_data.push_back(temp1);

}
void OpenGlLineGeometry::GenerateLinesData(QVector<QVector3D> points)
{
    vertex_data.clear();      //顶点数据需要清空，插入新的顶点数据
    for(int index=0;index<points.size();index++)
    {
        OpenGlPositionVertex temp0;
        temp0.position = points[index]*robot3d::rendering::kMillimetersToModelUnits;
        vertex_data.push_back(temp0);
    }
}
