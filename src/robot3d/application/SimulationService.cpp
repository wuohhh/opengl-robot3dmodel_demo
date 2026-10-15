#include "robot3d/application/SimulationService.h"
#include <cmath>
#include <stdexcept>
namespace robot3d { namespace application {
SimulationSample SimulationService::step(double dt) {
    if(!std::isfinite(dt)||dt<0) throw std::invalid_argument("Invalid simulation step");
    time_+=dt;
    SimulationSample s;
    s.joints={30*sin(time_*.8),25*sin(time_*.5)-10,30*sin(time_*.65+1)+20,
              45*sin(time_*.9+.3),35*sin(time_*1.1+2),60*sin(time_*.7+1.5)};
    s.tcpSpeed=120+100*sin(time_*2); // Demonstration speed, not a device measurement.
    return s;
}
}}
