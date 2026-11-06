#pragma once

#include "Interfaces/IApplicationService.h"

namespace Elysium {

// What Application drives of the in-engine editor. Not a service: the desktop main hands
// Application one, and builds without the editor (Web) hand it none.
class IEditor {
   public:
    virtual ~IEditor() = default;

    virtual void Initialize(const ApplicationConfig& config) = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void Draw(AppMode mode) = 0;
    virtual void OnModeChanged(AppMode mode) = 0;
    virtual void Shutdown() = 0;
};

}  // namespace Elysium
