#pragma once

#include <vector>
#include <functional>
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

        using InspectorFunc = std::function<void(World*, Entity)>;
        const std::unordered_map<std::string, InspectorFunc>& GetInspectors() const { return inspectors_; }

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
        std::unordered_map<std::string, InspectorFunc> inspectors_;
        std::vector<std::function<void(sol::state&)>> scriptBinders_;
        std::unordered_map<std::string, LuaComponentAccess> scriptAccessors_;
    };

} // namespace Elysium

#define REGISTER_COMPONENT(Type) \
    static bool _registered_##Type = [] { \
        ::Elysium::ComponentRegistry::Instance().Register<Type>(); \
        return true; \
    }();
