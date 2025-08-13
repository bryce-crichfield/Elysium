#pragma once
#include "System.h"
namespace Elysium::Systems
{
class MovementSystem  : public System
{
public:
    MovementSystem(Context context);

    void Update(float deltaTime) override;
    void Render() override {}
    void OnDraw() {}
    void OnEvent(const char* eventName, void* data) override {}
};
} // namespace Elysium::Systems
