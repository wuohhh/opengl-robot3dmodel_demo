#include"basegeometry.h"
int g_openglVersion=-1;
BaseGeometry::BaseGeometry()
    : vertex_index_Buf(QOpenGLBuffer::IndexBuffer)
{
    initializeOpenGLFunctions();

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
