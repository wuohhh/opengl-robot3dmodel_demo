#include "substancegeometry.h"

SubstanceGeometry::SubstanceGeometry(SubstanceMsg substanceMsgIn):BaseGeometry()
{
    this->substanceMsg = substanceMsgIn;

    GenerateSubstanceData(this->substanceMsg);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(vertex_data.data(), num_vertex_data*sizeof(FeatureVertexData));
    vertex_data_Buf.release();

}

SubstanceGeometry::SubstanceGeometry(QVector<SubstanceMsg> substanceMsgsIn,int maxNum):BaseGeometry()
{
    if(substanceMsgsIn.count()>0)
    {
        this->substanceMsg=substanceMsgsIn[0];
    }
    this->maxNum=maxNum;
    GenerateSubstancesData(substanceMsgsIn);
    num_vertex_data = vertex_data.size();
    vertex_data_Buf.bind();
    vertex_data_Buf.allocate(nullptr,sizeof(FeatureVertexData)*numVertex*maxNum);
    vertex_data_Buf.release();
}

SubstanceGeometry::~SubstanceGeometry()
{

}

void SubstanceGeometry::SetSubstanceData(QVector<SubstanceMsg> substanceMsgsIn)
{
    if(substanceMsgsIn.count()>0)
     {
         this->substanceMsg=substanceMsgsIn[0];
     }
    GenerateSubstancesData(substanceMsgsIn);
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

void SubstanceGeometry::SetSubstanceData(SubstanceMsg substanceMsgIn)
{
    GenerateSubstanceData(substanceMsgIn);
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

void SubstanceGeometry::AddDeleteSubstanceData(QVector<SubstanceMsg> substanceMsgsIn)
{
    if(substanceMsgsIn.count()>0)
    {
        this->substanceMsg=substanceMsgsIn[0];
    }
    GenerateSubstancesData(substanceMsgsIn);
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

bool SubstanceGeometry::initShaders()
{
    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/substance_vertex_shader.glsl"))
        return false;

    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/substance_fragment_shader.glsl"))
        return false;

    if (!program.link())
        return false;

    if (!program.bind())
        return false;

    return  true;
}

void SubstanceGeometry::drawGeometry(QMatrix4x4 model,QMatrix4x4 mvpMatrix, QMatrix4x4 lightMatrix)
{
    program.bind();
    program.setUniformValue("lightColor", QVector3D(1.0,1.0,1.0));
    program.setUniformValue("toyColor", QVector3D(this->substanceMsg.color.red()/255.0, this->substanceMsg.color.green()/255.0, this->substanceMsg.color.blue()/255.0));
    program.setUniformValue("mvp_matrix", mvpMatrix);

    vertex_data_Buf.bind();

    quintptr offset = 0;
    int vertexLocation = program.attributeLocation("a_position");
    program.enableAttributeArray(vertexLocation);
    program.setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(FeatureVertexData));


    //设置透明
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
   glDrawArrays(GL_TRIANGLES, 0,num_vertex_data);
}

void SubstanceGeometry::GenerateSubstanceData(SubstanceMsg substanceMsgIn)
{
    vertex_data.clear();      //顶点数据需要清空，插入新的顶点数据
    switch (substanceMsgIn.substanceType)
    {
    case 0://球
        GenerateSphereData(substanceMsgIn.centerPose, substanceMsgIn.radius);
        break;
    case 1://立方体
        GenerateCubeData(substanceMsgIn.centerPose, substanceMsgIn.length, substanceMsgIn.width, substanceMsgIn.height);
        break;
    case 2://圆柱
        GenerateCylinderData(substanceMsgIn.centerPose, substanceMsgIn.radius, substanceMsgIn.height);
        break;
    case 3://圆锥
        GenerateConeData(substanceMsgIn.centerPose, substanceMsgIn.radius, substanceMsgIn.height);
        break;
    default:
        break;
    }
}
void SubstanceGeometry::GenerateSubstancesData(QVector<SubstanceMsg> substanceMsgsIn)
{
    vertex_data.clear();      //顶点数据需要清空，插入新的顶点数据
    for(int index=0; (index<substanceMsgsIn.size()) && (index<this->maxNum); index++)
    {
        SubstanceMsg substanceMsgIn = substanceMsgsIn[index];
        switch (substanceMsgIn.substanceType)
        {
        case 0://球
            GenerateSphereData(substanceMsgIn.centerPose, substanceMsgIn.radius);
            break;
        case 1://立方体
            GenerateCubeData(substanceMsgIn.centerPose, substanceMsgIn.length, substanceMsgIn.width, substanceMsgIn.height);
            break;
        case 2://圆柱
            GenerateCylinderData(substanceMsgIn.centerPose, substanceMsgIn.radius, substanceMsgIn.height);
            break;
        case 3://圆锥
            GenerateConeData(substanceMsgIn.centerPose, substanceMsgIn.radius, substanceMsgIn.height);
            break;
        default:
            break;
        }
    }
}

void SubstanceGeometry::GenerateSphereData(double* centerPose,float radius)
{
    QVector3D center = QVector3D(centerPose[0],centerPose[1],centerPose[2]);
    int angleSpan = 10;   // 将球进行单位切分的角度
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

void SubstanceGeometry::GenerateConeData(double* centerPose,float radius,float height)
{
    QVector3D center = QVector3D(centerPose[0],centerPose[1],centerPose[2]);
    QMatrix4x4 RotateMatrix = this->AxisAngle2RotateMatrix(centerPose);
    bool drawBase = true;

    int angleSpan = 10;  // 将圆锥进行单位切分的角度
    QVector3D apex = center + QVector3D(0, -height, 0);  // 圆锥的顶点

    // 循环生成底部圆周的顶点和三角形面
    for (int hAngle = 0; hAngle < 360; hAngle += angleSpan) // 水平方向切分
    {
        // 计算当前角度下的底部圆周的顶点
        float x0 = center.x() + radius * static_cast<float>(qCos(M_PI / 180 * hAngle));
        float y0 = center.y();
        float z0 = center.z() + radius * static_cast<float>(qSin(M_PI / 180 * hAngle));

        // 计算下一个角度的底部圆周顶点
        float x1 = center.x() + radius * static_cast<float>(qCos(M_PI / 180 * (hAngle + angleSpan)));
        float y1 = center.y();
        float z1 = center.z() + radius * static_cast<float>(qSin(M_PI / 180 * (hAngle + angleSpan)));

        // 顶点数据结构体
        FeatureVertexData temp0;
        temp0.position = QVector3D(x0, y0, z0) * G_SCALE;  // 当前底部圆周点

        FeatureVertexData temp1;
        temp1.position = QVector3D(x1, y1, z1) * G_SCALE;  // 下一个底部圆周点

        FeatureVertexData temp2;
        temp2.position = apex * G_SCALE;  // 圆锥的顶点

        // 将数据加入到顶点数组中，形成三角形
        vertex_data.push_back(GetRotatePoint(temp0, RotateMatrix, center)); // 当前底部圆周点
        vertex_data.push_back(GetRotatePoint(temp1, RotateMatrix, center)); // 下一个底部圆周点
        vertex_data.push_back(GetRotatePoint(temp2, RotateMatrix, center)); // 圆锥的顶点
    }

    // 绘制底部圆
    if (drawBase)
    {
        // 循环绘制底部圆
        for (int hAngle = 0; hAngle < 360; hAngle += angleSpan)
        {
            // 计算当前角度下的底部圆周的顶点
            float x0 = center.x() + radius * static_cast<float>(qCos(M_PI / 180 * hAngle));
            float y0 = center.y();
            float z0 = center.z() + radius * static_cast<float>(qSin(M_PI / 180 * hAngle));

            // 计算下一个角度的底部圆周顶点
            float x1 = center.x() + radius * static_cast<float>(qCos(M_PI / 180 * (hAngle + angleSpan)));
            float y1 = center.y();
            float z1 = center.z() + radius * static_cast<float>(qSin(M_PI / 180 * (hAngle + angleSpan)));

            // 底部圆的圆心
            FeatureVertexData centerVertex;
            centerVertex.position = center * G_SCALE;

            FeatureVertexData temp0;
            temp0.position = QVector3D(x0, y0, z0) * G_SCALE;  // 当前圆周点

            FeatureVertexData temp1;
            temp1.position = QVector3D(x1, y1, z1) * G_SCALE;  // 下一个圆周点

            // 将数据加入到顶点数组中，形成底部圆的三角形
            vertex_data.push_back(GetRotatePoint(centerVertex, RotateMatrix, center));  // 圆心
            vertex_data.push_back(GetRotatePoint(temp0, RotateMatrix, center));         // 当前圆周点
            vertex_data.push_back(GetRotatePoint(temp1, RotateMatrix, center));         // 下一个圆周点
        }
    }
}

void SubstanceGeometry::GenerateCylinderData(double* centerPose,float radius,float height)
{
    QVector3D center = QVector3D(centerPose[0],centerPose[1],centerPose[2]);
    QMatrix4x4 RotateMatrix = this->AxisAngle2RotateMatrix(centerPose);
    int angleSpan = 10;   // 圆柱体的切分角度，越小细节越多

    // 生成底面和顶面的顶点
    QVector<FeatureVertexData> bottomVertices;
    QVector<FeatureVertexData> topVertices;
    for (int angle = 0; angle < 360; angle += angleSpan)
    {
        float radian = M_PI / 180 * angle;
        float x = center.x() + radius * qCos(radian);
        float y = center.y();
        float z = center.z() + radius * qSin(radian);

        FeatureVertexData bottomVertex;
        bottomVertex.position = QVector3D(x, y, z) * G_SCALE;
        bottomVertices.push_back(bottomVertex);

        y = center.y() - height ;
        FeatureVertexData topVertex;
        topVertex.position = QVector3D(x, y, z) * G_SCALE;
        topVertices.push_back(topVertex);
    }

    // 生成侧面顶点数据
    for (int i = 0; i < bottomVertices.size(); ++i)
    {
        // 获取底面和顶面的顶点
        FeatureVertexData bottomVertex = bottomVertices[i];
        FeatureVertexData topVertex = topVertices[i];

        // 获取下一个顶点（圆形闭环连接）
        int nextIndex = (i + 1) % bottomVertices.size();
        FeatureVertexData bottomNextVertex = bottomVertices[nextIndex];
        FeatureVertexData topNextVertex = topVertices[nextIndex];

        // 侧面三角形 由两个三角形组成
        vertex_data.push_back(GetRotatePoint(bottomVertex, RotateMatrix, center));
        vertex_data.push_back(GetRotatePoint(topVertex, RotateMatrix, center));
        vertex_data.push_back(GetRotatePoint(bottomNextVertex, RotateMatrix, center));

        vertex_data.push_back(GetRotatePoint(bottomNextVertex, RotateMatrix, center));
        vertex_data.push_back(GetRotatePoint(topVertex, RotateMatrix, center));
        vertex_data.push_back(GetRotatePoint(topNextVertex, RotateMatrix, center));
    }

    // 生成底面顶点三角形（中心点 + 每个底面上的点）
    FeatureVertexData centerBottomVertex;
    centerBottomVertex.position = QVector3D(center.x(), center.y(), center.z()) * G_SCALE;

    for (int i = 0; i < bottomVertices.size(); ++i)
    {
        vertex_data.push_back(GetRotatePoint(centerBottomVertex, RotateMatrix, center));
        vertex_data.push_back(GetRotatePoint(bottomVertices[i], RotateMatrix, center));
        vertex_data.push_back(GetRotatePoint(bottomVertices[(i + 1) % bottomVertices.size()], RotateMatrix, center));
    }

    // 生成顶面顶点三角形（中心点 + 每个顶面上的点）
    FeatureVertexData centerTopVertex;
    centerTopVertex.position = QVector3D(center.x(), center.y(), center.z()) * G_SCALE;

    for (int i = 0; i < topVertices.size(); ++i)
    {
        vertex_data.push_back(GetRotatePoint(centerTopVertex, RotateMatrix, center));
        vertex_data.push_back(GetRotatePoint(topVertices[(i + 1) % topVertices.size()], RotateMatrix, center));
        vertex_data.push_back(GetRotatePoint(topVertices[i], RotateMatrix, center));
    }
}

void SubstanceGeometry::GenerateCubeData(double* centerPose,float length, float width, float height)
{
    QVector3D center = QVector3D(centerPose[0],centerPose[1],centerPose[2]);
    QMatrix4x4 RotateMatrix = this->AxisAngle2RotateMatrix(centerPose);

    // 立方体的八个顶点
    QVector3D cubeVertices[8] = {
        QVector3D(center.x() + length / 2, center.y(), center.z() + width / 2),
        QVector3D(center.x() + length / 2, center.y(), center.z() - width / 2),
        QVector3D(center.x() - length / 2, center.y(), center.z() - width / 2),
        QVector3D(center.x() - length / 2, center.y(), center.z() + width / 2),
        QVector3D(center.x() + length / 2, center.y() - height, center.z() + width / 2),
        QVector3D(center.x() + length / 2, center.y() - height, center.z() - width / 2),
        QVector3D(center.x() - length / 2, center.y() - height, center.z() - width / 2),
        QVector3D(center.x() - length / 2, center.y() - height, center.z() + width / 2),
    };

    // 每个面由两个三角形构成，定义六个面
    int indices[6*6] = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        0, 3, 7, 0, 7, 4,
        1, 5, 6, 1, 6, 2,
        2, 3, 7, 2, 7, 6,
        0, 1, 5, 0, 5, 4
    };

    // 为每个面生成顶点数据
    for (int i = 0; i < 6*6; i += 3)
    {
        FeatureVertexData vertex1;
        vertex1.position = cubeVertices[indices[i]] * G_SCALE;
        vertex_data.push_back(GetRotatePoint(vertex1, RotateMatrix, center));

        FeatureVertexData vertex2;
        vertex2.position = cubeVertices[indices[i + 1]] * G_SCALE;
        vertex_data.push_back(GetRotatePoint(vertex2, RotateMatrix, center));

        FeatureVertexData vertex3;
        vertex3.position = cubeVertices[indices[i + 2]] * G_SCALE;
        vertex_data.push_back(GetRotatePoint(vertex3, RotateMatrix, center));
    }
}

QMatrix4x4 SubstanceGeometry::AxisAngle2RotateMatrix(double* pose)
{
    double theta;
    double r[3];
    pose[3]=pose[3]*M_PI/180;
    pose[4]=pose[4]*M_PI/180;
    pose[5]=pose[5]*M_PI/180;
    QMatrix4x4 RotZYX;

    theta = sqrt((pose[3] * pose[3] + pose[4] *  pose[4] +  pose[5] * pose[5]))   /*pi/180.0*/;
    if (fabs(theta) < 1e-10)
    {
        RotZYX.setToIdentity();
    }
    else
    {
        r[0] = pose[3] / theta;
        r[1] = pose[4] / theta;
        r[2] = pose[5] / theta;
        RotZYX.setRow(0,QVector4D(r[0]*r[0]*(1-cos(theta))+cos(theta),r[0]*r[1]*(1-cos(theta))-r[2]*sin(theta),r[0]*r[2]*(1-cos(theta))+r[1]*sin(theta),1.0));
        RotZYX.setRow(1,QVector4D(r[0]*r[1]*(1-cos(theta))+r[2]*sin(theta),r[1]*r[1]*(1-cos(theta))+cos(theta),r[1]*r[2]*(1-cos(theta))-r[0]*sin(theta),1.0));
        RotZYX.setRow(2,QVector4D(r[0]*r[2]*(1-cos(theta))-r[1]*sin(theta),r[1]*r[2]*(1-cos(theta))+r[0]*sin(theta),r[2]*r[2]*(1-cos(theta))+cos(theta),1.0));
    }
    RotZYX.setColumn(3,QVector4D(pose[0],pose[1],pose[2],1.0/G_SCALE)*G_SCALE);
    return RotZYX;
}

FeatureVertexData SubstanceGeometry::GetRotatePoint(FeatureVertexData pointData, QMatrix4x4 RotateMatrix, QVector3D center)
{
    FeatureVertexData result;
    QVector3D tmpPoint = pointData.position/G_SCALE;
    tmpPoint = (QVector3D(tmpPoint.x()-center.x(),tmpPoint.z()-center.z(),center.y()-tmpPoint.y()));
    QVector4D direction(tmpPoint.x(),tmpPoint.y(),tmpPoint.z(),0);
    result.position = (QVector3D(RotateMatrix*direction)+center)*G_SCALE;
    return result;
}
