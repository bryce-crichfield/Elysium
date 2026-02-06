#include "Components/AnimationComponent.h"
#include "Core/ComponentRegistry.h"
#include "imgui.h"

namespace Elysium {
    AnimationComponent::AnimationComponent(std::string marker) {}

    void AnimationComponent::LoadXml(AnimationComponent& c, tinyxml2::XMLElement* el) {
        // No attrs loaded in original
    }

    void AnimationComponent::Inspect(AnimationComponent& c, Entity e) {
    }

    void AnimationComponent::BindLua(sol::usertype<AnimationComponent>& ut) {
      
    }

    void AnimationComponent::SetFromLua(AnimationComponent& c, sol::object v) {
       
    }

    REGISTER_COMPONENT(AnimationComponent);
}
