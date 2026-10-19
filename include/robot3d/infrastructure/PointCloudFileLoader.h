#pragma once
#include "robot3d/domain/PointCloud.h"
#include <QString>
#include <memory>
namespace robot3d { namespace infrastructure {
struct PointCloudLoadResult {
    std::shared_ptr<const domain::PointCloud> cloud;
    QString error;
    bool ok() const { return cloud!=nullptr; }
};
// Supports PCD (ASCII, binary, binary_compressed), PLY (ASCII / both byte orders), XYZ/TXT.
// Invalid coordinates (NaN / Inf) are omitted; malformed files never replace the current cloud.
PointCloudLoadResult loadPointCloudFile(const QString& path);
} }
