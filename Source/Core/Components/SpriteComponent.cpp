#include "Core/Components/SpriteComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Sprite.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"
#include "Services/AssetService.h"
#include "Services/LogService.h"
#include <set>

namespace Elysium {
    SpriteComponent::SpriteComponent(const Sprite& sprite, const std::string& sequence)
        : spriteName(sprite.name), sequenceName(sequence) {}

    void SpriteComponent::SaveXml(const SpriteComponent& c, XMLBuilder& builder) {
        if (c.spriteName.empty()) return;
        builder.AddElement("SpriteComponent")
            .SetAttribute("spriteName", c.spriteName.c_str())
            .SetAttribute("sheetName", c.sheetName.c_str())
            .SetAttribute("sequenceName", c.sequenceName.c_str());
    }

    void SpriteComponent::LoadXml(SpriteComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        const char* spriteName = el->Attribute("spriteName");
        const char* sheetName = el->Attribute("sheetName");
        const char* sequenceName = el->Attribute("sequenceName");

        if (spriteName) {
            c.spriteName = spriteName;

            // Load the sprite if not already loaded
            auto& assetService = services.Get<Elysium::Services::IAssetService>();
            assetService.LoadAsset<Sprite>(Path(spriteName));
        }
        if (sheetName) {
            c.sheetName = sheetName;
        }
        if (sequenceName) {
            c.sequenceName = sequenceName;
        }

        if (!spriteName || !sheetName || !sequenceName) {
            LOG_WARNING("Scene", "SpriteComponent missing attributes: spriteName, sheetName, or sequenceName");
        }
    }

    FieldList SpriteComponent::Fields() {
        return {
            Field("Sprite", &SpriteComponent::spriteName, "spriteName").Asset(AssetKind::Sprite),
            Field("Sheet", &SpriteComponent::sheetName, "sheetName"),
            Field("Sequence", &SpriteComponent::sequenceName, "sequenceName"),
        };
    }

    void SpriteComponent::BindLua(sol::usertype<SpriteComponent>& ut) {
        ut["sprite"] = &SpriteComponent::spriteName;
        ut["sheet"] = &SpriteComponent::sheetName;
        ut["sequence"] = &SpriteComponent::sequenceName;
        ut["duration"] = &SpriteComponent::frameDuration;
    }

    void SpriteComponent::SetFromLua(SpriteComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.spriteName = t.get_or("sprite", c.spriteName);
            c.sheetName = t.get_or("sheet", c.sheetName);
            c.sequenceName = t.get_or("sequence", c.sequenceName);
        }
    }

    REGISTER_COMPONENT(SpriteComponent);
}
