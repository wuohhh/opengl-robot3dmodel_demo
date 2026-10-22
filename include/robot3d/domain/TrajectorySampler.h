#pragma once
#include "robot3d/domain/RobotTypes.h"
#include <cstddef>
namespace robot3d { namespace domain {
class TrajectorySampler {
public:
    explicit TrajectorySampler(std::size_t capacity=1000) : capacity_(capacity) {}
    bool append(const EulerPoseDeg& pose, std::vector<EulerPoseDeg>& points) const;
private: std::size_t capacity_;
};
}}
