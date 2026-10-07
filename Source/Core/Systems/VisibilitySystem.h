#pragma once
#include "Core/System.h"

namespace Elysium::Systems {
    class VisibilitySystem : public System {
    public:
        using System::System;
        void Update(float deltaTime) override;
    };
}
