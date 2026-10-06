#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Asset.h"
#include "Core/Future.h"
#include "Core/Scene.h"
#include "Core/ServiceLocator.h"
#include "Interfaces/ISceneService.h"

namespace Elysium::Services {

class SceneService : public ISceneService {
public:
    SceneService(ServiceLocator& registry);
    ~SceneService() = default;
    SceneService(const SceneService&) = delete;
    SceneService& operator=(const SceneService&) = delete;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void Render() override;
    void OnMessage(const Message& message) override;

    // Stack operations (queued, applied in Update)
    void Push(const std::string& sceneName) override;
    void Pop() override;
    void Replace(const std::string& sceneName) override;
    void Clear() override;

    // Stack queries
    Scene* GetTopScene() const override;
    size_t GetStackSize() const override { return sceneStack_.size(); }
    bool IsEmpty() const override { return sceneStack_.empty(); }
    const std::vector<Scene*>& GetStack() const override { return sceneStack_; }

    // Play
    bool IsPlaying() const override;
    void SetEntryScene(const std::string& sceneName) override { entryScene_ = sceneName; }
    const std::string& GetEntryScene() const override { return entryScene_; }
    void ReloadFromDisk() override;

    // Loading
    void SetLoadingScene(const std::string& sceneName) override { loadingSceneName_ = sceneName; }
    const std::string& GetLoadingScene() const override { return loadingSceneName_; }
    std::optional<LoadingState> GetLoadingState() const override;

    // Editor
    void SetEditorScene(Scene* scene) override { editorScene_ = scene; }
    Scene* GetEditorScene() const override { return editorScene_; }

    // Framebuffer
    const Framebuffer& GetFramebuffer() const override { return framebuffer_; }
    void SetFramebufferSize(int width, int height) override;
    void Present(Rectangle target) override;
    const Rectangle& GetLetterboxRect() const override { return letterboxRect_; }
    float GetScaleX() const override { return scaleX_; }
    float GetScaleY() const override { return scaleY_; }

    // The viewport rect is where on the window the framebuffer is actually drawn.
    // Application or editor sets this so input coordinates can be translated correctly.
    void SetViewportRect(Rectangle rect) override;
    const Rectangle& GetViewportRect() const override { return viewportRect_; }
    const ScreenFit& GetScreenFit() const override { return fit_; }
    // Converts a raylib-window screen position (e.g. GetMousePosition()) into framebuffer
    // pixel coordinates using the current viewport rect.
    Vector2 ScreenToFramebuffer(Vector2 screenPos) const override;

private:
    enum class SceneOperationType { Push, Pop, Replace, Clear };
    struct SceneOperation {
        SceneOperationType type;
        std::string name;
    };

    // A Push/Replace held back while its target's preloads load, the loading scene
    // (owned, never on the stack) showing instead of the stack meanwhile.
    struct LoadingJob {
        SceneOperation op;
        std::vector<std::string> files;  // to preload, once the loading scene is up
        // The target's scene script, loading so its Preload() can add to `files` first.
        std::string script;
        std::optional<Future<IAsset*>> scriptLoad;
        std::vector<std::pair<std::string, Future<IAsset*>>> loads;  // the preloads, once started
        Scene* scene = nullptr;
        bool started = false;
        float waited = 0.0f;  // seconds before starting them
    };

    // Stack
    void ApplySceneOperations();
    void ApplySceneOperation(const SceneOperation& op);
    // A new scene loaded from ScenePath(name), or null (logged) if it can't be.
    Scene* LoadNamed(const std::string& name);
    // Takes the top scene off the stack and frees it.
    void PopAndFree();
    void ProcessInput();

    // Loading
    // Starts loading for `op` if it opens a scene with preloads not yet loaded; false if not.
    bool StartLoading(const SceneOperation& op);
    // Once every preload is in, drops the loading scene, applies the held operation and
    // returns true; false while still loading.
    bool UpdateLoading();
    void CancelLoading();
    Scene* LoadLoadingScene();
    // Adds what the target's scene script's Preload() names to the files to preload.
    void AddScriptPreloads(LoadingJob& job);

    // Rendering
    void CalculateLetterboxing();

    ServiceLocator& registry_;

    // Stack. Owned: each is deleted when it leaves the stack.
    std::vector<Scene*> sceneStack_;
    std::vector<SceneOperation> pendingOperations_;
    std::string entryScene_;
    // Cached for systems that need it
    float cachedDeltaTime_ = 0.016f;

    // Loading
    std::string loadingSceneName_;
    std::optional<LoadingJob> loading_;

    // Editor: open document shown in the viewport instead of the stack (not owned)
    Scene* editorScene_ = nullptr;

    // Rendering
    Framebuffer framebuffer_;
    int requestedWidth_ = 0;   // editor viewport size, applied in Render()
    int requestedHeight_ = 0;
    Rectangle letterboxRect_;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    Vector2 offset_ = {0, 0};
    // Where on the window the framebuffer is actually drawn
    Rectangle viewportRect_ = {0, 0, 0, 0};
    // Where the game screen sits in the framebuffer
    ScreenFit fit_;
};

}  // namespace Elysium::Services
