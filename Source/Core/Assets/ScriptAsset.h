#pragma once

#include "Core/Asset.h"
#include "Core/Script.h"
#include "raylib.h"

namespace Elysium {

class ScriptAsset : public IAsset {
public:
    explicit ScriptAsset(Path path) : IAsset(std::move(path)) {}

    void LoadData() override {
        char* text = ::LoadFileText(path_.c_str());
        if (text) {
            script_ = {std::string(text), path_};
            ::UnloadFileText(text);
            loaded_ = true;
        }
    }

    void Unload() override { script_ = {}; loaded_ = false; }

    AssetType GetType() const override { return AssetType::SCRIPT; }
    const Script& Get() const { return script_; }

private:
    Script script_;
};

}  // namespace Elysium
