#pragma once
#include "System.h"
namespace Elysium::Systems
{
class MovementSystem  : public System
{
public:
    MovementSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;
};
} // namespace Elysium::Systems
