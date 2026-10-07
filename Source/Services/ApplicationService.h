#pragma once

#include "Core/ServiceLocator.h"
#include "Interfaces/IApplicationService.h"

namespace Elysium {
class Application;
}

namespace Elysium::Services {

class ApplicationService : public IApplicationService {
   public:
    ApplicationService(ServiceLocator& registry, Application& app);

    void Initialize() override {}
    void Shutdown() override {}
    void Update(float deltaTime) override {}

    const ApplicationConfig& GetConfig() const override;
    AppMode GetMode() const override;
    void SetMode(AppMode mode) override;
    float GetTime() const override;
    bool ShouldClose() const override;
    void RequestClose() override;
    int GetWindowWidth() const override;
    int GetWindowHeight() const override;

   private:
    Application& app_;
};

}  // namespace Elysium::Services
