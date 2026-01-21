#pragma once

#include <string>
#include <typeindex>

#include <memory>
#include <unordered_map>
#include <vector>
#include "imgui.h"
namespace Elysium {
class Service {
   public:
    virtual ~Service() = default;

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    std::string GetName();
    virtual void Update(float deltaTime) = 0;
    virtual void Render() {}
    void DebugDraw();
    virtual void ImGui();
    void ToggleVisibility();

   protected:
    std::string name_ = "UndefinedService";
    bool isVisible_ = false;
    bool hasUi_ = true;
};

struct Task {
    virtual ~Task() = default;
    virtual void Execute() = 0;
    virtual std::string GetDescription() const = 0;
};

class ServiceRegistry {
   public:
    template <typename T>
    void RegisterService(std::unique_ptr<T> service) {
        auto typeIndex = std::type_index(typeid(T));
        services_[typeIndex] = std::move(service);
    }

    template <typename T>
    T& GetService() {
        auto typeIndex = std::type_index(typeid(T));
        return dynamic_cast<T&>(*services_.at(typeIndex));
    }

    std::vector<Service*> GetAllServices() {
        std::vector<Service*> result;
        for (auto& [typeIndex, service] : services_) {
            result.push_back(service.get());
        }
        return result;
    }

   private:
    std::unordered_map<std::type_index, std::unique_ptr<Service>> services_;
};
}  // namespace Elysium
