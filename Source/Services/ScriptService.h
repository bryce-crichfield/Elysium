#pragma once

#include "Service.h"
#include <string>
#include <unordered_map>
#include "Core/Entity.h"
#include "Core/Event.h"
#include <sol/sol.hpp>

namespace Elysium::Services {

class IComponentAdapter {
public:
    virtual ~IComponentAdapter() = default;
    
    virtual void RegisterUserType(sol::state& lua) = 0;
    virtual bool Has(World* world, Entity entity) const = 0;
    virtual void Add(World* world, Entity entity) = 0;
    virtual void Remove(World* world, Entity entity) = 0;
    virtual sol::object Get(sol::state& lua, World* world, Entity entity) = 0;
    virtual void Set(World* world, Entity entity, sol::object value) = 0;
    virtual std::string GetName() const = 0;
};

template<typename T>
class ComponentAdapter : public IComponentAdapter {
private:
    std::string name_;
    std::function<void(sol::usertype<T>&)> bindFunc_;
    std::function<void(T&, sol::object)> setFromTableFunc_;

public:
    ComponentAdapter(const std::string& name, 
                     std::function<void(sol::usertype<T>&)> bindFunc = nullptr,
                     std::function<void(T&, sol::object)> setFromTable = nullptr)
        : name_(name), bindFunc_(bindFunc), setFromTableFunc_(setFromTable) {}

    void RegisterUserType(sol::state& lua) override {
        auto usertype = lua.new_usertype<T>(name_, sol::constructors<T()>());
        if (bindFunc_) {
            bindFunc_(usertype);
        }
    }

    bool Has(World* world, Entity entity) const override {
        return world->HasComponent<T>(entity);
    }

    void Add(World* world, Entity entity) override {
        if (!world->HasComponent<T>(entity)) {
            world->AddComponent<T>(entity, T{});
        }
    }

    void Remove(World* world, Entity entity) override {
        world->RemoveComponent<T>(entity);
    }

    sol::object Get(sol::state& lua, World* world, Entity entity) override {
        if (world->HasComponent<T>(entity)) {
            return sol::make_object(lua, &world->GetComponent<T>(entity));
        }
        return sol::nil;
    }

    void Set(World* world, Entity entity, sol::object value) override {
        if (!world->HasComponent<T>(entity)) {
            world->AddComponent<T>(entity, T{});
        }
        
        T& comp = world->GetComponent<T>(entity);
        
        if (value.is<T>()) {
            comp = value.as<T>();
        } else if (value.is<sol::table>() && setFromTableFunc_) {
            setFromTableFunc_(comp, value);
        }
    }

    std::string GetName() const override {
        return name_;
    }
};

class ComponentRegistry {
private:
    std::unordered_map<std::string, std::unique_ptr<IComponentAdapter>> adapters_;

public:
    template<typename T>
    void Register(const std::string& name,
                  std::function<void(sol::usertype<T>&)> bindFunc = nullptr,
                  std::function<void(T&, sol::object)> setFromTable = nullptr) {
        adapters_[name] = std::make_unique<ComponentAdapter<T>>(name, bindFunc, setFromTable);
    }

    void RegisterAllUserTypes(sol::state& lua) {
        for (auto& [name, adapter] : adapters_) {
            adapter->RegisterUserType(lua);
        }
    }

    IComponentAdapter* Get(const std::string& name) {
        auto it = adapters_.find(name);
        return it != adapters_.end() ? it->second.get() : nullptr;
    }

    bool Has(const std::string& name, World* world, Entity entity) {
        auto* adapter = Get(name);
        return adapter ? adapter->Has(world, entity) : false;
    }

    void Add(const std::string& name, World* world, Entity entity) {
        if (auto* adapter = Get(name)) {
            adapter->Add(world, entity);
        }
    }

    void Remove(const std::string& name, World* world, Entity entity) {
        if (auto* adapter = Get(name)) {
            adapter->Remove(world, entity);
        }
    }

    sol::object GetComponent(const std::string& name, sol::state& lua, World* world, Entity entity) {
        auto* adapter = Get(name);
        return adapter ? adapter->Get(lua, world, entity) : sol::nil;
    }

    void SetComponent(const std::string& name, World* world, Entity entity, sol::object value) {
        if (auto* adapter = Get(name)) {
            adapter->Set(world, entity, value);
        }
    }
};

class ScriptService : public Elysium::Service {
public:
    ScriptService();
    ~ScriptService() override;

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Core execution
    void ExecuteString(const std::string& scriptString);
    
    // Entity Scripting System
    // Returns true if the script was loaded and has an 'update' function
    bool UpdateEntity(Entity entity, const std::string& scriptName, float deltaTime);
    
    // Returns true if the script was loaded and Init was called (or didn't exist)
    bool InitEntity(Entity entity, const std::string& scriptName);

    // Handle events
    void OnEntityEvent(Entity entity, const std::string& scriptName, Event& event);

    // Force reload of a script from disk (hot-reloading)
    void ReloadScript(const std::string& scriptName);

    // Editor Helpers
    void InspectEntityScript(Entity entity);

    sol::state& GetLua() { return lua; }

private:
    sol::state lua;
    
    // Captures the script "Module" or "Class" table.
    // Key: Script Path (e.g. "Scripts/test.lua")
    std::unordered_map<std::string, sol::table> scriptRegistry;

    // Active Instances per Entity
    // Key: Entity ID
    std::unordered_map<Entity, sol::table> entityScriptInstances;

    void InitLuaContext();
    void BindEntityAPI();
    void BindRaylibConstants();
    void BindComponents();
    
    // Loads the script if not already loaded, returns the table
    sol::table GetOrLoadScript(const std::string& scriptName);
    
    // Helper to get or create the instance for an entity
    // If create is true, it will instantiate from the script template
    sol::table GetEntityInstance(Entity entity, const std::string& scriptName, bool create = false);

    ComponentRegistry componentRegistry_;
};

} // namespace Elysium::Services