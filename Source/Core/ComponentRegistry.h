#pragma once

#include <vector>
#include <functional>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>
#include <set>

#include "Core/Entity.h"
#include "Core/Component.h"
#include "Core/World.h"

#include "Core/Xml.h"
#include "Core/Editor.h"
#include "Core/Reflection.h"
#include <sol/sol.hpp>

namespace Elysium {

    class World;

    // Override field paths. A top-level field is just the attribute name ("x"); a nested one
    // names each element on the way down, then the attribute: "Layer[Texture]/Uniform[uColor]/value".
    // An element is keyed by its `name` attribute when it has one, else by its position among
    // same-tag siblings ("Layer#1"), so a prefab adding a uniform doesn't misdirect overrides.
    inline std::string FieldKey(const tinyxml2::XMLElement* element) {
        if (const char* name = element->Attribute("name")) return std::string(element->Name()) + "[" + name + "]";
        int index = 0;
        for (auto* e = element->PreviousSiblingElement(element->Name()); e; e = e->PreviousSiblingElement(element->Name())) ++index;
        return std::string(element->Name()) + "#" + std::to_string(index);
    }

    inline tinyxml2::XMLElement* FindChildByKey(tinyxml2::XMLElement* parent, const std::string& key) {
        for (auto* child = parent->FirstChildElement(); child; child = child->NextSiblingElement()) {
            if (FieldKey(child) == key) return child;
        }
        return nullptr;
    }

    // The element `field` lives on (under `component`) and its attribute name, or null.
    inline tinyxml2::XMLElement* ResolveField(tinyxml2::XMLElement* component, const std::string& field, std::string& attr) {
        tinyxml2::XMLElement* element = component;
        size_t start = 0;
        for (size_t slash; element && (slash = field.find('/', start)) != std::string::npos; start = slash + 1) {
            element = FindChildByKey(element, field.substr(start, slash - start));
        }
        attr = field.substr(start);
        return element;
    }

    class ComponentRegistry {
    public:
        static ComponentRegistry& Instance();

        // Prefab override support, keyed by XML tag (the "component" of an <Override>).
        // Only for components that round-trip through XML (loadable and savable).
        struct PrefabFieldSupport {
            // Serializes the entity's component into `scratch`; null if absent or nothing written.
            std::function<tinyxml2::XMLElement*(tinyxml2::XMLDocument& scratch, World*, Entity)> serialize;
            // Overwrites one serialized field and reloads the component through its LoadXml.
            // `field` is an attribute of the component element, or a path to one on a nested
            // element (see ResolveField).
            std::function<void(World*, Entity, const std::string& field, const std::string& value, ServiceLocator&)> applyOverride;
        };

        // Generic per-component operations keyed by XML tag, for code that has to round-trip a
        // single component without knowing its type — the editor's undo history and clipboard.
        // Same gate as prefab overrides: a component is only here if XML can round-trip it.
        struct XmlComponentOps {
            const char* displayName = nullptr;  // T::Name(), what the inspector tables are keyed by
            // Serializes the entity's component into `scratch`; null if absent or nothing written.
            std::function<tinyxml2::XMLElement*(tinyxml2::XMLDocument& scratch, World*, Entity)> serialize;
            // Loads `element` onto the entity, adding the component if it is missing. Loads on top
            // of the live value so runtime-only state (a resolved parent Entity, an initialized
            // script flag) survives a reload of the serialized fields.
            std::function<void(tinyxml2::XMLElement* element, World*, Entity, ServiceLocator&)> apply;
            std::function<void(World*, Entity)> remove;
            std::function<bool(World*, Entity)> has;
        };

        // Register a component type
        template<typename T>
        void Register() {
            // 1. Register for ECS (World)
            worldRegistrars_.push_back([](World& w) {
                w.RegisterComponent<T>();
            });

            const char* name = T::Name();
            const char* tag = name;
            if constexpr (requires { T::XmlTag(); }) tag = T::XmlTag();

            // Typed fields, by display name and by XML tag (what prefab parameters name).
            if constexpr (Reflected<T>) {
                fields_[name] = T::Fields();
                fields_[tag] = T::Fields();
            }
            // Placement-owned: a prefab placement's own data (where it is, what it's called),
            // editable on its root; everything else inside a placement is the prefab's.
            if constexpr (requires { { T::PlacementOwned } -> std::convertible_to<bool>; }) {
                if (T::PlacementOwned) {
                    placementOwned_.insert(name);
                    placementOwned_.insert(tag);
                }
            }

            // 2. Register XML Loader
            if constexpr (XmlLoadable<T>) {
                const char* xmlTag = name;

                if constexpr (requires { T::XmlTag(); }) {
                    xmlTag = T::XmlTag();
                }

                xmlLoaders_[xmlTag] = [](XMLElement* el, World* w, Entity e, ServiceLocator& services) {
                    T comp{};
                    T::LoadXml(comp, el, services);
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
                    return after != before ? after : nullptr;
                };

                // 3c. Generic ops for the editor's command history, sharing the same serializer.
                XmlComponentOps ops;
                ops.displayName = name;
                ops.serialize = support.serialize;
                ops.apply = [](tinyxml2::XMLElement* element, World* w, Entity e, ServiceLocator& services) {
                    T comp = w->HasComponent<T>(e) ? w->GetComponent<T>(e) : T{};
                    T::LoadXml(comp, element, services);
                    if (w->HasComponent<T>(e)) {
                        w->GetComponent<T>(e) = std::move(comp);
                    } else {
                        w->AddComponent<T>(e, std::move(comp));
                    }
                };
                ops.remove = [](World* w, Entity e) {
                    if (w->HasComponent<T>(e)) w->RemoveComponent<T>(e);
                };
                ops.has = [](World* w, Entity e) { return w->HasComponent<T>(e); };
                xmlComponentOps_[fieldXmlTag] = std::move(ops);
                xmlTagByName_[name] = fieldXmlTag;
                support.applyOverride = [fieldXmlTag](World* w, Entity e, const std::string& field, const std::string& value,
                                                      ServiceLocator& services) {
                    if (!w->HasComponent<T>(e)) return;

                    tinyxml2::XMLDocument scratch;
                    tinyxml2::XMLElement* scratchRoot = scratch.NewElement("Scratch");
                    scratch.InsertFirstChild(scratchRoot);
                    XMLBuilder builder(&scratch, scratchRoot);
                    T::SaveXml(w->GetComponent<T>(e), builder);

                    tinyxml2::XMLElement* compElem = scratchRoot->FirstChildElement();
                    if (!compElem) {
                        // Saver wrote nothing (all defaults): give the override somewhere to land.
                        compElem = scratch.NewElement(fieldXmlTag);
                        scratchRoot->InsertFirstChild(compElem);
                    }
                    std::string attr;
                    tinyxml2::XMLElement* target = ResolveField(compElem, field, attr);
                    if (!target) return;  // the nested element it names no longer exists
                    target->SetAttribute(attr.c_str(), value.c_str());

                    // Reload on top of the live value so runtime-only state (e.g. a resolved
                    // parent Entity) survives.
                    T comp = w->GetComponent<T>(e);
                    T::LoadXml(comp, compElem, services);
                    w->GetComponent<T>(e) = std::move(comp);
                };

                prefabFieldSupport_[fieldXmlTag] = std::move(support);
            }

            // 4. Register Inspector
            // Its own Inspect, or else one drawn from its fields.
            if constexpr (Inspectable<T>) {
                inspectorOrder_[name] = InspectorOrderOf<T>();
                inspectors_[name] = [](World* w, Entity e, ServiceLocator& services) {
                    if (w->HasComponent<T>(e)) {
                        auto& comp = w->GetComponent<T>(e);
                        T::Inspect(comp, e, services);
                    }
                };
            } else if constexpr (Reflected<T>) {
                inspectorOrder_[name] = InspectorOrderOf<T>();
                inspectors_[name] = [fields = T::Fields()](World* w, Entity e, ServiceLocator&) {
                    if (w->HasComponent<T>(e)) InspectFields(&w->GetComponent<T>(e), fields);
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
        
        using XmlLoaderFunc = std::function<void(tinyxml2::XMLElement*, World*, Entity, ServiceLocator&)>;
        const std::unordered_map<std::string, XmlLoaderFunc>& GetXmlLoaders() const { return xmlLoaders_; }

        using XmlSaverFunc = std::function<void(XMLBuilder&, World*, Entity)>;
        const std::map<std::string, XmlSaverFunc>& GetXmlSavers() const { return xmlSavers_; }

        using InspectorFunc = std::function<void(World*, Entity, ServiceLocator&)>;
        const std::unordered_map<std::string, InspectorFunc>& GetInspectors() const { return inspectors_; }
        InspectorOrder GetInspectorOrder(const std::string& name) const {
            auto it = inspectorOrder_.find(name);
            return it == inspectorOrder_.end() ? InspectorOrder::Other : it->second;
        }

        const std::unordered_map<std::string, PrefabFieldSupport>& GetPrefabFieldSupport() const { return prefabFieldSupport_; }

        // Generic single-component round-tripping, keyed by XML tag. Null for a component XML
        // can't round-trip (missing LoadXml or SaveXml), which is also the set of components the
        // undo history can't record — see ComponentEditCommand.
        const XmlComponentOps* GetXmlOps(const std::string& xmlTag) const {
            auto it = xmlComponentOps_.find(xmlTag);
            return it == xmlComponentOps_.end() ? nullptr : &it->second;
        }
        // The XML tag of the component whose T::Name() is `name` (the key the inspector and
        // placeholder tables use). Empty when it doesn't round-trip through XML.
        const std::string& GetXmlTag(const std::string& name) const {
            static const std::string none;
            auto it = xmlTagByName_.find(name);
            return it == xmlTagByName_.end() ? none : it->second;
        }

        // A component's typed fields, by display name or XML tag; null if it has none.
        const FieldList* GetFields(const std::string& component) const {
            auto it = fields_.find(component);
            return it == fields_.end() ? nullptr : &it->second;
        }
        // The field `key` (an XML attribute) of `component`, or null.
        const FieldInfo* FindField(const std::string& component, const std::string& key) const {
            const FieldList* fields = GetFields(component);
            if (!fields) return nullptr;
            for (const auto& field : *fields) {
                if (field.key == key) return &field;
            }
            return nullptr;
        }
        bool IsPlacementOwned(const std::string& component) const { return placementOwned_.count(component) > 0; }

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
        std::unordered_map<std::string, InspectorOrder> inspectorOrder_;
        std::unordered_map<std::string, PrefabFieldSupport> prefabFieldSupport_;
        std::unordered_map<std::string, XmlComponentOps> xmlComponentOps_;
        std::unordered_map<std::string, std::string> xmlTagByName_;
        std::unordered_map<std::string, FieldList> fields_;
        std::set<std::string> placementOwned_;
        std::vector<std::function<void(sol::state&)>> scriptBinders_;
        std::unordered_map<std::string, LuaComponentAccess> scriptAccessors_;
    };

} // namespace Elysium

#define REGISTER_COMPONENT(Type) \
    static bool _registered_##Type = [] { \
        ::Elysium::ComponentRegistry::Instance().Register<Type>(); \
        return true; \
    }();
