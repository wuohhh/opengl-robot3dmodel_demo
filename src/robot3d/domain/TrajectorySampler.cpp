#include "robot3d/domain/TrajectorySampler.h"
#include <cmath>
namespace robot3d { namespace domain {
bool TrajectorySampler::append(const EulerPoseDeg& p, std::vector<EulerPoseDeg>& points) const {
    if(!capacity_ || !std::isfinite(p.X) || !std::isfinite(p.Y) || !std::isfinite(p.Z)) return false;
    if(!points.empty()) { const auto& q=points.back();
        if(std::fabs(p.X-q.X)<5 && std::fabs(p.Y-q.Y)<5 && std::fabs(p.Z-q.Z)<5) return false;
    }
    if(points.size()>=capacity_) points.erase(points.begin());
    points.push_back(p); return true;
}
}}
