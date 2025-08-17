#pragma once

#include "System.h"

namespace Elysium::Systems {
    class CooldownSystem : public System{
    public:
        CooldownSystem(Context context) : System(context) {}

        void Update(float deltaTime) override;
        void Render() override {}
        void OnDraw() {}
        void OnEvent(const char* eventName, void* data) override {}
    };
};
