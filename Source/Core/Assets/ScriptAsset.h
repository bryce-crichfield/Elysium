#pragma once

#include "Core/Asset.h"
#include "Core/Script.h"

namespace Elysium {

class ScriptAsset : public AssetBase<ScriptAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override {}

    Script& GetData() { return script_; }

   private:
    Script script_;
};

}  // namespace Elysium
