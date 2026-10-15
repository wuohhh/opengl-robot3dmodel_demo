#pragma once
#include "robot3d/core/Result.h"
#include "robot3d/domain/RobotTypes.h"
namespace robot3d { namespace ports {
class IRobotConfigRepository {
public:
    virtual ~IRobotConfigRepository() {}
    virtual core::Result<domain::RobotConfigurationPtr> load(const std::string& id) = 0;
    virtual std::vector<std::string> robotTypes() const = 0;
};
}}
