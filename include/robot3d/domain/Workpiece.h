#pragma once
#include "robot3d/domain/RobotTypes.h"
#include "robot3d/core/Result.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace robot3d { namespace domain {
struct Vec3d {
    double x = 0, y = 0, z = 0;
    Vec3d() {}
    Vec3d(double a, double b, double c) : x(a), y(b), z(c) {}
};
struct Matrix4d {
    // Row-major matrix, column vectors, translation in [3], [7], [11].
    std::array<double, 16> values;
    Matrix4d() : values{{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}} {}
};
struct WorkpieceVertex {
    Vec3d position;
    Vec3d normal;
    std::array<float, 3> color = {{0.7f, 0.7f, 0.7f}};
};
struct WorkpieceMesh {
    std::vector<WorkpieceVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> triangleFaceIds;
};
struct WorkpieceComponent {
    std::string id, name, parentId;
    // Cumulative source-node-to-asset transform, NOT relative to parentId.
    Matrix4d transform;
    std::vector<WorkpieceMesh> meshes;
    bool selected = true;
};
struct WorkpieceSourceFile {
    std::string relativePath;
    std::vector<std::uint8_t> bytes;
    std::string sha256;
};
struct WorkpieceAsset {
    std::string id, name, sourcePath, format, adapterVersion;
    std::string sourceUnit;
    std::vector<std::uint8_t> sourceBytes;
    std::vector<WorkpieceSourceFile> dependencies;
    std::vector<WorkpieceComponent> components;
    std::vector<std::string> warnings;
    double sourceUnitToMm = 1.0;
    bool unitKnown = false;
    bool normalized = false;
    double linearDeflectionMm = 0.1;
    double angularDeflectionDeg = 15.0;
    // Source-to-local normalization metadata. Normalized geometry is baked in mm.
    Matrix4d normalization;
    Vec3d boundsMin, boundsMax;
};
typedef std::shared_ptr<const WorkpieceAsset> WorkpieceAssetPtr;
enum class WorkpieceDisplayMode { Solid = 0, Wireframe = 1, SolidEdges = 2 };
struct WorkpieceInstance {
    std::string id, name, parentCoordinateId = "b_0";
    WorkpieceAssetPtr asset;
    EulerPoseDeg pose = {};
    Matrix4d fallbackWorld;
    bool visible = true, locked = false, colorOverride = false, showAxes = false;
    std::array<float, 3> color = {{0.65f, 0.7f, 0.8f}};
    double opacity = 1.0;
    WorkpieceDisplayMode displayMode = WorkpieceDisplayMode::SolidEdges;
    std::string warning;
};
struct WorkpieceSceneState {
    std::vector<WorkpieceInstance> instances;
    std::string selectedId;
    std::string robotId;
    EulerPoseDeg baseInWorld = {};
    std::uint64_t revision = 0;
};
enum class WorkpieceOrigin { Source = 0, Center = 1, BottomCenter = 2 };
struct WorkpieceNormalizeOptions {
    // 0 means use trusted asset unit metadata; unknown units require explicit input.
    double unitToMm = 0;
    // Signed source axes: +/-1=X, +/-2=Y, +/-3=Z. Target is right-handed Z-up/X-forward.
    int upAxis = 3, forwardAxis = 1;
    bool leftHanded = false;
    WorkpieceOrigin origin = WorkpieceOrigin::Source;
    bool selectAllComponents = true;
    std::vector<std::string> selectedComponentIds;
    std::shared_ptr<std::atomic_bool> cancellation;
};
Matrix4d multiply(const Matrix4d &, const Matrix4d &);
Matrix4d rigidInverse(const Matrix4d &);
Matrix4d poseMatrix(const EulerPoseDeg &);
EulerPoseDeg matrixPose(const Matrix4d &);
Vec3d transformPoint(const Matrix4d &, const Vec3d &);
Vec3d transformDirection(const Matrix4d &, const Vec3d &);
bool finiteMatrix(const Matrix4d &);
bool parentWorldMatrix(const std::string &, const RobotStateSnapshot &, Matrix4d &);
Matrix4d workpieceWorldMatrix(const WorkpieceInstance &, const RobotStateSnapshot &);
core::Result<WorkpieceAssetPtr> normalizeWorkpieceAsset(const WorkpieceAsset &, const WorkpieceNormalizeOptions &);
core::Result<bool> validateWorkpieceAsset(const WorkpieceAsset &, bool requireNormalized = true);
bool sameWorkpieceAssetContent(const WorkpieceAsset &, const WorkpieceAsset &);
std::uint64_t workpieceTriangleCount(const WorkpieceAsset &);
} }
