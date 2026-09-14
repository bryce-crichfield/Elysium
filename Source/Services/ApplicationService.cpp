#include "Services/ApplicationService.h"
#include "Core/Application.h"

namespace Elysium::Services {

ApplicationService::ApplicationService(ServiceLocator&, Application& app)
    : app_(app) {}

const ApplicationConfig& ApplicationService::GetConfig() const {
    return app_.GetConfig();
}

AppMode ApplicationService::GetMode() const {
    return app_.GetMode();
}

void ApplicationService::SetMode(AppMode mode) {
    app_.SetMode(mode);
}

float ApplicationService::GetTime() const {
    return app_.GetTime();
}

void ApplicationService::RequestFontReload() {
    app_.RequestFontReload();
}

bool ApplicationService::ShouldClose() const {
    return app_.ShouldClose();
}

int ApplicationService::GetWindowWidth() const {
    return app_.GetWindowWidth();
}

int ApplicationService::GetWindowHeight() const {
    return app_.GetWindowHeight();
}

}  // namespace Elysium::Services
