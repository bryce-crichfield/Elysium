#include "Services/SceneService.h"
#include "Services/LogService.h"
#include "Application.h"
#include "System.h"
#include "Path.h"
#include "Event.h"
#include "imgui.h"
#include "raylib.h"
#include "tinyxml2.h"
#include <typeinfo>
#include "Common.h"

using namespace tinyxml2;

namespace Elysium::Services
{

// =============================================================================
// Constructor
// =============================================================================

SceneService::SceneService()
{
    name_ = "SceneService";
}

void SceneService::Initialize()
{
    Profile;
    auto& app = Application::GetInstance();
    const auto& config = app.GetConfig();

    framebuffer_ = LoadRenderTexture(config.framebufferWidth, config.framebufferHeight);
    CalculateLetterboxing();
}

// =============================================================================
// Stack Operations
// =============================================================================

void SceneService::Push(const std::string& sceneName)
{
    Profile;
    auto it = scenes_.find(sceneName);
    if (it == scenes_.end())
    {
        LOG_ERRORF("SceneService", "Cannot push scene. Scene not found: %s", sceneName.c_str());
        return;
    }

    // Check if scene is already in stack
    Scene* scene = CreateOrGetScene(sceneName);
    if (!scene)
    {
        LOG_ERRORF("SceneService", "Failed to create scene: %s", sceneName.c_str());
        return;
    }

    for (Scene* s : sceneStack_)
    {
        if (s == scene)
        {
            LOG_WARNINGF("SceneService", "Scene '%s' is already in the stack", sceneName.c_str());
            return;
        }
    }

    sceneStack_.push_back(scene);
    EnterScene(scene, sceneName);
    LOG_INFOF("SceneService", "Pushed scene: %s (stack size: %zu)", sceneName.c_str(), sceneStack_.size());
}

void SceneService::Pop()
{
    Profile;
    if (sceneStack_.empty())
    {
        LOG_WARNING("SceneService", "Cannot pop. Scene stack is empty");
        return;
    }

    Scene* scene = sceneStack_.back();
    sceneStack_.pop_back();

    if (scene)
    {
        scene->OnExit();
    }

    LOG_INFOF("SceneService", "Popped scene (stack size: %zu)", sceneStack_.size());
}

void SceneService::Replace(const std::string& sceneName)
{
    Profile;
    if (!sceneStack_.empty())
    {
        Scene* oldScene = sceneStack_.back();
        sceneStack_.pop_back();
        if (oldScene)
        {
            oldScene->OnExit();
        }
    }

    Push(sceneName);
    LOG_INFOF("SceneService", "Replaced top scene with: %s", sceneName.c_str());
}

void SceneService::Clear()
{
    Profile;
    while (!sceneStack_.empty())
    {
        Scene* scene = sceneStack_.back();
        sceneStack_.pop_back();
        if (scene)
        {
            scene->OnExit();
        }
    }
    LOG_INFO("SceneService", "Cleared scene stack");
}

Scene* SceneService::GetTopScene() const
{
    return sceneStack_.empty() ? nullptr : sceneStack_.back();
}

// =============================================================================
// Scene Management
// =============================================================================

void SceneService::RegisterScene(const std::string& name, std::string xmlPath, SceneFactory factory)
{
    SceneRegistration data{name, nullptr, factory, xmlPath, false};
    scenes_.emplace(name, data);
    LOG_INFOF("SceneService", "Registered scene: %s", name.c_str());
}

Scene* SceneService::CreateOrGetScene(const std::string& name)
{
    auto it = scenes_.find(name);
    if (it == scenes_.end())
    {
        LOG_ERRORF("SceneService", "Scene not found: %s", name.c_str());
        return nullptr;
    }

    SceneRegistration& sceneData = it->second;
    if (!sceneData.scene)
    {
        sceneData.scene = sceneData.factory();
    }

    return sceneData.scene;
}

void SceneService::EnterScene(Scene* scene, const std::string& name)
{
    auto it = scenes_.find(name);
    if (it == scenes_.end())
    {
        LOG_ERRORF("SceneService", "Cannot enter scene. Scene not found: %s", name.c_str());
        return;
    }

    SceneRegistration& sceneData = it->second;

    // Load XML if needed
    if (!sceneData.xmlPath.empty() && !sceneData.xmlLoaded)
    {
        LoadScene(*scene, Path(sceneData.xmlPath).GetFullPath());
        sceneData.xmlLoaded = true;
    }

    scene->OnEnter();
}

// =============================================================================
// Update Loop
// =============================================================================

void SceneService::OnMessage(const Message& message)
{
    Profile;

    // Dispatch message to all scenes in the stack (bottom to top)
    for (Scene* scene : sceneStack_)
    {
        scene->OnMessage(const_cast<Message&>(message));
    }
}

void SceneService::Update(float deltaTime)
{
    Profile;

    // Cache deltaTime for systems that need it
    if (deltaTime > 0.0f && deltaTime < 0.1f)
    {
        cachedDeltaTime_ = deltaTime;
    }

    // Process input events (dispatches to scenes top-down)
    ProcessInput();

    // Update ALL scenes in the stack (bottom to top)
    for (Scene* scene : sceneStack_)
    {
        scene->OnUpdate(cachedDeltaTime_);
    }
}

// =============================================================================
// Rendering
// =============================================================================

void SceneService::CalculateLetterboxing()
{
    auto& app = Application::GetInstance();
    const auto& config = app.GetConfig();

    int windowWidth = GetScreenWidth();
    int windowHeight = GetScreenHeight();

    float screenAspect = (float)windowWidth / windowHeight;
    float framebufferAspect = (float)config.framebufferWidth / config.framebufferHeight;

    if (framebufferAspect > screenAspect)
    {
        scaleX_ = scaleY_ = (float)windowWidth / config.framebufferWidth;
        float scaledHeight = config.framebufferHeight * scaleY_;
        offset_.x = 0;
        offset_.y = (windowHeight - scaledHeight) * 0.5f;
        letterboxRect_ = Rectangle{offset_.x, offset_.y, (float)windowWidth, scaledHeight};
    }
    else
    {
        scaleX_ = scaleY_ = (float)windowHeight / config.framebufferHeight;
        float scaledWidth = config.framebufferWidth * scaleX_;
        offset_.x = (windowWidth - scaledWidth) * 0.5f;
        offset_.y = 0;
        letterboxRect_ = Rectangle{offset_.x, offset_.y, scaledWidth, (float)windowHeight};
    }
}

void SceneService::Render()
{
    Profile;
    auto& app = Application::GetInstance();
    const auto& config = app.GetConfig();

    // Recalculate letterboxing if window was resized
    if (IsWindowResized())
    {
        CalculateLetterboxing();
    }

    auto screenRect = Rectangle{0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight};

    // Render all scenes to framebuffer (bottom-to-top)
    BeginTextureMode(framebuffer_);
    ClearBackground(config.backgroundColor);

    for (Scene* scene : sceneStack_)
    {
        if (scene)
        {
            scene->OnDraw(screenRect);
        }
    }

    EndTextureMode();

    // Draw framebuffer to screen with letterboxing
    DrawTexturePro(
        framebuffer_.texture,
        Rectangle{0, 0, (float)framebuffer_.texture.width, -(float)framebuffer_.texture.height},
        letterboxRect_, Vector2{0, 0}, 0.0f, WHITE);
}

// =============================================================================
// Event Handling
// =============================================================================

Vector2 SceneService::ScreenToFramebuffer(Vector2 screenPos) const
{
    // Transform screen coords to framebuffer coords
    float fbX = (screenPos.x - letterboxRect_.x) / scaleX_;
    float fbY = (screenPos.y - letterboxRect_.y) / scaleY_;

    return Vector2{fbX, fbY};
}

void SceneService::ProcessInput()
{
    Profile;
    if (sceneStack_.empty()) return;

    // Check if ImGui wants the mouse - if so, don't send events to scene
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
    {
        return;
    }

    Vector2 mousePos = GetMousePosition();

    // Only process mouse events if mouse is inside the framebuffer
    if (CheckCollisionPointRec(mousePos, letterboxRect_))
    {
        Vector2 fbPos = ScreenToFramebuffer(mousePos);

        // Mouse button events
        for (int button = 0; button < 3; button++)
        {
            if (IsMouseButtonPressed(button))
            {
                MouseButtonPressedEvent event(button, fbPos);
                // Dispatch top-down through stack
                for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it)
                {
                    (*it)->OnEvent(event);
                    if (event.handled) break;
                }
            }
            else if (IsMouseButtonReleased(button))
            {
                MouseButtonReleasedEvent event(button, fbPos);
                for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it)
                {
                    (*it)->OnEvent(event);
                    if (event.handled) break;
                }
            }
        }

        // Mouse wheel
        float wheelMove = GetMouseWheelMove();
        if (wheelMove != 0.0f)
        {
            MouseWheelEvent event(wheelMove, fbPos);
            for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it)
            {
                (*it)->OnEvent(event);
                if (event.handled) break;
            }
        }

        // Mouse move
        static Vector2 lastMousePos = mousePos;
        if (mousePos.x != lastMousePos.x || mousePos.y != lastMousePos.y)
        {
            Vector2 delta = {mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y};
            MouseMovedEvent event(fbPos, delta);
            for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it)
            {
                (*it)->OnEvent(event);
                if (event.handled) break;
            }
            lastMousePos = mousePos;
        }
    }

    // Keyboard events
    const int keysToCheck[] = {
        KEY_SPACE, KEY_ENTER, KEY_ESCAPE,
        KEY_W, KEY_A, KEY_S, KEY_D,
        KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
        KEY_LEFT_SHIFT, KEY_LEFT_CONTROL, KEY_LEFT_ALT
    };

    for (int key : keysToCheck)
    {
        if (IsKeyPressed(key))
        {
            KeyPressedEvent event(key);
            for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it)
            {
                (*it)->OnEvent(event);
                if (event.handled) break;
            }
        }
        else if (IsKeyReleased(key))
        {
            KeyReleasedEvent event(key);
            for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it)
            {
                (*it)->OnEvent(event);
                if (event.handled) break;
            }
        }
    }
}

// =============================================================================
// Debug & Utility
// =============================================================================

void SceneService::ImGui()
{
    Profile;
    inspector_.DrawUI(*this);
}

// =============================================================================
// Shutdown
// =============================================================================

void SceneService::Shutdown()
{
    Profile;
    UnloadRenderTexture(framebuffer_);

    // Exit all scenes in stack
    for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it)
    {
        (*it)->OnExit();
    }
    sceneStack_.clear();

    // Delete all scene instances
    for (auto& [name, data] : scenes_)
    {
        delete data.scene;
    }
    scenes_.clear();
}

} // namespace Elysium::Services
