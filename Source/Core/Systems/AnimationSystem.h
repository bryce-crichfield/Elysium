#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Core/System.h"

namespace Elysium {
struct Animation;
struct Skeleton;
}

namespace Elysium::Systems {

// Plays every AnimationComponent on its entity's skinned model: advances the clip, samples
// it into the skeleton's bones (crossfading from the last clip), and leaves the skin
// matrices on the component for the renderer.
class AnimationSystem : public System {
   public:
    AnimationSystem(Context context);
    void Update(float deltaTime) override;
    // Animates while paused too, so the editor previews clips.
    bool RunsWhenPaused() const override { return true; }

   private:
    const std::vector<int>& TrackBones(const Animation& clip, const Skeleton& skeleton);

    std::map<std::pair<const Animation*, const Skeleton*>, std::vector<int>> trackBones_;
    std::set<std::string> requested_;
};

}  // namespace Elysium::Systems
