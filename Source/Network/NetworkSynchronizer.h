#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "Entity.h"

namespace Elysium::Network {


struct EntityState {
    uint16_t version = 0;                // Incremented on any networked state change
    uint32_t lastSentVersion = 0;        // Version number when last synced to clients
    uint32_t lastSyncTick = 0;           // Server tick when last synced
    uint32_t lastSentComponentMask = 0;  // Component mask at last sync (for add/remove detection)
};

class NetworkSynchronizer {
public:
    NetworkSynchronizer() = default;

    void Clear();
    std::vector<Entity> GetDirtyEntities() const;
    uint32_t GetLastSentComponentMask(Entity entity) const;
    size_t GetTrackedCount() const { return entityStates_.size(); }
    std::vector<Entity> GetTrackedEntities() const;
    uint16_t GetVersion(Entity entity) const;
    bool HasComponentMaskChanged(Entity entity, uint32_t currentMask) const;
    bool IsDirty(Entity entity) const;
    bool IsTracked(Entity entity) const;
    void MarkAllSynced(uint32_t tick);
    void MarkDirty(Entity entity);
    void MarkSynced(Entity entity, uint32_t tick, uint32_t componentMask);
    void TrackEntity(Entity entity);
    void UntrackEntity(Entity entity);

private:
    std::unordered_map<Entity, EntityState> entityStates_;
};

}  // namespace Elysium::Network