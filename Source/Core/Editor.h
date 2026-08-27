#pragma once

#include <string>

#include "imgui.h"

#include "Core/Entity.h"
#include "Core/ServiceLocator.h"

namespace Elysium {

struct ApplicationConfig;

template<typename T>
concept Inspectable = requires(T& c, Entity e, ServiceLocator& services) {
    { T::Inspect(c, e, services) } -> std::same_as<void>;
};

class Editor {
   public:
    Editor(ServiceLocator& services, const std::string& name) : services_(services), name_(name) {}
    virtual ~Editor() = default;

    virtual void Initialize(const ApplicationConfig& config) {}
    virtual void Draw() = 0;

    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }
    void ToggleVisibility() { isVisible_ = !isVisible_; }
    const std::string& GetName() const { return name_; }

   protected:
    ServiceLocator& services_;
    std::string name_;
    bool isVisible_ = false;
};

}  // namespace Elysium
