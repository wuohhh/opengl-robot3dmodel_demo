#include "robot3d/rendering/opengl/geometry/OpenGlGeometryBase.h"
#include <QOpenGLContext>

OpenGlGeometryBase::OpenGlGeometryBase() : vertex_index_Buf(QOpenGLBuffer::IndexBuffer)
{
    initializeOpenGLFunctions();
    m_glMajor = QOpenGLContext::currentContext()->format().majorVersion();

    vertex_data_Buf.create();
    vertex_index_Buf.create();
}
OpenGlGeometryBase::~OpenGlGeometryBase()
{
    vertex_data_Buf.destroy();
    vertex_index_Buf.destroy();
}

bool OpenGlGeometryBase::initShaders()
{
    return false;
}

void OpenGlGeometryBase::drawGeometry(QMatrix4x4 model, QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
{
}

void OpenGlGeometryBase::uploadVertices()
{
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(vertex_data.constData(), num_vertex_data * sizeof(OpenGlPositionVertex));
    vertex_data_Buf.release();
}
