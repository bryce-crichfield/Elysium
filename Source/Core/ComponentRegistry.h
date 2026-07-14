#pragma once

#include <vector>
#include <functional>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>

#include "Core/Entity.h"
#include "Core/Component.h"
#include "Core/World.h"

#include "Core/Xml.h"
#include "Core/Editor.h"
#include <sol/sol.hpp>

namespace Elysium {

    class World;

    class ComponentRegistry {
    public:
        static ComponentRegistry& Instance();

        // Support for prefab override diffing/application, keyed by XmlTag (the same
        // key space as xmlLoaders_ and the "component" attribute on <Override> elements).
        // Only registered for components that round-trip through XML (both loadable and
        // savable) — components without a SaveXml (e.g. HealthComponent) can't be diffed
        // and so can't carry per-instance prefab overrides, same as they already can't be
        // persisted at all when flattened into a plain <Entity> block.
        struct PrefabFieldSupport {
            // Serializes the entity's current component value as a fresh XML element
            // appended to `scratch`'s root (creating a root if needed). Returns nullptr if
            // the entity lacks the component, or if the saver chose not to emit anything
            // (e.g. a component that's fully at its type default).
            std::function<tinyxml2::XMLElement*(tinyxml2::XMLDocument& scratch, World*, Entity)> serialize;
            // Applies a single field override on top of the entity's current component
            // value: serializes the current value, overwrites just `field`, then reloads
            // through the component's own LoadXml so every other field is preserved.
            std::function<void(World*, Entity, const std::string& field, const std::string& value)> applyOverride;
        };

        // Register a component type
        template<typename T>
        void Register() {
            // 1. Register for ECS (World)
            worldRegistrars_.push_back([](World& w) {
                w.RegisterComponent<T>();
            });

            const char* name = T::Name();

            // 2. Register XML Loader
            if constexpr (XmlLoadable<T>) {
                const char* xmlTag = name;
                
                if constexpr (requires { T::XmlTag(); }) {
                    xmlTag = T::XmlTag();
                }

                xmlLoaders_[xmlTag] = [](XMLElement* el, World* w, Entity e) {
                    T comp{};
                    T::LoadXml(comp, el);
                    w->AddComponent<T>(e, std::move(comp));
                };
            }

            // 3. Register XML Saver
            if constexpr (XmlSavable<T>) {
                xmlSavers_[name] = [](XMLBuilder& builder, World* w, Entity e) {
                    if (w->HasComponent<T>(e)) {
                        T::SaveXml(w->GetComponent<T>(e), builder);
                    }
                };
            }

            // 3b. Register prefab field support (diff + override application)
            if constexpr (XmlLoadable<T> && XmlSavable<T>) {
                const char* fieldXmlTag = name;
                if constexpr (requires { T::XmlTag(); }) {
                    fieldXmlTag = T::XmlTag();
                }

                PrefabFieldSupport support;
                support.serialize = [](tinyxml2::XMLDocument& scratch, World* w, Entity e) -> tinyxml2::XMLElement* {
                    if (!w->HasComponent<T>(e)) return nullptr;
                    tinyxml2::XMLElement* scratchRoot = scratch.RootElement();
                    if (!scratchRoot) {
                        scratchRoot = scratch.NewElement("Scratch");
                        scratch.InsertFirstChild(scratchRoot);
                    }
                    tinyxml2::XMLElement* before = scratchRoot->LastChildElement();
                    XMLBuilder builder(&scratch, scratchRoot);
                    T::SaveXml(w->GetComponent<T>(e), builder);
                    tinyxml2::XMLElement* after = scratchRoot->LastChildElement();
                    return (after != before) ? after : nullptr;
                };
                support.applyOverride = [fieldXmlTag](World* w, Entity e, const std::string& field, const std::string& value) {
                    if (!w->HasComponent<T>(e)) return;

                    tinyxml2::XMLDocument scratch;
                    tinyxml2::XMLElement* scratchRoot = scratch.NewElement("Scratch");
                    scratch.InsertFirstChild(scratchRoot);
                    XMLBuilder builder(&scratch, scratchRoot);
                    T::SaveXml(w->GetComponent<T>(e), builder);

                    tinyxml2::XMLElement* compElem = scratchRoot->FirstChildElement();
                    if (!compElem) {
                        // Saver emitted nothing (component is at its "don't write" default) —
                        // synthesize an empty element so the override still has somewhere to land.
                        compElem = scratch.NewElement(fieldXmlTag);
                        scratchRoot->InsertFirstChild(compElem);
                    }
                    compElem->SetAttribute(field.c_str(), value.c_str());

                    T comp{};
                    T::LoadXml(comp, compElem);
                    w->GetComponent<T>(e) = comp;
                };

                prefabFieldSupport_[fieldXmlTag] = std::move(support);
            }

            // 4. Register Inspector
            if constexpr (Inspectable<T>) {
                inspectors_[name] = [](World* w, Entity e) {
                    if (w->HasComponent<T>(e)) {
                        auto& comp = w->GetComponent<T>(e);
                        T::Inspect(comp, e);
                    }
                };
            }

            // 5. Register Script Binding
            if constexpr (Scriptable<T>) {
                scriptBinders_.push_back([](sol::state& lua) {
                    sol::usertype<T> ut = lua.new_usertype<T>(T::Name(), sol::constructors<T()>());
                    T::BindLua(ut);
                });
            }
            
            // 6. Register generic "Add/Set/Get" for Lua (Dynamic access)
            // This is complex because we need to bridge the compile-time T to runtime strings
            scriptAccessors_[name] = LuaComponentAccess {
                .add = [](World* w, Entity e) { w->AddComponent<T>(e, T{}); },
                .remove = [](World* w, Entity e) { w->RemoveComponent<T>(e); },
                .has = [](World* w, Entity e) { return w->HasComponent<T>(e); },
                .get = [](World* w, Entity e, sol::state_view& lua) -> sol::object { 
                     if(w->HasComponent<T>(e)) return sol::make_object(lua, std::ref(w->GetComponent<T>(e)));
                     return sol::nil;
                },
                .set = [](World* w, Entity e, sol::object v) {
                    if constexpr (LuaSettable<T>) {
                        if (w->HasComponent<T>(e)) {
                            T::SetFromLua(w->GetComponent<T>(e), v);
                        }
                    }
                }
            };
        }

        // Apply registrations
        void RegisterAllComponents(World& world);
        
        using XmlLoaderFunc = std::function<void(tinyxml2::XMLElement*, World*, Entity)>;
        const std::unordered_map<std::string, XmlLoaderFunc>& GetXmlLoaders() const { return xmlLoaders_; }

        using XmlSaverFunc = std::function<void(XMLBuilder&, World*, Entity)>;
        const std::map<std::string, XmlSaverFunc>& GetXmlSavers() const { return xmlSavers_; }

        using InspectorFunc = std::function<void(World*, Entity)>;
        const std::unordered_map<std::string, InspectorFunc>& GetInspectors() const { return inspectors_; }

        const std::unordered_map<std::string, PrefabFieldSupport>& GetPrefabFieldSupport() const { return prefabFieldSupport_; }

        void BindAllScripts(sol::state& lua);

        struct LuaComponentAccess {
            std::function<void(World*, Entity)> add;
            std::function<void(World*, Entity)> remove;
            std::function<bool(World*, Entity)> has;
            std::function<sol::object(World*, Entity, sol::state_view&)> get;
            std::function<void(World*, Entity, sol::object)> set;
        };
        
        const LuaComponentAccess* GetLuaAccess(const std::string& name) const;

    private:
        std::vector<std::function<void(World&)>> worldRegistrars_;
        std::unordered_map<std::string, XmlLoaderFunc> xmlLoaders_;
        std::map<std::string, XmlSaverFunc> xmlSavers_;
        std::unordered_map<std::string, InspectorFunc> inspectors_;
        std::unordered_map<std::string, PrefabFieldSupport> prefabFieldSupport_;
        std::vector<std::function<void(sol::state&)>> scriptBinders_;
        std::unordered_map<std::string, LuaComponentAccess> scriptAccessors_;
    };

} // namespace Elysium

#define REGISTER_COMPONENT(Type) \
    static bool _registered_##Type = [] { \
        ::Elysium::ComponentRegistry::Instance().Register<Type>(); \
        return true; \
    }();
