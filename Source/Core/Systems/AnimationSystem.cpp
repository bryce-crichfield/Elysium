#include "Core/Systems/AnimationSystem.h"

#include <algorithm>
#include <cmath>

#include "Core/Animation.h"
#include "Core/Components/AnimationComponent.h"
#include "Core/Components/ModelComponent.h"
#include "Core/Graphics.h"
#include "Core/Path.h"
#include "Core/SystemRegistry.h"
#include "Interfaces/IAssetService.h"

namespace Elysium::Systems {

AnimationSystem::AnimationSystem(Context context) : System(context) {}

const std::vector<int>& AnimationSystem::TrackBones(const Animation& clip, const Skeleton& skeleton) {
    auto [it, fresh] = trackBones_.try_emplace({&clip, &skeleton});
    // Rebound if the clip was reloaded in place.
    if (fresh || it->second.size() != clip.tracks.size()) it->second = Skinning::BindTracks(clip, skeleton);
    return it->second;
}

void AnimationSystem::Update(float deltaTime) {
    auto& assets = services->Get<Services::IAssetService>();
    world->Query<ModelComponent, AnimationComponent>([&](Entity, const ModelComponent& model, AnimationComponent& anim) {
        const Model* loaded = model.modelPath.empty() ? nullptr : assets.Get<Model>(Path(model.modelPath));
        if (!loaded || !loaded->skeleton) {
            anim.skin.clear();
            return;
        }
        const Skeleton& skeleton = *loaded->skeleton;
        const size_t boneCount = skeleton.bones.size();
        if (anim.pose.size() != boneCount) {
            anim.pose.clear();
            for (const Bone& bone : skeleton.bones) anim.pose.push_back(bone.bind);
            anim.fade = 0.0f;
        }

        // A new clip: start it, fading out of the pose the old one left.
        if (anim.clip != anim.current) {
            if (!anim.current.empty() && anim.blend > 0.0f && !anim.skin.empty()) {
                anim.fadeFrom = anim.pose;
                anim.fade = anim.fadeTotal = anim.blend;
            }
            anim.current = anim.clip;
            anim.time = 0.0f;
            anim.duration = 0.0f;  // until the new clip is loaded
            anim.finished = false;
        }

        const Animation* clip = nullptr;
        if (!anim.clip.empty()) {
            clip = assets.Get<Animation>(Path(anim.clip));
            if (!clip && requested_.insert(anim.clip).second) assets.LoadAsset<Animation>(Path(anim.clip));
        }

        for (size_t i = 0; i < boneCount; ++i) anim.pose[i] = skeleton.bones[i].bind;
        if (clip && clip->duration > 0.0f) {
            anim.duration = clip->duration;
            if (anim.playing) anim.time += deltaTime * anim.speed;
            if (anim.loop) {
                anim.time = std::fmod(anim.time, clip->duration);
                if (anim.time < 0.0f) anim.time += clip->duration;
                anim.finished = false;
            } else {
                anim.finished = anim.time >= clip->duration;
                anim.time = std::clamp(anim.time, 0.0f, clip->duration);
            }
            Skinning::Sample(*clip, TrackBones(*clip, skeleton), anim.time, anim.pose);
            if (anim.inPlace) {
                // Ground plane is x/z in model space (y up): keep the rest pose's there.
                for (size_t i = 0; i < boneCount; ++i) {
                    if (skeleton.bones[i].parent >= 0) continue;
                    anim.pose[i].t[0] = skeleton.bones[i].bind.t[0];
                    anim.pose[i].t[2] = skeleton.bones[i].bind.t[2];
                }
            }
        }

        if (anim.fade > 0.0f && anim.fadeFrom.size() == boneCount) {
            anim.fade = std::max(0.0f, anim.fade - deltaTime);
            std::vector<BoneTransform> target = anim.pose;
            anim.pose = anim.fadeFrom;
            Skinning::Blend(anim.pose, target, 1.0f - anim.fade / anim.fadeTotal);
        }

        Skinning::SkinMatrices(skeleton, anim.pose, anim.skin);
        ++anim.poseVersion;
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::AnimationSystem)
