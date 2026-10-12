#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    // A sound the entity makes. SoundSystem plays it while `playing` is set, and stops it when
    // it's cleared, the component goes, or the entity does. A sound that doesn't loop clears
    // `playing` itself when it ends; set it again to play it again.
    struct SoundComponent {
        std::string sound;       // a sound asset
        float volume = 1.0f;     // under its channel's
        bool loop = false;
        int channel = 2;         // a mixer channel (Services::AudioChannel), Effects by default
        bool playing = false;

        static constexpr const char* Name() { return "Sound"; }
        static constexpr const char* XmlTag() { return "SoundComponent"; }

        static void LoadXml(SoundComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        static void SaveXml(const SoundComponent& c, XMLBuilder& builder);
        static FieldList Fields();
        static void BindLua(sol::usertype<SoundComponent>& ut);
        static void SetFromLua(SoundComponent& c, sol::object v);
    };
}
