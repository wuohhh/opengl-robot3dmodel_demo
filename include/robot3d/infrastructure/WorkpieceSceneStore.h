#pragma once
#include "robot3d/domain/Workpiece.h"
#include <QString>
namespace robot3d { namespace infrastructure {
// Self-contained binary package with SHA-256 integrity and atomic replacement.
bool saveWorkpieceScene(const QString &path, const domain::WorkpieceSceneState &state, QString &error);
bool loadWorkpieceScene(const QString &path, domain::WorkpieceSceneState &state, QString &error);
} }
