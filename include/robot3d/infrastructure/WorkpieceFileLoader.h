#pragma once
#include "robot3d/domain/Workpiece.h"
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <cstddef>

namespace robot3d { namespace infrastructure {
struct WorkpieceLoadOptions {
    double linearDeflectionMm = 0.1;
    double angularDeflectionDeg = 15.0;
    std::size_t maxFileBytes = 512u * 1024u * 1024u;
    std::size_t maxDependencyBytes = 1024u * 1024u * 1024u;
    std::uint64_t maxTriangles = 5000000;
    std::uint64_t maxVertices = 15000000;
    std::shared_ptr<std::atomic_bool> cancellation;
    // Executed on the loading thread. Percent=-1 means an indeterminate stage.
    std::function<void(const QString &stage, int percent)> progress;
};
// Reads an immutable source/dependency snapshot. No scene or GPU objects escape.
// Result remains untouched on failure; all strings in domain data are UTF-8.
bool loadWorkpieceFile(const QString &path, const WorkpieceLoadOptions &options,
                       std::shared_ptr<domain::WorkpieceAsset> &result, QString &error);
// Reparse only embedded source bytes/dependencies, without accessing original
// files or writing plaintext temporary files. Returns raw geometry; the caller
// reapplies the saved normalization/selection and preserves scene identities.
// Stored tessellation parameters override options; failure never changes result.
bool rebuildWorkpieceAsset(const domain::WorkpieceAsset &stored,const WorkpieceLoadOptions &options,
                           std::shared_ptr<domain::WorkpieceAsset> &result,QString &error);
// Empty means this build cannot rebuild that format (existing caches can display).
QString workpieceAdapterVersion(const QString &format);
QString workpieceImportCapabilities();
} }
