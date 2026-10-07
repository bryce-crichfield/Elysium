#pragma once

#include "System.h"

namespace Elysium::Systems {

class ScriptSystem : public System {
public:
    ScriptSystem(Context context);
    void Update(float deltaTime) override;
    void OnEvent(Event& event) override;
};

} // namespace Elysium::Systems
