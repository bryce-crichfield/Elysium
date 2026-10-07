#pragma once
#include "Core/Animation.h"
#include "Core/Component.h"
#include <string>
#include <vector>

namespace Elysium {
    // Plays a skeletal animation clip on the entity's skinned model (a ModelComponent whose
    // .mesh has a .skel beside it). AnimationSystem advances it and poses the bones; the
    // renderer draws the model in that pose. Switching `clip` crossfades from the pose the old
    // one was in over `blend` seconds.
    struct AnimationComponent {
        std::string clip;        // an .anim asset
        float speed = 1.0f;      // playback rate
        float blend = 0.2f;      // crossfade seconds when the clip changes
        bool loop = true;        // else it holds its last frame
        bool playing = true;
        // Pin the root bones' ground travel to the rest pose, so a clip with root motion (a
        // Mixamo run not baked "In Place") plays on the spot; the up and down bob stays.
        // The entity's Transform does the moving.
        bool inPlace = true;

        // Runtime.
        float time = 0.0f;          // seconds into the clip
        float duration = 0.0f;      // the clip's length, once loaded
        bool finished = false;      // a clip that doesn't loop reached its end
        std::string current;        // the clip `time` belongs to
        std::vector<BoneTransform> pose;      // local, per bone
        std::vector<BoneTransform> fadeFrom;  // the pose a crossfade leaves
        float fade = 0.0f, fadeTotal = 0.0f;  // seconds left of it, and its length
        std::vector<Matrix> skin;   // per bone, for the renderer; empty when there's no pose
        uint64_t poseVersion = 0;   // bumped whenever `skin` changes

        static constexpr const char* Name() { return "Animation"; }
        static constexpr const char* XmlTag() { return "AnimationComponent"; }

        static void LoadXml(AnimationComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const AnimationComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<AnimationComponent>& ut);
        static void SetFromLua(AnimationComponent& c, sol::object v);
    };
}
