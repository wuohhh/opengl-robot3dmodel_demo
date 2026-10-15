#pragma once
#include "robot3d/ports/IRobotConfigRepository.h"
namespace robot3d { namespace infrastructure {
class XmlRobotConfigRepository : public ports::IRobotConfigRepository {
public:
    explicit XmlRobotConfigRepository(const std::string& root);
    core::Result<domain::RobotConfigurationPtr> load(const std::string& id) override;
    std::vector<std::string> robotTypes() const override;
private: std::string root_;
};
}}
