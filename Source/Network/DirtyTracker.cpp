#include "../../Include/Network/DirtyTracker.h"

namespace Elysium::Network {

void DirtyTracker::MarkDirty(Entity entity) {
    auto it = entityStates_.find(entity);
    if (it != entityStates_.end()) {
        // Increment version - wraps around at 65535, which is fine
        it->second.version++;
    }
}

bool DirtyTracker::IsDirty(Entity entity) const {
    auto it = entityStates_.find(entity);
    if (it == entityStates_.end()) {
        return false;
    }
    return it->second.version > it->second.lastSentVersion;
}

std::vector<Entity> DirtyTracker::GetDirtyEntities() const {
    std::vector<Entity> dirty;
    dirty.reserve(entityStates_.size() / 4);  // Estimate ~25% dirty

    for (const auto& [entity, state] : entityStates_) {
        if (state.version > state.lastSentVersion) {
            dirty.push_back(entity);
        }
    }
    return dirty;
}

void DirtyTracker::MarkSynced(Entity entity, uint32_t tick) {
    auto it = entityStates_.find(entity);
    if (it != entityStates_.end()) {
        it->second.lastSentVersion = it->second.version;
        it->second.lastSyncTick = tick;
    }
}

void DirtyTracker::MarkAllSynced(uint32_t tick) {
    for (auto& [entity, state] : entityStates_) {
        state.lastSentVersion = state.version;
        state.lastSyncTick = tick;
    }
}

void DirtyTracker::TrackEntity(Entity entity) {
    if (entityStates_.find(entity) == entityStates_.end()) {
        EntityNetworkState state;
        state.version = 1;        // Start dirty so it gets synced
        state.lastSentVersion = 0;
        state.lastSyncTick = 0;
        entityStates_[entity] = state;
    }
}

void DirtyTracker::UntrackEntity(Entity entity) {
    entityStates_.erase(entity);
}

bool DirtyTracker::IsTracked(Entity entity) const {
    return entityStates_.find(entity) != entityStates_.end();
}

uint16_t DirtyTracker::GetVersion(Entity entity) const {
    auto it = entityStates_.find(entity);
    if (it != entityStates_.end()) {
        return it->second.version;
    }
    return 0;
}

std::vector<Entity> DirtyTracker::GetTrackedEntities() const {
    std::vector<Entity> entities;
    entities.reserve(entityStates_.size());
    for (const auto& [entity, state] : entityStates_) {
        entities.push_back(entity);
    }
    return entities;
}

void DirtyTracker::Clear() {
    entityStates_.clear();
}

bool DirtyTracker::CheckAndUpdatePosition(Entity entity, float x, float y) {
    auto it = entityStates_.find(entity);
    if (it == entityStates_.end()) {
        return false;
    }

    // Check if position changed (using small epsilon for float comparison)
    constexpr float EPSILON = 0.001f;
    float dx = x - it->second.lastPosX;
    float dy = y - it->second.lastPosY;

    if (dx * dx + dy * dy > EPSILON * EPSILON) {
        // Position changed - mark dirty and update cached position
        it->second.version++;
        it->second.lastPosX = x;
        it->second.lastPosY = y;
        return true;
    }

    return false;
}

void DirtyTracker::UpdateStoredPosition(Entity entity, float x, float y) {
    auto it = entityStates_.find(entity);
    if (it != entityStates_.end()) {
        it->second.lastPosX = x;
        it->second.lastPosY = y;
    }
}

} // namespace Elysium::Network
