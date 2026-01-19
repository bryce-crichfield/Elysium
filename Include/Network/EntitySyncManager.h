#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "../Entity.h"

namespace Elysium::Network {

/**
 * Entity network state tracking
 * Stored per-entity, not per-component, to keep components simple POD types
 */
struct EntitySyncState {
    uint16_t version = 0;             // Incremented on any networked state change
    uint32_t lastSentVersion = 0;     // Version number when last synced to clients
    uint32_t lastSyncTick = 0;        // Server tick when last synced
    uint32_t lastSentComponentMask = 0; // Component mask at last sync (for add/remove detection)
};

/**
 * EntitySyncManager - Tracks which entitites need to sync and when
 *
 * Uses version numbers instead of memcmp to:
 * - Keep components as simple POD types (no extra tracking fields)
 * - Avoid issues with padding, vectors, strings in memcmp
 * - Provide explicit dirty marking (no false positives)
 *
 * Usage:
 * 1. Systems call MarkDirty(entity) when modifying networked state
 * 2. At sync time, GetDirtyEntities() returns entities needing sync
 * 3. After sending sync packet, call MarkSynced() with the component mask
 */
class EntitySyncManager {
public:
    EntitySyncManager() = default;

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
    std::unordered_map<Entity, EntitySyncState> entityStates_;
};

} // namespace Elysium::Network