#include "Core/Components/AnimationComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/ServiceLocator.h"
#include "Core/Path.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"

namespace Elysium {

    void AnimationComponent::LoadXml(AnimationComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        if (const char* clip = el->Attribute("clip")) c.clip = clip;
        c.speed   = el->FloatAttribute("speed", c.speed);
        c.blend   = el->FloatAttribute("blend", c.blend);
        c.loop    = el->BoolAttribute("loop", c.loop);
        c.playing = el->BoolAttribute("playing", c.playing);
        c.inPlace = el->BoolAttribute("inPlace", c.inPlace);
        if (!c.clip.empty()) services.Get<Services::IAssetService>().LoadAsset<Animation>(Path(c.clip));
    }

    void AnimationComponent::SaveXml(const AnimationComponent& c, XMLBuilder& builder) {
        auto element = builder.AddElement("AnimationComponent").SetAttribute("clip", c.clip.c_str());
        if (c.speed != 1.0f) element.SetAttribute("speed", c.speed);
        if (c.blend != 0.2f) element.SetAttribute("blend", c.blend);
        if (!c.loop) element.SetAttribute("loop", c.loop);
        if (!c.playing) element.SetAttribute("playing", c.playing);
        if (!c.inPlace) element.SetAttribute("inPlace", c.inPlace);
    }

    FieldList AnimationComponent::Fields() {
        return {
            Field("Clip", &AnimationComponent::clip, "clip").Asset(AssetKind::Animation),
            Field("Speed", &AnimationComponent::speed, "speed").Speed(0.05f).Range(0.0f, 10.0f),
            Field("Blend", &AnimationComponent::blend, "blend").Speed(0.01f).Range(0.0f, 5.0f),
            Field("Loop", &AnimationComponent::loop, "loop"),
            Field("Playing", &AnimationComponent::playing, "playing"),
            Field("In Place", &AnimationComponent::inPlace, "inPlace"),
            Field("Time", &AnimationComponent::time).Section("State"),
            Field("Duration", &AnimationComponent::duration).Section("State"),
            Field("Finished", &AnimationComponent::finished).Section("State"),
        };
    }

    void AnimationComponent::BindLua(sol::usertype<AnimationComponent>& ut) {
        // Set `clip` to play another one (crossfading); set `time` to 0 to restart this one.
        ut["clip"]     = &AnimationComponent::clip;
        ut["speed"]    = &AnimationComponent::speed;
        ut["blend"]    = &AnimationComponent::blend;
        ut["loop"]     = &AnimationComponent::loop;
        ut["playing"]  = &AnimationComponent::playing;
        ut["inPlace"]  = &AnimationComponent::inPlace;
        ut["time"]     = &AnimationComponent::time;
        ut["duration"] = sol::readonly(&AnimationComponent::duration);
        ut["finished"] = sol::readonly(&AnimationComponent::finished);
        // The clip `time` and `duration` belong to: lags `clip` by a frame after a switch.
        ut["current"]  = sol::readonly(&AnimationComponent::current);
    }

    void AnimationComponent::SetFromLua(AnimationComponent& c, sol::object v) {
        if (!v.is<sol::table>()) return;
        sol::table t = v.as<sol::table>();
        c.clip    = t.get_or("clip", c.clip);
        c.speed   = t.get_or("speed", c.speed);
        c.blend   = t.get_or("blend", c.blend);
        c.loop    = t.get_or("loop", c.loop);
        c.playing = t.get_or("playing", c.playing);
        c.inPlace = t.get_or("inPlace", c.inPlace);
        if (t["time"].valid()) c.time = t["time"];
    }

    REGISTER_COMPONENT(AnimationComponent);
}
