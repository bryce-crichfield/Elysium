#pragma once

#include "IScene.h"
#include <string>

class Scene : public IScene {
public:
    Scene(const std::string& name);
    virtual ~Scene() = default;
    
    const std::string& GetName() const { return name_; }
    
    virtual void OnUpdate(float deltaTime) override {}
    virtual void OnDraw() override {}
    virtual void OnNetwork(const NetworkEvent& event) override {}
    virtual void OnInput(const InputEvent& event) override {}
    
    virtual void OnEnter() override {}
    virtual void OnExit() override {}

protected:
    std::string name_;
};