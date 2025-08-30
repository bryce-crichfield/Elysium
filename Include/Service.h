#pragma once

#include <string>

#include "imgui.h"
#include <unordered_map>
#include <memory>
#include <vector>
namespace Elysium
{
    class Service
    {
      public:
        virtual ~Service() = default;

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        std::string GetName();
        virtual void Update(float deltaTime) = 0;
        void DebugDraw();
        virtual void OnDebugDraw();
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

    class ServiceRegistry
    {
      public:
        template <typename T>
        void RegisterService(std::unique_ptr<T> service)
        {
            const std::string& name = service->GetName();
            services_[name] = std::move(service);
        }

        template <typename T>
        T& GetService(const std::string& serviceName)
        {
            return dynamic_cast<T&>(*services_.at(serviceName));
        }

        std::vector<Service*> GetAllServices()
        {
            std::vector<Service*> result;
            for (auto& [name, service] : services_)
            {
                result.push_back(service.get());
            }
            return result;
        }

      private:
        std::unordered_map<std::string, std::unique_ptr<Service>> services_;
    };
}
