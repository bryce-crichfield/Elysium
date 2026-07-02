#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Message.h"
#include "Core/Serial.h"
#include "Core/Scene.h"
#include "Services/InvokeService.h"
#include "Service.h"
#include "raylib.h"

namespace Elysium {
class SceneEditor;
}

namespace Elysium::Services {

enum class SceneChangeOp : uint8_t { Push, Pop, Replace, Clear };

struct SceneChangeRequestMessage {
    SceneChangeOp op = SceneChangeOp::Push;
    std::string sceneName;

    static void Write(const SceneChangeRequestMessage& self, SerialBuffer& buffer) {
        buffer.WriteU8(static_cast<uint8_t>(self.op));
        buffer.WriteString(self.sceneName);
    }
    static void Read(SceneChangeRequestMessage& self, SerialBuffer& buffer) {
        self.op = static_cast<SceneChangeOp>(buffer.ReadU8());
        self.sceneName = buffer.ReadString();
    }
};

struct SceneChangeResponseMessage {
    bool success = false;

    static void Write(const SceneChangeResponseMessage& self, SerialBuffer& buffer) {
        buffer.WriteU8(self.success ? 1 : 0);
    }
    static void Read(SceneChangeResponseMessage& self, SerialBuffer& buffer) {
        self.success = buffer.ReadU8() != 0;
    }
};

class SceneChangedMessage : public Message {
public:
    SceneChangedMessage(SceneChangeOp op, const std::string& sceneName)
        : op(op), sceneName(sceneName) {}
    SceneChangeOp op;
    std::string sceneName;
};

struct SceneChange {};

template <>
struct InvokeMethod<SceneChange> {
    using Request = SceneChangeRequestMessage;
    using Response = SceneChangeResponseMessage;
    static constexpr InvokeMethodId Id = 0x01;
};

struct SceneRegistration {
    std::string name;
    Scene* scene = nullptr;
    SceneFactory factory;
    std::string xmlPath;
    bool xmlLoaded = false;
};

class SceneService : public Elysium::Service {
public:
    friend class Elysium::SceneEditor;

    SceneService(ServiceRegistry& registry);
    ~SceneService() = default;
    SceneService(const SceneService&) = delete;
    SceneService& operator=(const SceneService&) = delete;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void OnMessage(const Message& message);
    void Update(float deltaTime) override;
    void Render() override;
    
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

    // Rendering info getters
    const Rectangle& GetLetterboxRect() const { return letterboxRect_; }
    float GetScaleX() const { return scaleX_; }
    float GetScaleY() const { return scaleY_; }
    RenderTexture2D& GetFramebuffer() { return framebuffer_; }

    // The viewport rect is where on the window the framebuffer is actually drawn.
    // Application or editor sets this so input coordinates can be translated correctly.
    void SetViewportRect(Rectangle rect);
    const Rectangle& GetViewportRect() const { return viewportRect_; }

private:
    void ProcessInput();
    Scene* CreateOrGetScene(const std::string& name);
    void EnterScene(Scene* scene, const std::string& name);
    void ApplySceneOperations();

    enum class SceneOperationType { Push, Pop, Replace, Clear };
    struct SceneOperation {
        SceneOperationType type;
        std::string name;
    };
    std::vector<SceneOperation> pendingOperations_;

    // Helper to convert screen coords to framebuffer coords
    Vector2 ScreenToFramebuffer(Vector2 screenPos) const;

    // Rendering helpers
    void CalculateLetterboxing();

    // Scene registry and stack
    std::unordered_map<std::string, SceneRegistration> scenes_;
    std::vector<Scene*> sceneStack_;

    // Rendering
    RenderTexture2D framebuffer_;
    Rectangle letterboxRect_;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    Vector2 offset_ = {0, 0};

    // Where on the window the framebuffer is actually drawn
    Rectangle viewportRect_ = {0, 0, 0, 0};

    // Cached for systems that need it
    float cachedDeltaTime_ = 0.016f;

    // Editor pause flag — stops scene updates and input processing
    bool paused_ = false;
};

}  // namespace Elysium::Services
