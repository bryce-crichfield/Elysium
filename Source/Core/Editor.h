#pragma once

#include <string>

#include "imgui.h"

#include "Core/Entity.h"

namespace Elysium {

class Application;

template<typename T>
concept Inspectable = requires(T& c, Entity e) {
    { T::Inspect(c, e) } -> std::same_as<void>;
};

class Editor {
   public:
    explicit Editor(const std::string& name) : name_(name) {}
    virtual ~Editor() = default;

    virtual void Initialize() {}
    virtual void Draw(Application& app) = 0;

    bool IsVisible() const { return isVisible_; }
    void SetVisible(bool visible) { isVisible_ = visible; }
    void ToggleVisibility() { isVisible_ = !isVisible_; }
    const std::string& GetName() const { return name_; }

   protected:
    std::string name_;
    bool isVisible_ = false;
};

}  // namespace Elysium
