#pragma once

#include "Core/Application.h"

namespace Elysium::Services {

// Uniform access to Application-owned state (window mode, config, clock) — the
// one exception to "everything is a real service": there is exactly one
// Application, so this just forwards to it. Exists so Systems/Components/
// Editors never need a raw Application* alongside their ServiceLocator.
class IApplicationService {
   public:
    virtual ~IApplicationService() = default;

    virtual const ApplicationConfig& GetConfig() const = 0;
    virtual AppMode GetMode() const = 0;
    virtual void SetMode(AppMode mode) = 0;
    virtual float GetTime() const = 0;
    virtual void RequestFontReload() = 0;
    virtual bool ShouldClose() const = 0;
};

}  // namespace Elysium::Services
