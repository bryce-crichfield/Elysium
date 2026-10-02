#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Message.h"
#include "Core/Scene.h"
#include "Core/Graphics.h"
#include "Core/Math/MathTypes.h"
#include "Core/Framebuffer.h"
#include "Interfaces/IService.h"

namespace Elysium::Services {

class ISceneService : public IService {
   public:

    virtual void OnMessage(const Elysium::Message& message) = 0;

    // Stack operations. A scene is named by its file: Push("Town") loads Scenes/Town.xml
    // (ScenePath), fresh each time it goes on the stack.
    virtual void Push(const std::string& sceneName) = 0;
    virtual void Pop() = 0;
    virtual void Replace(const std::string& sceneName) = 0;
    virtual void Clear() = 0;

    // Stack queries
    virtual Elysium::Scene* GetTopScene() const = 0;
    virtual size_t GetStackSize() const = 0;
    virtual bool IsEmpty() const = 0;
    virtual const std::vector<Elysium::Scene*>& GetStack() const = 0;

    bool IsInStack(const Elysium::Scene* scene) const {
        const auto& stack = GetStack();
        return scene && std::find(stack.begin(), stack.end(), scene) != stack.end();
    }

    // `scene`'s name, or "Unknown".
    std::string GetSceneName(const Elysium::Scene* scene) const {
        return scene && !scene->GetName().empty() ? scene->GetName() : "Unknown";
    }

    // Rendering info
    virtual const Rectangle& GetLetterboxRect() const = 0;
    virtual float GetScaleX() const = 0;
    virtual float GetScaleY() const = 0;

    // The offscreen target scenes are rendered into. Callers bind it via RenderContext
    // or sample fb.TextureId() directly (e.g. ImGui::Image). In play mode it is the fixed
    // game resolution, letterboxed onto the window; in the editor it matches the viewport
    // panel pixel for pixel (see SetFramebufferSize).
    virtual const Framebuffer& GetFramebuffer() const = 0;

    // Editor only: the size the viewport panel shows the scene at. The framebuffer is
    // resized to it at the start of the next Render(), so the displayed image is never
    // one being rendered to. Ignored in play mode.
    virtual void SetFramebufferSize(int width, int height) = 0;

    // Blits the framebuffer to the current render target, letterboxed into `target`
    // (window pixels), and records `target` as the viewport rect for input mapping.
    virtual void Present(Rectangle target) = 0;

    virtual void SetViewportRect(Rectangle rect) = 0;
    virtual const Rectangle& GetViewportRect() const = 0;

    // Editor only: the open editor document's scene (never on the stack) to render and tick,
    // paused. In editor mode the stack itself is frozen and not drawn.
    virtual void SetEditorScene(Elysium::Scene* scene) = 0;
    virtual Elysium::Scene* GetEditorScene() const = 0;

    // The project's entry scene, pushed at startup and when entering Play with an empty stack.
    virtual void SetEntryScene(const std::string& sceneName) = 0;
    virtual const std::string& GetEntryScene() const = 0;

    // Throws away the loaded stack and reloads the same scenes (or the entry scene) from
    // disk, so Play always runs what's saved rather than anything the editor touched.
    virtual void ReloadFromDisk() = 0;

    // Gameplay simulation (systems, scripts, input) runs exactly when the app is in Play mode.
    virtual bool IsPlaying() const = 0;

    virtual Vector2 ScreenToFramebuffer(Vector2 screenPos) const = 0;
};

}  // namespace Elysium::Services
