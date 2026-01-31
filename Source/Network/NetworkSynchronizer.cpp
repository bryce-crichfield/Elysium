#include "Network/NetworkSynchronizer.h"

namespace Elysium::Network {

void NetworkSynchronizer::MarkDirty(Entity entity) {
    auto it = entityStates_.find(entity);
    if (it != entityStates_.end()) {
        // Increment version - wraps around at 65535, which is fine
        it->second.version++;
    }
}

bool NetworkSynchronizer::IsDirty(Entity entity) const {
    auto it = entityStates_.find(entity);
    if (it == entityStates_.end()) {
        return false;
    }
    return it->second.version > it->second.lastSentVersion;
}

std::vector<Entity> NetworkSynchronizer::GetDirtyEntities() const {
    std::vector<Entity> dirty;
    dirty.reserve(entityStates_.size() / 4);  // Estimate ~25% dirty

    for (const auto& [entity, state] : entityStates_) {
        if (state.version > state.lastSentVersion) {
            dirty.push_back(entity);
        }
    }
    return dirty;
}

void NetworkSynchronizer::MarkSynced(Entity entity, uint32_t tick, uint32_t componentMask) {
    auto it = entityStates_.find(entity);
    if (it != entityStates_.end()) {
        it->second.lastSentVersion = it->second.version;
        it->second.lastSyncTick = tick;
        it->second.lastSentComponentMask = componentMask;
    }
}

void NetworkSynchronizer::MarkAllSynced(uint32_t tick) {
    for (auto& [entity, state] : entityStates_) {
        state.lastSentVersion = state.version;
        state.lastSyncTick = tick;
        state.lastSentComponentMask = 0;
    }
}

void NetworkSynchronizer::TrackEntity(Entity entity) {
    if (entityStates_.find(entity) == entityStates_.end()) {
        EntityState state;
        state.version = 1;  // Start dirty so it gets synced
        state.lastSentVersion = 0;
        state.lastSyncTick = 0;
        state.lastSentComponentMask = 0;
        entityStates_[entity] = state;
    }
}

void NetworkSynchronizer::UntrackEntity(Entity entity) {
    entityStates_.erase(entity);
}

bool NetworkSynchronizer::IsTracked(Entity entity) const {
    return entityStates_.find(entity) != entityStates_.end();
}

uint16_t NetworkSynchronizer::GetVersion(Entity entity) const {
    auto it = entityStates_.find(entity);
    if (it != entityStates_.end()) {
        return it->second.version;
    }
    return 0;
}

std::vector<Entity> NetworkSynchronizer::GetTrackedEntities() const {
    std::vector<Entity> entities;
    entities.reserve(entityStates_.size());
    for (const auto& [entity, state] : entityStates_) {
        entities.push_back(entity);
    }
    return entities;
}

void NetworkSynchronizer::Clear() {
    entityStates_.clear();
}

bool NetworkSynchronizer::HasComponentMaskChanged(Entity entity, uint32_t currentMask) const {
    auto it = entityStates_.find(entity);
    if (it == entityStates_.end()) {
        return false;
    }
    return it->second.lastSentComponentMask != currentMask;
}

uint32_t NetworkSynchronizer::GetLastSentComponentMask(Entity entity) const {
    auto it = entityStates_.find(entity);
    if (it != entityStates_.end()) {
        return it->second.lastSentComponentMask;
    }
    return 0;
}

}  // namespace Elysium::Network