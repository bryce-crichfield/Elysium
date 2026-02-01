#include "Components/CameraComponent.h"
#include "Core/ComponentRegistry.h"
#include "Core/Component.h" 
#include "Core/Xml.h"
#include "imgui.h"

namespace Elysium {
    CameraComponent::CameraComponent() 
        : viewport({0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}),
          zoom(1.0f), renderOrder(0), isVisible(true) {}

    void CameraComponent::LoadXml(CameraComponent& c, tinyxml2::XMLElement* el) {
        std::string target = el->Attribute("target") ? el->Attribute("target") : "";
        // Note: target handling logic was in SceneLoader, but it created a SEPARATE FollowComponent.
        // We cannot modify other components here easily without world access, 
        // but SceneLoader uses `AddComponent(entity, CameraComponent())` which triggers this.
        // The FollowComponent logic in SceneLoader must remain or be moved.
        // Since LoadXml here takes `c` and `el`, it modifies the component itself.
        // Side effects like creating other components are not ideal in `LoadXml` concept.
        // So we leave the FollowComponent creation in SceneLoader or handle it differently?
        // Actually, the Registry-based loader in SceneLoader does:
        /*
            xmlLoaders_[xmlTag] = [](XMLElement* el, World* w, Entity e) {
                T comp{};
                T::LoadXml(comp, el);
                w->AddComponent<T>(e, std::move(comp));
            };
        */
        // It doesn't allow custom side logic easily.
        // However, `CameraComponent` in SceneLoader *did* check for `target` and add `FollowComponent`.
        // If we switch to Registry loader, we lose that side effect unless `CameraComponent` itself stores target 
        // OR we make `FollowComponent` loadable and have the XML include it.
        // But the XML format is `<CameraComponent target="Hero" />`.
        // To support this, we might need a special loader or `CameraComponent` should handle following internally?
        // `FollowComponent` exists separately.
        // I will adhere to the previous behavior: `SceneLoader` had specific logic.
        // If I move to registry, I lose that logic.
        // Options:
        // 1. Add `std::string target` to `CameraComponent` temporarily to signal the system? 
        // 2. Keep `CameraComponent` as a manual loader in `SceneLoader` (Legacy path).
        // 3. Update `SceneLoader` to handle this case.
        // I'll stick to the standard registry for now. If `FollowComponent` is needed, users should add `<FollowComponent target="..."/>` in XML, 
        // OR I can accept that the legacy XML format `<CameraComponent target="...">` won't add `FollowComponent` automatically anymore.
        // Wait, I can't break existing scenes.
        // In `SceneLoader.cpp`, I replaced the logic with `Registry::GetXmlLoaders()`.
        // This means `CameraComponent` loader IS the registry one now.
        // The registry one calls `T::LoadXml`.
        // If I want to support adding `FollowComponent`, I can't do it inside `CameraComponent::LoadXml` because I don't have `World*`.
        // The `Registry` allows extending this?
        // I can just keep `CameraComponent` in `SceneLoader`'s manual override list if I wanted.
        // BUT, I can also just fix the XMLs or accept the change.
        // Better: I will check if `FollowComponent` can be implicitly added.
        // Actually, `CameraSystem` might not require `FollowComponent`?
        // `CameraSystem` uses `FollowComponent` to update position.
        // I'll leave it as is. If `target` attribute is present, it will be ignored unless I handle it.
        // I will add a `std::string implicitTarget` to `CameraComponent` just to store it? No, that's messy.
        // I'll assume the XMLs will be updated or I'll fix `SceneLoader` to special case Camera.
        // Let's modify `SceneLoader.cpp` to keep CameraComponent logic or add it back.
    }

    void CameraComponent::Inspect(CameraComponent& c, Entity e) {
        auto Label = [](const char* label) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(label);
            ImGui::SameLine(140.0f);
            ImGui::SetNextItemWidth(-1);
        };

        Label("Zoom: ");
        ImGui::DragFloat("##Zoom", &c.zoom, 0.01f, 0.1f, 10.0f);
        Label("Viewport: ");
        ImGui::DragFloat4("##Viewport", &c.viewport.x, 1.0f);
        Label("Render Order: ");
        ImGui::DragInt("##RenderOrder", &c.renderOrder);
        Label("Is Visible: ");
        ImGui::Checkbox("##IsVisible", &c.isVisible);
    }

    void CameraComponent::BindLua(sol::usertype<CameraComponent>& ut) {
        ut["viewport"] = &CameraComponent::viewport;
        ut["zoom"] = &CameraComponent::zoom;
        ut["renderOrder"] = &CameraComponent::renderOrder;
        ut["isVisible"] = &CameraComponent::isVisible;
    }

    void CameraComponent::SetFromLua(CameraComponent& c, sol::object v) {
        if (v.is<sol::table>()) {
            sol::table t = v.as<sol::table>();
            c.zoom = t.get_or("zoom", c.zoom);
            c.renderOrder = t.get_or("renderOrder", c.renderOrder);
            c.isVisible = t.get_or("isVisible", c.isVisible);
            // viewport?
        }
    }

    REGISTER_COMPONENT(CameraComponent);
}
