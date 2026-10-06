#pragma once

#include <algorithm>
#include <optional>
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

// The game screen (UI layers, script and input coordinates) in the framebuffer. The
// configured screen size (ApplicationConfig) is scaled to fit, and the screen then extends
// along whichever axis has room to spare: a wider window gets a wider screen, the same height.
// Scripts ask for its size (GetScreenSize) and anchor to its edges. The identity in the editor.
struct ScreenFit {
    float scale = 1.0f;
    Vector2 offset{0.0f, 0.0f};
    Vector2 layout{0.0f, 0.0f};       // the configured screen size
    Vector2 screen{0.0f, 0.0f};       // the screen's size: at least `layout`, along one axis more
    Vector2 framebuffer{0.0f, 0.0f};  // the framebuffer's

    bool IsIdentity() const {
        return scale == 1.0f && offset.x == 0.0f && offset.y == 0.0f && screen.x == layout.x && screen.y == layout.y;
    }
    Vector2 ToFramebuffer(Vector2 p) const { return {p.x * scale + offset.x, p.y * scale + offset.y}; }
    Vector2 ToScreen(Vector2 p) const { return {(p.x - offset.x) / scale, (p.y - offset.y) / scale}; }
};

// A scene change waiting on its preloads, while the loading scene shows (GetLoadingState).
struct LoadingState {
    std::string scene;  // the scene being opened
    std::string asset;  // a preload still loading; "" before they start
    int loaded = 0;     // preloads in (failed ones count: they're logged, not waited on)
    int total = 0;
    float Progress() const { return total > 0 ? static_cast<float>(loaded) / total : 0.0f; }
};

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

    // The scene shown while a Push/Replace target's <Preload> assets load: the project's
    // Scenes/<name>.xml, else the engine's own (Assets/Scenes/<name>.xml). Empty disables
    // loading screens (the change then happens at once, assets loading in as they come).
    virtual void SetLoadingScene(const std::string& sceneName) = 0;
    virtual const std::string& GetLoadingScene() const = 0;
    // The scene change being loaded for, if any; the loading scene's script reads it.
    virtual std::optional<LoadingState> GetLoadingState() const = 0;

    // Throws away the loaded stack and reloads the same scenes (or the entry scene) from
    // disk, so Play always runs what's saved rather than anything the editor touched.
    virtual void ReloadFromDisk() = 0;

    // Gameplay simulation (systems, scripts, input) runs exactly when the app is in Play mode.
    virtual bool IsPlaying() const = 0;

    // A window position to the framebuffer pixel under it.
    virtual Vector2 ScreenToFramebuffer(Vector2 screenPos) const = 0;

    // How the game screen sits in the framebuffer this frame. Input events and script
    // coordinates are in game screen pixels; the render system maps them through this.
    virtual const ScreenFit& GetScreenFit() const = 0;
};

}  // namespace Elysium::Services
