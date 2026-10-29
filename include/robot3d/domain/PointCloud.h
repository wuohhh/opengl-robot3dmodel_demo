#pragma once
#include <array>
#include <cstddef>
#include <vector>
namespace robot3d
{
namespace domain
{
/** @brief 点云中的一个三维采样点及其 RGB 颜色，坐标保留源文件单位。 */
struct CloudPoint
{
    double x = 0;                                           ///< X 坐标，保留源文件单位。
    double y = 0;                                           ///< Y 坐标，保留源文件单位。
    double z = 0;                                           ///< Z 坐标，保留源文件单位。
    std::array<unsigned char, 3> color = {{255, 255, 255}}; ///< RGB 三通道颜色，各通道范围为 0～255，默认白色。
};
/** @brief 点云加载后的有效点集合及源文件颜色、无效点统计。 */
struct PointCloud
{
    std::vector<CloudPoint> points; ///< 有效点列表，已排除坐标包含 NaN 或 Inf 的点。
    bool hasColors = false;         ///< 源文件是否提供 RGB 颜色；为 false 时由查看器使用统一颜色。
    std::size_t skippedPoints = 0;  ///< 因坐标非有限数值而跳过的点数。
};
}
}
