#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Message.h"
#include "Core/Scene.h"
#include "Core/Graphics.h"
#include "Core/MathTypes.h"
#include "Interfaces/IService.h"

namespace Elysium::Services {

// SceneEditor used to reach these via `friend class SceneEditor` on the
// concrete SceneService, poking sceneStack_/scenes_ directly. Exposed here
// instead so it (and anything else) can go through the interface only.
struct SceneRegistration {
    std::string name;
    Elysium::Scene* scene = nullptr;
    Elysium::SceneFactory factory;
    std::string xmlPath;
    bool xmlLoaded = false;
};

class ISceneService : public IService {
   public:

    virtual void OnMessage(const Elysium::Message& message) = 0;

    // Stack operations
    virtual void Push(const std::string& sceneName) = 0;
    virtual void Pop() = 0;
    virtual void Replace(const std::string& sceneName) = 0;
    virtual void Clear() = 0;

    // Stack queries
    virtual Elysium::Scene* GetTopScene() const = 0;
    virtual size_t GetStackSize() const = 0;
    virtual bool IsEmpty() const = 0;
    virtual const std::vector<Elysium::Scene*>& GetStack() const = 0;
    virtual const std::unordered_map<std::string, SceneRegistration>& GetSceneRegistry() const = 0;

    // Rendering info
    virtual const Rectangle& GetLetterboxRect() const = 0;
    virtual float GetScaleX() const = 0;
    virtual float GetScaleY() const = 0;

    // The offscreen target scenes are rendered into. A plain GL-shaped handle —
    // callers bind it via RenderContext or sample fb.textureId directly.
    virtual const Framebuffer& GetFramebuffer() const = 0;

    // Blits the framebuffer to the current render target, letterboxed into `target`
    // (window pixels), and records `target` as the viewport rect for input mapping.
    virtual void Present(Rectangle target) = 0;

    virtual void SetViewportRect(Rectangle rect) = 0;
    virtual const Rectangle& GetViewportRect() const = 0;

    virtual bool IsPlaying() const = 0;
    virtual void SetPlaying(bool playing) = 0;

    virtual Vector2 ScreenToFramebuffer(Vector2 screenPos) const = 0;
};

}  // namespace Elysium::Services
