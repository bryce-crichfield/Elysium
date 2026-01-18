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
struct EntityNetworkState {
    uint16_t version = 0;        // Incremented on any networked state change
    uint32_t lastSentVersion = 0; // Version number when last synced to clients
    uint32_t lastSyncTick = 0;   // Server tick when last synced

    // Cached position for automatic dirty detection
    float lastPosX = 0.0f;
    float lastPosY = 0.0f;
};

/**
 * DirtyTracker - Tracks which entities have changed since last sync
 *
 * Uses version numbers instead of memcmp to:
 * - Keep components as simple POD types (no extra tracking fields)
 * - Avoid issues with padding, vectors, strings in memcmp
 * - Provide explicit dirty marking (no false positives)
 *
 * Usage:
 * 1. Systems call MarkDirty(entity) when modifying networked state
 * 2. At sync time, GetDirtyEntities() returns entities needing sync
 * 3. After sending sync packet, call ClearDirty(entity) or MarkSynced()
 */
class DirtyTracker {
public:
    DirtyTracker() = default;

    /**
     * Mark an entity as dirty (modified since last sync)
     * Call this whenever a networked component changes
     */
    void MarkDirty(Entity entity);

    /**
     * Check if an entity is dirty (needs sync)
     */
    bool IsDirty(Entity entity) const;

    /**
     * Get all entities that are dirty since their last sync
     * Returns entities where version > lastSentVersion
     */
    std::vector<Entity> GetDirtyEntities() const;

    /**
     * Mark entity as synced at the given tick
     * Call after successfully sending entity state to clients
     */
    void MarkSynced(Entity entity, uint32_t tick);

    /**
     * Mark all tracked entities as synced
     * Useful after a full-state sync (handshake)
     */
    void MarkAllSynced(uint32_t tick);

    /**
     * Register an entity for tracking
     * Called when entity is created or enters networked scope
     */
    void TrackEntity(Entity entity);

    /**
     * Stop tracking an entity
     * Called when entity is destroyed
     */
    void UntrackEntity(Entity entity);

    /**
     * Check if an entity is being tracked
     */
    bool IsTracked(Entity entity) const;

    /**
     * Get the current version number for an entity
     */
    uint16_t GetVersion(Entity entity) const;

    /**
     * Get all tracked entities
     */
    std::vector<Entity> GetTrackedEntities() const;

    /**
     * Clear all tracking data
     */
    void Clear();

    /**
     * Get the number of tracked entities
     */
    size_t GetTrackedCount() const { return entityStates_.size(); }

    /**
     * Check if position changed and mark dirty if so
     * Returns true if entity was marked dirty
     */
    bool CheckAndUpdatePosition(Entity entity, float x, float y);

    /**
     * Update stored position without marking dirty
     * Call after syncing to store the "last sent" position
     */
    void UpdateStoredPosition(Entity entity, float x, float y);

private:
    std::unordered_map<Entity, EntityNetworkState> entityStates_;
};

} // namespace Elysium::Network
