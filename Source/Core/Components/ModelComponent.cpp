#include "Core/Components/ModelComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/ServiceLocator.h"
#include "Core/Path.h"
#include "Core/Xml.h"
#include "Interfaces/IAssetService.h"

namespace Elysium {

    void ModelComponent::LoadXml(ModelComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services) {
        if (const char* path = el->Attribute("model")) c.modelPath = path;
        c.scale    = el->FloatAttribute("scale", c.scale);
        c.yaw      = el->FloatAttribute("yaw", c.yaw);
        c.centered = el->BoolAttribute("centered", c.centered);
        c.walkable = el->BoolAttribute("walkable", c.walkable);
        if (const char* tint = el->Attribute("tint")) c.tint = ParseHexColor(tint, c.tint);
        if (const char* emissive = el->Attribute("emissive")) c.emissive = ParseHexColor(emissive, c.emissive);
        if (!c.modelPath.empty()) services.Get<Services::IAssetService>().LoadAsset<Model>(Path(c.modelPath));
    }

    void ModelComponent::SaveXml(const ModelComponent& c, XMLBuilder& builder) {
        auto element = builder.AddElement("ModelComponent")
            .SetAttribute("model", c.modelPath.c_str())
            .SetAttribute("scale", c.scale);
        if (c.yaw != 0.0f) element.SetAttribute("yaw", c.yaw);
        if (!c.centered) element.SetAttribute("centered", c.centered);
        if (c.walkable) element.SetAttribute("walkable", c.walkable);
        if (c.tint.r != 255 || c.tint.g != 255 || c.tint.b != 255 || c.tint.a != 255)
            element.SetAttribute("tint", ColorToHex(c.tint).c_str());
        if (c.emissive.r != 0 || c.emissive.g != 0 || c.emissive.b != 0)
            element.SetAttribute("emissive", ColorToHex(c.emissive).c_str());
    }

    FieldList ModelComponent::Fields() {
        return {
            Field("Model", &ModelComponent::modelPath, "model").Asset(AssetKind::Model),
            Field("Scale", &ModelComponent::scale, "scale").Speed(0.1f).Range(0.001f, 10000.0f),
            Field("Yaw", &ModelComponent::yaw, "yaw").Range(-360.0f, 360.0f),
            Field("Tint", &ModelComponent::tint, "tint"),
            Field("Emissive", &ModelComponent::emissive, "emissive"),
            Field("Centered", &ModelComponent::centered, "centered"),
            Field("Walkable", &ModelComponent::walkable, "walkable"),
        };
    }

    void ModelComponent::BindLua(sol::usertype<ModelComponent>& ut) {
        ut["model"]    = &ModelComponent::modelPath;
        ut["scale"]    = &ModelComponent::scale;
        ut["yaw"]      = &ModelComponent::yaw;
        ut["tint"]     = &ModelComponent::tint;
        ut["centered"] = &ModelComponent::centered;
        ut["walkable"] = &ModelComponent::walkable;
        ut["emissive"] = &ModelComponent::emissive;
    }

    void ModelComponent::SetFromLua(ModelComponent& c, sol::object v) {
        if (!v.is<sol::table>()) return;
        sol::table t = v.as<sol::table>();
        if (t["model"].valid()) c.modelPath = t["model"].get<std::string>();
        if (t["scale"].valid()) c.scale = t["scale"];
        if (t["yaw"].valid())   c.yaw = t["yaw"];
        if (t["walkable"].valid()) c.walkable = t["walkable"];
    }

    REGISTER_COMPONENT(ModelComponent);
}
