#pragma once
#include <array>
#include <cstddef>
#include <vector>
namespace robot3d { namespace domain {
struct CloudPoint {
    double x=0,y=0,z=0;
    std::array<unsigned char,3> color={{255,255,255}};
};
struct PointCloud {
    std::vector<CloudPoint> points;
    bool hasColors=false;
    std::size_t skippedPoints=0;
};
} }
