#pragma once

#include "Interfaces/IApplicationService.h"
#include "Service.h"

namespace Elysium {
class Application;
}

namespace Elysium::Services {

// Thin forwarding wrapper around the one real Application instance — see
// IApplicationService for why this exists instead of a raw Application*.
class ApplicationService : public Elysium::Service, public IApplicationService {
   public:
    ApplicationService(ServiceLocator& registry, Application& app);

    void Initialize() override {}
    void Shutdown() override {}
    void Update(float deltaTime) override {}

    const ApplicationConfig& GetConfig() const override;
    AppMode GetMode() const override;
    void SetMode(AppMode mode) override;
    float GetTime() const override;
    void RequestFontReload() override;
    bool ShouldClose() const override;

   private:
    Application& app_;
};

}  // namespace Elysium::Services
