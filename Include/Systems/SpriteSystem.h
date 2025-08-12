#pragma once

#include "System.h"

namespace Elysium::Systems {

class SpriteSystem : public System {
public:
    SpriteSystem(Context context);
    
    void Update(float deltaTime) override;
    void Render() override;
    void OnEvent(const char* eventName, void* data) override;
};

} // namespace Elysium::Systems