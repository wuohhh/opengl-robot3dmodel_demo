#include"basegeometry.h"
#include <QOpenGLContext>

BaseGeometry::BaseGeometry()
    : vertex_index_Buf(QOpenGLBuffer::IndexBuffer)
{
    initializeOpenGLFunctions();
    m_glMajor=QOpenGLContext::currentContext()->format().majorVersion();

    vertex_data_Buf.create();
    vertex_index_Buf.create();
}
BaseGeometry::~BaseGeometry()
{
    vertex_data_Buf.destroy();
    vertex_index_Buf.destroy();
}

bool BaseGeometry::initShaders()
{
    return false;
}

void BaseGeometry::drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
{

}

void BaseGeometry::uploadVertices() {
    num_vertex_data=vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(vertex_data.constData(),num_vertex_data*sizeof(FeatureVertexData));
    vertex_data_Buf.release();
}
