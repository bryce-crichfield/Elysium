#include "Services/SceneService.h"
#include <typeinfo>
#include "Core/Common.h"
#include "Core/Event.h"
#include "Core/Path.h"
#include "Editor/Theme.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IInvokeService.h"
#include "Interfaces/IMessageService.h"
#include "Interfaces/IScriptService.h"
#include "Services/InvokeService.h"
#include "Services/LogService.h"
#include "Services/MessageService.h"
#include "Services/ScriptService.h"
#include "Core/System.h"
#include "Core/Framebuffer.h"
#include "Core/Input.h"
#include "imgui.h"
#include "raylib.h"
#include "Core/RaylibConvert.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium::Services {

// Release a scene's memory and reset its registration so it reloads fresh next push.
static void FreeScene(SceneRegistration& data) {
    delete data.scene;
    data.scene = nullptr;
    data.xmlLoaded = false;
}

// =============================================================================
// Constructor
// =============================================================================

SceneService::SceneService(ServiceLocator& registry) : registry_(registry) {}

void SceneService::Initialize() {
    Profile;
    const auto& config = registry_.Get<IApplicationService>().GetConfig();

    // Load scenes from Scenes folder
    Path scenesDir("Scenes");
    auto sceneFiles = scenesDir.GetFiles();
    for (const auto& file : sceneFiles) {
        std::string filename = file.GetFilename(".xml");
        if (!filename.empty()) {
            std::string sceneName = filename.substr(0, filename.find_last_of('.'));
            std::string xmlPath = file.GetRelativePath();
            SceneFactory factory = [](ServiceLocator& services) { return new Scene(services); };
            SceneRegistration data{sceneName, nullptr, factory, xmlPath, false};
            scenes_.emplace(sceneName, data);
            LOG_INFOF("SceneService", "Registered scene: %s", sceneName.c_str());
        }
    }

    framebuffer_ = Framebuffer(config.framebufferWidth, config.framebufferHeight);
    CalculateLetterboxing();

    auto& invokeService = registry_.Get<IInvokeService>();
    invokeService.Register<SceneChange>(
        std::function<SceneChangeResponseMessage(NetworkPeer, const SceneChangeRequestMessage&)>(
        [this](NetworkPeer, const SceneChangeRequestMessage& req) -> SceneChangeResponseMessage {
            SceneChangeResponseMessage resp;
            switch (req.op) {
                case SceneChangeOp::Push:
                    Push(req.sceneName);
                    resp.success = true;
                    break;
                case SceneChangeOp::Pop:
                    Pop();
                    resp.success = true;
                    break;
                case SceneChangeOp::Replace:
                    Replace(req.sceneName);
                    resp.success = true;
                    break;
                case SceneChangeOp::Clear:
                    Clear();
                    resp.success = true;
                    break;
                default:
                    resp.success = false;
                    break;
            }
            return resp;
        }));
}

// =============================================================================
// Stack Operations
// =============================================================================

void SceneService::Push(const std::string& sceneName) {
    pendingOperations_.push_back({SceneOperationType::Push, sceneName});
}

void SceneService::Pop() {
    pendingOperations_.push_back({SceneOperationType::Pop, ""});
}

void SceneService::Replace(const std::string& sceneName) {
    pendingOperations_.push_back({SceneOperationType::Replace, sceneName});
}

void SceneService::Clear() {
    pendingOperations_.push_back({SceneOperationType::Clear, ""});
}

void SceneService::ApplySceneOperations() {
    if (pendingOperations_.empty()) return;
    ProfileN("SceneService ApplySceneOperations");

    // Process all pending operations
    for (const auto& op : pendingOperations_) {
        switch (op.type) {
            case SceneOperationType::Push: {
                auto it = scenes_.find(op.name);
                if (it == scenes_.end()) {
                    LOG_ERRORF("SceneService", "Cannot push scene. Scene not found: %s", op.name.c_str());
                    continue;
                }

                Scene* scene = CreateOrGetScene(op.name);
                if (!scene) {
                    LOG_ERRORF("SceneService", "Failed to create scene: %s", op.name.c_str());
                    continue;
                }

                // Check duplicates
                bool found = false;
                for (Scene* s : sceneStack_) {
                    if (s == scene) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    LOG_WARNINGF("SceneService", "Scene '%s' is already in the stack", op.name.c_str());
                    continue;
                }

                sceneStack_.push_back(scene);
                EnterScene(scene, op.name);
                LOG_INFOF("SceneService", "Pushed scene: %s (stack size: %zu)", op.name.c_str(), sceneStack_.size());

                auto& messageService = registry_.Get<IMessageService>();
                messageService.Post<SceneChangedMessage>(SceneChangeOp::Push, op.name);
                break;
            }
            case SceneOperationType::Pop: {
                if (sceneStack_.empty()) {
                    LOG_WARNING("SceneService", "Cannot pop. Scene stack is empty");
                    continue;
                }

                Scene* scene = sceneStack_.back();
                sceneStack_.pop_back();

                if (scene) {
                    scene->OnExit();
                    for (auto& [name, data] : scenes_) {
                        if (data.scene == scene) {
                            FreeScene(data);
                            break;
                        }
                    }
                }

                LOG_INFOF("SceneService", "Popped scene (stack size: %zu)", sceneStack_.size());

                auto& messageService = registry_.Get<IMessageService>();
                messageService.Post<SceneChangedMessage>(SceneChangeOp::Pop, "");
                break;
            }
            case SceneOperationType::Replace: {
                if (!sceneStack_.empty()) {
                    Scene* oldScene = sceneStack_.back();
                    sceneStack_.pop_back();
                    if (oldScene) {
                        oldScene->OnExit();
                        for (auto& [name, data] : scenes_) {
                            if (data.scene == oldScene) {
                                FreeScene(data);
                                break;
                            }
                        }
                    }
                }

                auto it = scenes_.find(op.name);
                if (it == scenes_.end()) {
                    LOG_ERRORF("SceneService", "Cannot replace with scene. Scene not found: %s", op.name.c_str());
                    continue;
                }

                Scene* scene = CreateOrGetScene(op.name);
                if (!scene) {
                    LOG_ERRORF("SceneService", "Failed to create scene: %s", op.name.c_str());
                    continue;
                }

                sceneStack_.push_back(scene);
                EnterScene(scene, op.name);
                LOG_INFOF("SceneService", "Replaced top scene with: %s", op.name.c_str());

                auto& messageService = registry_.Get<IMessageService>();
                messageService.Post<SceneChangedMessage>(SceneChangeOp::Replace, op.name);
                break;
            }
            case SceneOperationType::Clear: {
                while (!sceneStack_.empty()) {
                    Scene* scene = sceneStack_.back();
                    sceneStack_.pop_back();
                    if (scene) {
                        scene->OnExit();
                        for (auto& [name, data] : scenes_) {
                            if (data.scene == scene) {
                                FreeScene(data);
                                break;
                            }
                        }
                    }
                }
                LOG_INFO("SceneService", "Cleared scene stack");

                auto& messageService = registry_.Get<IMessageService>();
                messageService.Post<SceneChangedMessage>(SceneChangeOp::Clear, "");
                break;
            }
        }
    }
    pendingOperations_.clear();
}

Scene* SceneService::GetTopScene() const {
    return sceneStack_.empty() ? nullptr : sceneStack_.back();
}

// =============================================================================
// Scene Management
// =============================================================================
Scene* SceneService::CreateOrGetScene(const std::string& name) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        LOG_ERRORF("SceneService", "Scene not found: %s", name.c_str());
        return nullptr;
    }

    SceneRegistration& sceneData = it->second;
    if (!sceneData.scene) {
        sceneData.scene = sceneData.factory(registry_);
    }

    return sceneData.scene;
}

void SceneService::EnterScene(Scene* scene, const std::string& name) {
    ProfileN("SceneService EnterScene");
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        LOG_ERRORF("SceneService", "Cannot enter scene. Scene not found: %s", name.c_str());
        return;
    }

    SceneRegistration& sceneData = it->second;

    if (!sceneData.xmlPath.empty() && !sceneData.xmlLoaded) {
        LoadScene(*scene, sceneData.xmlPath);
        sceneData.xmlLoaded = true;
    }

    scene->OnEnter();
}

// =============================================================================
// Update Loop
// =============================================================================

void SceneService::OnMessage(const Message& message) {
    Profile;

    // Dispatch message to all scenes in the stack (bottom to top)
    for (Scene* scene : sceneStack_) {
        scene->OnMessage(const_cast<Message&>(message));
    }
}

void SceneService::Update(float deltaTime) {
    Profile;

    ApplySceneOperations();

    bool isPlaying = !paused_;

    // Gameplay input/scripting stays paused with the sim; structural systems
    // (TransformSystem) keep running below so editor edits still take effect.
    if (isPlaying) {
        if (deltaTime > 0.0f && deltaTime < 0.1f) {
            cachedDeltaTime_ = deltaTime;
        }

        ProcessInput();

        if (!pendingOperations_.empty()) {
            ApplySceneOperations();
        }
    }

    // Only the top scene runs its update loop.
    // Lower scenes are suspended until they become the top again.
    if (Scene* top = GetTopScene()) {
        top->OnUpdate(cachedDeltaTime_, isPlaying);
    }
}

// =============================================================================
// Rendering
// =============================================================================

void SceneService::CalculateLetterboxing() {
    auto& app = registry_.Get<IApplicationService>();
    const auto& config = app.GetConfig();

    int windowWidth = app.GetWindowWidth();
    int windowHeight = app.GetWindowHeight();

    float screenAspect = (float)windowWidth / windowHeight;
    float framebufferAspect = (float)config.framebufferWidth / config.framebufferHeight;

    if (framebufferAspect > screenAspect) {
        scaleX_ = scaleY_ = (float)windowWidth / config.framebufferWidth;
        float scaledHeight = config.framebufferHeight * scaleY_;
        offset_.x = 0;
        offset_.y = (windowHeight - scaledHeight) * 0.5f;
        letterboxRect_ = Rectangle{offset_.x, offset_.y, (float)windowWidth, scaledHeight};
    } else {
        scaleX_ = scaleY_ = (float)windowHeight / config.framebufferHeight;
        float scaledWidth = config.framebufferWidth * scaleX_;
        offset_.x = (windowWidth - scaledWidth) * 0.5f;
        offset_.y = 0;
        letterboxRect_ = Rectangle{offset_.x, offset_.y, scaledWidth, (float)windowHeight};
    }

    // In play mode the viewport matches the letterbox
    if (app.GetMode() == AppMode::Play) {
        viewportRect_ = letterboxRect_;
    }
}

// Renders to the framebuffer, but it's up to the caller to blit it to the screen
void SceneService::Render() {
    Profile;
    auto& app = registry_.Get<IApplicationService>();
    const auto& config = app.GetConfig();

    // Recalculate letterboxing if window was resized
    if (IsWindowResized()) {
        CalculateLetterboxing();
    }

    auto screenRect = Rectangle{0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight};

    // Render all scenes to framebuffer (bottom-to-top)
    ::BeginTextureMode(ToRaylib(framebuffer_));
    // The editor viewport looks at the world through the editor camera, so the empty space
    // around the scene is editor chrome and follows the theme.
    Color clearColor = config.backgroundColor;
    if (app.GetMode() == AppMode::Editor) {
        const ImVec4 bg = EditorStyle::CurrentPalette().ViewportBackground;
        clearColor = Color{(unsigned char)(bg.x * 255), (unsigned char)(bg.y * 255), (unsigned char)(bg.z * 255), 255};
    }
    ClearBackground(ToRaylib(clearColor));

    for (Scene* scene : sceneStack_) {
        if (scene) {
            scene->OnDraw(screenRect);
        }
    }

    ::EndTextureMode();

    // In editor mode the ImGui "Viewport" panel blits the framebuffer (ViewportEditor);
    // in play mode Application drives Present() after all services have rendered.
}

// Blits framebuffer_ to the currently-bound target, letterboxed into `target`.
void SceneService::Present(Rectangle target) {
    ::Rectangle src{0, 0, (float)framebuffer_.Width(), -(float)framebuffer_.Height()};
    ::DrawTexturePro(ToRaylibColorTexture(framebuffer_), src, ToRaylib(target),
                     ::Vector2{0, 0}, 0.0f, ToRaylib(Colors::White));
    viewportRect_ = target;
}

// =============================================================================
// Event Handling
// =============================================================================

void SceneService::SetViewportRect(Rectangle rect) {
    viewportRect_ = rect;
}

Vector2 SceneService::ScreenToFramebuffer(Vector2 screenPos) const {
    const auto& config = registry_.Get<IApplicationService>().GetConfig();

    // Use viewport rect to translate screen coords to framebuffer coords
    float fbX = (screenPos.x - viewportRect_.x) / viewportRect_.width * config.framebufferWidth;
    float fbY = (screenPos.y - viewportRect_.y) / viewportRect_.height * config.framebufferHeight;

    return Vector2{fbX, fbY};
}

void SceneService::ProcessInput() {
    Profile;
    if (sceneStack_.empty())
        return;

    // Check if ImGui wants the mouse - if so, don't send events to scene
    /*ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }*/

    Vector2 mousePos = Input::GetMousePosition();
    bool isInside = viewportRect_.Contains(mousePos);

    static bool wasInside = false;
    if (isInside && !wasInside) {
        MouseEnterEvent event;
        for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
            (*it)->OnEvent(event);
            if (event.handled) break;
        }
    } else if (!isInside && wasInside) {
        MouseExitEvent event;
        for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
            (*it)->OnEvent(event);
            if (event.handled) break;
        }
    }
    wasInside = isInside;

    // Only process mouse events if mouse is inside the framebuffer
    if (isInside) {
        Vector2 fbPos = ScreenToFramebuffer(mousePos);

        // Mouse button events
        for (int button = 0; button < 3; button++) {
            MouseButton mouseButton = static_cast<MouseButton>(button);
            if (Input::IsMouseButtonPressed(mouseButton)) {
                MouseButtonPressedEvent event(button, fbPos);
                // Dispatch top-down through stack
                for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
                    (*it)->OnEvent(event);
                    if (event.handled)
                        break;
                }
            } else if (Input::IsMouseButtonReleased(mouseButton)) {
                MouseButtonReleasedEvent event(button, fbPos);
                for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
                    (*it)->OnEvent(event);
                    if (event.handled)
                        break;
                }
            }
        }

        // Mouse wheel
        float wheelMove = Input::GetMouseWheelMove();
        if (wheelMove != 0.0f) {
            MouseWheelEvent event(wheelMove, fbPos);
            for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
                (*it)->OnEvent(event);
                if (event.handled)
                    break;
            }
        }

        // Mouse move
        static Vector2 lastMousePos = mousePos;
        if (mousePos.x != lastMousePos.x || mousePos.y != lastMousePos.y) {
            Vector2 fbLastMousePos = ScreenToFramebuffer(lastMousePos);
            Vector2 delta = {fbPos.x - fbLastMousePos.x, fbPos.y - fbLastMousePos.y};
            MouseMovedEvent event(fbPos, delta);
            for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
                (*it)->OnEvent(event);
                if (event.handled)
                    break;
            }
            lastMousePos = mousePos;

            // The Script Service cache's mouse position for GetMousePosition
            auto& scriptService = registry_.Get<IScriptService>();
            scriptService.SetMousePosition(fbPos.x, fbPos.y);
        }
    }

    // Keyboard events
    const Key keysToCheck[] = {
        Key::One, Key::Two, Key::Three, Key::Four, Key::Five, Key::Six, Key::Seven, Key::Eight, Key::Nine, Key::Zero,
        Key::Space, Key::Enter, Key::Escape,
        Key::W, Key::A, Key::S, Key::D, Key::I,
        Key::Up, Key::Down, Key::Left, Key::Right,
        Key::LeftShift, Key::LeftControl, Key::LeftAlt};

    for (Key key : keysToCheck) {
        int keyCode = static_cast<int>(key);
        if (Input::IsKeyPressed(key)) {
            KeyPressedEvent event(keyCode);
            for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
                (*it)->OnEvent(event);
                if (event.handled)
                    break;
            }
        } else if (Input::IsKeyReleased(key)) {
            KeyReleasedEvent event(keyCode);
            for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
                (*it)->OnEvent(event);
                if (event.handled)
                    break;
            }
        }
    }
}

void SceneService::Shutdown() {
    Profile;
    framebuffer_ = Framebuffer();

    // Free any scenes still on the stack
    while (!sceneStack_.empty()) {
        Scene* scene = sceneStack_.back();
        sceneStack_.pop_back();
        if (scene) {
            scene->OnExit();
            for (auto& [name, data] : scenes_) {
                if (data.scene == scene) {
                    FreeScene(data);
                    break;
                }
            }
        }
    }


    // Any remaining allocated scenes not in the stack (shouldn't happen, but clean up)
    for (auto& [name, data] : scenes_) {
        delete data.scene;
        data.scene = nullptr;
    }
    scenes_.clear();
}

}  // namespace Elysium::Services
