#pragma once
#include <QVector>
#include <QVector3D>
#include <QString>
namespace robot3d
{
namespace rendering
{
/** @brief 从模型文件提取、与 Assimp 场景生命周期解耦的 CPU 顶点数据。 */
struct CpuRobotVertex
{
    QVector3D position; ///< 顶点位置，保留源模型坐标，尚未换算为渲染模型单位。
    QVector3D normal;   ///< 从模型读取或生成的顶点法向量。
    QVector3D material; ///< 所在网格材质的漫反射 RGB 分量。
};
/** @brief 合并模型各子网格后供渲染后端使用的 CPU 顶点集合。 */
struct CpuRobotMesh
{
    QVector<CpuRobotVertex> vertices; ///< 按导入顺序保存的全部子网格顶点，不持有 Assimp 场景指针。
};
// CPU-only parsing. The Assimp scene never escapes its importer lifetime.
bool loadRobotMesh(const QString &path, CpuRobotMesh &result, QString &error);
}
}
