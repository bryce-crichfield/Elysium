#pragma once

#include "Core/Asset.h"
#include "Core/Character.h"

namespace Elysium {

class CharacterAsset : public IAsset {
public:
    explicit CharacterAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override {
        try {
            character_ = Character::LoadFromXml(path_.GetFullPath());
            loaded_ = true;
        } catch (...) {}
    }

    void Unload() override { character_ = {}; loaded_ = false; }

    AssetType GetType() const override { return AssetType::CHARACTER; }
    const Character& Get() const { return character_; }

    // In-memory mutation without re-loading from disk
    void Set(const Character& c) { character_ = c; loaded_ = true; }

private:
    Character character_;
};

}  // namespace Elysium
