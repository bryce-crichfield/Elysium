#pragma once

#include "System.h"
#include "raylib.h"
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include "Entity.h"

namespace Elysium::Systems {

class AnimationSystem : public System
{
public:
    AnimationSystem(Context context);

    void Update(float deltaTime) override;
    void Render() override;
    void OnDraw();
    void OnEvent(const char* eventName, void* data) override;

private:
    void DrawActionDetails(const Action& action, const std::string& id);
    bool ProcessAction(World& world, Entity entity, std::shared_ptr<Action> action, float dt);
    std::shared_ptr<Action> CopyAction(const std::shared_ptr<Action>& action);

    bool showDebugWindow_ = false;
};
}
