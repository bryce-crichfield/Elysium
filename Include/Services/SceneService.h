#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include "Scene.h"
#include "Service.h"
#include "SceneInspector.h"
#include "raylib.h"

namespace Elysium::Services {

struct SceneRegistration {
    std::string name;
    Scene* scene = nullptr;
    SceneFactory factory;
    std::string xmlPath;
    bool xmlLoaded = false;
};

class SceneService : public Elysium::Service {
public:
    friend class Elysium::SceneInspector;

    SceneService();
    ~SceneService() = default;
    SceneService(const SceneService&) = delete;
    SceneService& operator=(const SceneService&) = delete;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void Render() override;
    void ImGui() override;

    // Scene registration
    void RegisterScene(const std::string& name, std::string xmlPath, SceneFactory factory);

    // Stack operations
    void Push(const std::string& sceneName);
    void Pop();
    void Replace(const std::string& sceneName);
    void Clear();

    // Stack queries
    Scene* GetTopScene() const;
    size_t GetStackSize() const { return sceneStack_.size(); }
    bool IsEmpty() const { return sceneStack_.empty(); }
    const std::vector<Scene*>& GetStack() const { return sceneStack_; }

    // Legacy compatibility - returns top scene
    Scene* GetScene() const { return GetTopScene(); }

    // Rendering info getters
    const Rectangle& GetLetterboxRect() const { return letterboxRect_; }
    float GetScaleX() const { return scaleX_; }
    float GetScaleY() const { return scaleY_; }

private:
    void ProcessInput();
    Scene* CreateOrGetScene(const std::string& name);
    void EnterScene(Scene* scene, const std::string& name);

    // Helper to convert screen coords to framebuffer coords
    Vector2 ScreenToFramebuffer(Vector2 screenPos) const;

    // Rendering helpers
    void CalculateLetterboxing();

    // Components
    Elysium::SceneInspector inspector_;

    // Scene registry and stack
    std::unordered_map<std::string, SceneRegistration> scenes_;
    std::vector<Scene*> sceneStack_;

    // Rendering
    RenderTexture2D framebuffer_;
    Rectangle letterboxRect_;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    Vector2 offset_ = {0, 0};

    // Cached for systems that need it
    float cachedDeltaTime_ = 0.016f;
};

} // namespace Elysium::Services
