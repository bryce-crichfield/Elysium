#pragma once
#include "Core/System.h"

namespace Elysium::Systems {
    class FollowSystem : public System {
    public:
        using System::System;
        void Update(float deltaTime) override;
    };
}
