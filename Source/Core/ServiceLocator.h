#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Elysium {

class Service;

// Owns every concrete Service, but can only be looked up by interface type —
// a concrete type is never a valid key for Get<T>(), so "ALL HAIL THE SERVICE
// LOCATOR" enforces abstraction-only access by construction, not convention.
// Lives in Core/ (not Services/) so Core, Systems, and Components can depend
// on the locator mechanism itself without depending on any concrete service.
class ServiceLocator {
   public:
    // Registers `service` for lifecycle ownership under TConcrete, and binds
    // it for lookup under every TInterfaces... Get<TInterface>() only ever
    // resolves entries registered this way.
    template <typename TConcrete, typename... TInterfaces>
    TConcrete& Register(std::unique_ptr<TConcrete> service) {
        TConcrete* ptr = service.get();
        (BindInterface<TInterfaces>(ptr), ...);
        owned_[std::type_index(typeid(TConcrete))] = std::move(service);
        return *ptr;
    }

    template <typename TInterface>
    TInterface& Get() {
        auto it = interfaces_.find(std::type_index(typeid(TInterface)));
        if (it == interfaces_.end()) {
            throw std::runtime_error(std::string("ServiceLocator: no service registered for interface ") +
                                      typeid(TInterface).name());
        }
        return *static_cast<TInterface*>(it->second);
    }

    template <typename TInterface>
    bool Has() const {
        return interfaces_.count(std::type_index(typeid(TInterface))) > 0;
    }

    std::vector<Service*> GetAllServices() {
        std::vector<Service*> result;
        result.reserve(owned_.size());
        for (auto& [typeIndex, service] : owned_) {
            result.push_back(service.get());
        }
        return result;
    }

   private:
    template <typename TInterface, typename TConcrete>
    void BindInterface(TConcrete* ptr) {
        interfaces_[std::type_index(typeid(TInterface))] = static_cast<TInterface*>(ptr);
    }

    std::unordered_map<std::type_index, std::unique_ptr<Service>> owned_;
    std::unordered_map<std::type_index, void*> interfaces_;
};

}  // namespace Elysium
