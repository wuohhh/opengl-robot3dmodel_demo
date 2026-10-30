#pragma once
#include "robot3d/domain/Workpiece.h"
#include <deque>
namespace robot3d { namespace application {
class WorkpieceService {
public:
    WorkpieceService();
    const domain::WorkpieceSceneState &snapshot() const { return state_; }
    core::Result<std::string> add(domain::WorkpieceAssetPtr asset, const std::string &name,
        const std::string &parentId, const domain::EulerPoseDeg &pose,
        const domain::RobotStateSnapshot &robot);
    core::Result<bool> update(const domain::WorkpieceInstance &, const domain::RobotStateSnapshot &);
    core::Result<bool> remove(const std::string &id);
    bool select(const std::string &id);
    const domain::WorkpieceInstance *find(const std::string &id) const;
    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool dirty() const { return contentVersion_ != savedVersion_; }
    void markSaved();
    // Validation is atomic; restoring a package does not mutate robot state.
    core::Result<bool> replace(const domain::WorkpieceSceneState &, const domain::RobotStateSnapshot &,
                               bool keepWorldPositions = true);
    // Freeze lost parents in world and record an undoable transition; no robot motion.
    bool syncRobotContext(const domain::RobotStateSnapshot &);
private:
    struct History { domain::WorkpieceSceneState state; std::uint64_t version; };
    void remember();
    void changed();
    std::string uniqueName(const std::string &, const std::string &except = std::string()) const;
    domain::WorkpieceSceneState state_;
    std::deque<History> undo_, redo_;
    std::uint64_t nextId_ = 1, nextVersion_ = 1, contentVersion_ = 0, savedVersion_ = 0;
};
} }
