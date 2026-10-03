#pragma once

#include "Core/Animation.h"
#include "Core/Asset.h"

namespace Elysium {

// A skeletal animation clip (.anim), played on a skinned model by an AnimationComponent.
class AnimationAsset : public AssetBase<AnimationAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    void Unload() override;

    Animation& GetData() { return animation_; }

   private:
    Animation animation_;
};

}  // namespace Elysium
