#include "Core/Components/SoundComponent.h"
#include "Core/Audio.h"
#include "Core/ComponentRegistry.h"
#include "Core/Path.h"
#include "Core/ServiceLocator.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"

namespace Elysium {

    void SoundComponent::LoadXml(SoundComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        if (const char* sound = el->Attribute("sound")) c.sound = sound;
        c.volume  = el->FloatAttribute("volume", c.volume);
        c.loop    = el->BoolAttribute("loop", c.loop);
        c.channel = el->IntAttribute("channel", c.channel);
        c.playing = el->BoolAttribute("playing", c.playing);
        if (!c.sound.empty()) services.Get<Services::IAssetService>().LoadAsset<Sound>(Path(c.sound));
    }

    void SoundComponent::SaveXml(const SoundComponent& c, XMLBuilder& builder) {
        auto element = builder.AddElement("SoundComponent").SetAttribute("sound", c.sound.c_str());
        if (c.volume != 1.0f) element.SetAttribute("volume", c.volume);
        if (c.loop) element.SetAttribute("loop", c.loop);
        if (c.channel != 2) element.SetAttribute("channel", c.channel);
        if (c.playing) element.SetAttribute("playing", c.playing);
    }

    FieldList SoundComponent::Fields() {
        return {
            Field("Sound", &SoundComponent::sound, "sound").Asset(AssetKind::Sound),
            Field("Volume", &SoundComponent::volume, "volume").Speed(0.01f).Range(0.0f, 1.0f),
            Field("Loop", &SoundComponent::loop, "loop"),
            // In Services::AudioChannel order.
            Field("Channel", &SoundComponent::channel, "channel").Choices({"Master", "Music", "Effects", "Ambient", "Dialogue"}),
            Field("Playing", &SoundComponent::playing, "playing"),
        };
    }

    void SoundComponent::BindLua(sol::usertype<SoundComponent>& ut) {
        ut["sound"]   = &SoundComponent::sound;
        ut["volume"]  = &SoundComponent::volume;
        ut["loop"]    = &SoundComponent::loop;
        ut["channel"] = &SoundComponent::channel;
        ut["playing"] = &SoundComponent::playing;
    }

    void SoundComponent::SetFromLua(SoundComponent& c, sol::object v) {
        if (!v.is<sol::table>()) return;
        sol::table t = v.as<sol::table>();
        c.sound   = t.get_or("sound", c.sound);
        c.volume  = t.get_or("volume", c.volume);
        c.loop    = t.get_or("loop", c.loop);
        c.channel = t.get_or("channel", c.channel);
        c.playing = t.get_or("playing", c.playing);
    }

    REGISTER_COMPONENT(SoundComponent);
}
