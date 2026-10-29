#pragma once
#include "robot3d/domain/PointCloud.h"
#include <QString>
#include <memory>
namespace robot3d
{
namespace infrastructure
{
/** @brief 点云文件的加载结果；通过点云指针是否为空判断加载成功与否。 */
struct PointCloudLoadResult
{
    std::shared_ptr<const domain::PointCloud> cloud; ///< 成功加载的只读点云；加载失败时为空。
    QString error;                                   ///< 加载失败时供界面显示的错误原因，成功时为空。
    bool ok() const
    {
        return cloud != nullptr;
    }
};
// Supports PCD (ASCII, binary, binary_compressed), PLY (ASCII / both byte orders), XYZ/TXT.
// Invalid coordinates (NaN / Inf) are omitted; malformed files never replace the current cloud.
PointCloudLoadResult loadPointCloudFile(const QString &path);
}
}
