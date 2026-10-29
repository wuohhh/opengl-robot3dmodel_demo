#include "robot3d/rendering/RobotMeshLoader.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include <limits>
#include <utility>
namespace robot3d
{
namespace rendering
{
bool loadRobotMesh(const QString &path, CpuRobotMesh &result, QString &error)
{
    Assimp::Importer importer;
    const auto filename = path.toStdString();
    // The packaged Assimp DLL is Release-only. Use its C-string entry point across CRT modes.
    const auto *scene =
        importer.ReadFile(filename.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);
    if (!scene || !scene->HasMeshes())
    {
        error = path + ": " + QString::fromUtf8(importer.GetErrorString());
        return false;
    }
    CpuRobotMesh candidate;
    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        const auto *mesh = scene->mMeshes[m];
        if (!mesh->HasPositions() || !mesh->HasNormals())
        {
            error = "Mesh positions/normals missing: " + path;
            return false;
        }
        aiColor3D color(.7f, .7f, .7f);
        if (mesh->mMaterialIndex < scene->mNumMaterials)
            scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        if (mesh->mNumVertices > unsigned((std::numeric_limits<int>::max)() - candidate.vertices.size()))
        {
            error = "Mesh too large";
            return false;
        }
        for (unsigned i = 0; i < mesh->mNumVertices; ++i)
        {
            const auto &p = mesh->mVertices[i];
            const auto &n = mesh->mNormals[i];
            CpuRobotVertex v = {QVector3D(p.x, p.y, p.z), QVector3D(n.x, n.y, n.z),
                                QVector3D(color.r, color.g, color.b)};
            candidate.vertices.push_back(v);
        }
    }
    if (candidate.vertices.isEmpty())
    {
        error = "Empty robot mesh: " + path;
        return false;
    }
    result = std::move(candidate);
    return true;
}
}
}
