#pragma once
#include <QVector>
#include <QVector3D>
#include <QString>
namespace robot3d { namespace rendering {
struct CpuRobotVertex { QVector3D position, normal, material; };
struct CpuRobotMesh { QVector<CpuRobotVertex> vertices; };
// CPU-only parsing. The Assimp scene never escapes its importer lifetime.
bool loadRobotMesh(const QString& path,CpuRobotMesh& result,QString& error);
}}
