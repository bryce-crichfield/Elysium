#include "Services/SceneService.h"
#include <algorithm>
#include <filesystem>
#include <typeinfo>
#include "Core/Common.h"
#include "Core/Event.h"
#include "Core/Path.h"
#include "Core/AssetKind.h"
#include "Core/Animation.h"
#include "Core/Audio.h"
#include "Core/Graphics.h"
#include "Core/Prefab.h"
#include "Core/Script.h"
#include "Core/Sprite.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/IScriptService.h"
#include "Services/LogService.h"
#include "Services/ScriptService.h"
#include "Core/System.h"
#include "Core/Framebuffer.h"
#include "Core/Input.h"
#include "raylib.h"
#include "Core/RaylibConvert.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium::Services {

// =============================================================================
// Constructor
// =============================================================================

SceneService::SceneService(ServiceLocator& registry) : registry_(registry) {}

void SceneService::Initialize() {
    Profile;
    const auto& config = registry_.Get<IApplicationService>().GetConfig();

    framebuffer_ = Framebuffer(config.screenWidth, config.screenHeight);
    CalculateLetterboxing();
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
    if (pendingOperations_.empty() || loading_) return;
    ProfileN("SceneService ApplySceneOperations");

    // Swapped out first: a scene entered below may queue operations of its own.
    const std::vector<SceneOperation> operations = std::move(pendingOperations_);
    pendingOperations_.clear();
    for (auto op = operations.begin(); op != operations.end(); ++op) {
        // A scene with preloads opens once they're in (UpdateLoading); the rest wait for it.
        if (StartLoading(*op)) {
            pendingOperations_.insert(pendingOperations_.begin(), op + 1, operations.end());
            return;
        }
        ApplySceneOperation(*op);
    }
}

void SceneService::ApplySceneOperation(const SceneOperation& op) {
    switch (op.type) {
        case SceneOperationType::Push: {
            const bool onStack = std::any_of(sceneStack_.begin(), sceneStack_.end(),
                                             [&](const Scene* s) { return s->GetName() == op.name; });
            if (onStack) {
                LOG_WARNINGF("SceneService", "Scene '%s' is already in the stack", op.name.c_str());
                return;
            }
            Scene* scene = LoadNamed(op.name);
            if (!scene) return;
            sceneStack_.push_back(scene);
            scene->OnEnter();
            LOG_INFOF("SceneService", "Pushed scene: %s (stack size: %zu)", op.name.c_str(), sceneStack_.size());
            return;
        }
        case SceneOperationType::Pop: {
            if (sceneStack_.empty()) {
                LOG_WARNING("SceneService", "Cannot pop. Scene stack is empty");
                return;
            }
            PopAndFree();
            LOG_INFOF("SceneService", "Popped scene (stack size: %zu)", sceneStack_.size());
            return;
        }
        case SceneOperationType::Replace: {
            // Loaded before the old one goes, so a missing scene leaves the stack as it was.
            Scene* scene = LoadNamed(op.name);
            if (!scene) return;
            if (!sceneStack_.empty()) PopAndFree();
            sceneStack_.push_back(scene);
            scene->OnEnter();
            LOG_INFOF("SceneService", "Replaced top scene with: %s", op.name.c_str());
            return;
        }
        case SceneOperationType::Clear: {
            while (!sceneStack_.empty()) PopAndFree();
            LOG_INFO("SceneService", "Cleared scene stack");
            return;
        }
    }
}

// =============================================================================
// Loading
// =============================================================================

namespace {

// Starts an async load of the asset at `relative` (its type from its folder), unless it's
// already loaded or isn't a kind that loads ahead (scenes; shaders, built per material).
std::optional<Future<IAsset*>> Preload(IAssetService& assets, const std::string& relative) {
    const auto kind = AssetKindOf(relative);
    if (!kind) {
        LOG_WARNINGF("SceneService", "Preload: not an asset path: %s", relative.c_str());
        return std::nullopt;
    }
    Path path(relative);
    if (assets.IsAssetLoaded(path)) return std::nullopt;
    switch (*kind) {
        case AssetKind::Texture:   return assets.LoadAsset<Elysium::Texture>(path);
        case AssetKind::Model:     return assets.LoadAsset<Elysium::Model>(path);
        case AssetKind::Font:      return assets.LoadAsset<Elysium::Font>(path);
        case AssetKind::Sound:     return assets.LoadAsset<Elysium::Sound>(path);
        case AssetKind::Sprite:    return assets.LoadAsset<Elysium::Sprite>(path);
        case AssetKind::Animation: return assets.LoadAsset<Elysium::Animation>(path);
        case AssetKind::Prefab:    return assets.LoadAsset<Elysium::Prefab>(path);
        case AssetKind::Script:    return assets.LoadAsset<Elysium::Script>(path);
        default:
            LOG_WARNINGF("SceneService", "Preload: can't preload %s", relative.c_str());
            return std::nullopt;
    }
}

// A preload entry is a file, or a folder standing for every asset file directly in it (not
// its subfolders: name those too, so a big Music/ under Sounds/ isn't swept in by accident).
void ExpandPreload(const std::string& entry, std::vector<std::string>& out) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path full(Path(entry).GetFullPath());
    if (!fs::is_directory(full, ec)) {
        out.push_back(entry);
        return;
    }
    for (fs::directory_iterator it(full, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string relative = (fs::path(entry) / fs::relative(it->path(), full, ec)).generic_string();
        if (AssetKindOf(relative)) out.push_back(std::move(relative));
    }
}

}  // namespace

bool SceneService::StartLoading(const SceneOperation& op) {
    const bool opens = op.type == SceneOperationType::Push || op.type == SceneOperationType::Replace;
    if (!opens || loadingSceneName_.empty() || op.name == loadingSceneName_) return false;
    const std::string path = ScenePath(op.name);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;  // LoadNamed reports it

    const ScenePreloads declared = ReadScenePreloads(path);
    LoadingJob job;
    job.op = op;
    job.script = declared.script;
    for (const auto& entry : declared.assets) ExpandPreload(entry, job.files);

    // The script names its own preloads (Preload()), so it loads first: now if it isn't in.
    auto& assets = registry_.Get<IAssetService>();
    if (!job.script.empty()) {
        if (assets.IsAssetLoaded(Path(job.script))) AddScriptPreloads(job);
        else job.scriptLoad = assets.LoadAsset<Script>(Path(job.script));
    }
    std::erase_if(job.files, [&](const std::string& file) { return assets.IsAssetLoaded(Path(file)); });
    if (job.files.empty() && !job.scriptLoad) return false;  // all in already

    // The preloads start once the loading scene is up and its own assets are in (UpdateLoading),
    // so it shows from the start rather than once its shaders work through the queue.
    job.scene = LoadLoadingScene();
    if (job.scene) job.scene->OnEnter();
    LOG_INFOF("SceneService", "Loading assets for scene: %s", op.name.c_str());
    loading_ = std::move(job);
    return true;
}

void SceneService::AddScriptPreloads(LoadingJob& job) {
    for (const auto& entry : registry_.Get<IScriptService>().GetScenePreloads(Path(job.script))) {
        ExpandPreload(entry, job.files);
    }
}

Scene* SceneService::LoadLoadingScene() {
    std::string path = ScenePath(loadingSceneName_);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        path = Path("Scenes/" + loadingSceneName_ + ".xml", PathRoot::Engine).GetFullPath();
    }
    auto* scene = new Scene(registry_);
    if (!std::filesystem::exists(path, ec) || !LoadScene(*scene, path)) {
        LOG_WARNINGF("SceneService", "No loading scene '%s': loading without one", loadingSceneName_.c_str());
        delete scene;
        return nullptr;
    }
    return scene;
}

bool SceneService::UpdateLoading() {
    if (loading_->scene) loading_->scene->OnUpdate(cachedDeltaTime_, true);

    // The scene script first, for what its Preload() adds.
    if (loading_->scriptLoad) {
        if (!loading_->scriptLoad->IsReady()) return false;
        loading_->scriptLoad.reset();
        AddScriptPreloads(*loading_);
    }

    // The loading scene has drawn once by now (asking for its shaders): the preloads start
    // when those are in, or after a moment regardless.
    constexpr float kMaxWait = 1.0f;
    auto& assets = registry_.Get<IAssetService>();
    if (!loading_->started) {
        loading_->waited += cachedDeltaTime_;
        if (assets.IsLoading() && loading_->waited < kMaxWait) return false;
        for (const auto& file : loading_->files) {
            if (auto future = Preload(assets, file)) loading_->loads.emplace_back(file, std::move(*future));
        }
        loading_->started = true;
    }

    for (const auto& [file, future] : loading_->loads) {
        if (!future.IsReady()) return false;
    }
    const SceneOperation op = loading_->op;
    CancelLoading();
    LOG_INFOF("SceneService", "Loaded scene assets: %s", op.name.c_str());
    ApplySceneOperation(op);
    return true;
}

void SceneService::CancelLoading() {
    if (!loading_) return;
    if (loading_->scene) {
        loading_->scene->OnExit();
        delete loading_->scene;
        // Its scripts set it as Lua's world; don't leave that dangling till the next scene runs.
        registry_.Get<IScriptService>().SetActiveWorld(nullptr);
    }
    loading_.reset();
}

std::optional<LoadingState> SceneService::GetLoadingState() const {
    if (!loading_) return std::nullopt;
    LoadingState state;
    state.scene = loading_->op.name;
    if (!loading_->started) {
        state.total = static_cast<int>(loading_->files.size());
        return state;
    }
    state.total = static_cast<int>(loading_->loads.size());
    for (const auto& [file, future] : loading_->loads) {
        if (future.IsReady()) ++state.loaded;
        else if (state.asset.empty()) state.asset = file;
    }
    return state;
}

Scene* SceneService::GetTopScene() const {
    return sceneStack_.empty() ? nullptr : sceneStack_.back();
}

// =============================================================================
// Scene Management
// =============================================================================

Scene* SceneService::LoadNamed(const std::string& name) {
    ProfileN("SceneService LoadNamed");
    const std::string path = ScenePath(name);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        LOG_ERRORF("SceneService", "Scene not found: %s (%s)", name.c_str(), path.c_str());
        return nullptr;
    }
    auto* scene = new Scene(registry_);
    if (!LoadScene(*scene, path)) {
        LOG_ERRORF("SceneService", "Failed to load scene: %s", name.c_str());
        delete scene;
        return nullptr;
    }
    return scene;
}

void SceneService::PopAndFree() {
    Scene* scene = sceneStack_.back();
    sceneStack_.pop_back();
    scene->OnExit();
    delete scene;
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

bool SceneService::IsPlaying() const {
    return registry_.Get<IApplicationService>().GetMode() == AppMode::Play;
}

void SceneService::ReloadFromDisk() {
    std::vector<std::string> names;
    for (Scene* scene : sceneStack_) names.push_back(GetSceneName(scene));
    if (names.empty() && !entryScene_.empty()) names.push_back(entryScene_);

    // Anything queued (e.g. the startup push while in the editor) is superseded.
    CancelLoading();
    pendingOperations_.clear();
    Clear();  // frees the scenes, so the pushes below load them fresh from their XML
    for (const auto& name : names) Push(name);
}

void SceneService::Update(float deltaTime) {
    Profile;

    if (deltaTime > 0.0f && deltaTime < 0.1f) {
        cachedDeltaTime_ = deltaTime;
    }

    // Editor: the game's stack is frozen (queued changes wait for Play); only the open
    // document ticks, paused, so structural systems keep editor edits applied.
    if (!IsPlaying()) {
        if (editorScene_) editorScene_->OnUpdate(cachedDeltaTime_, false);
        return;
    }

    // While a scene's preloads load, the loading scene runs instead of the stack.
    if (loading_ && !UpdateLoading()) return;

    ApplySceneOperations();
    if (!loading_) {
        ProcessInput();
        ApplySceneOperations();
    }
    if (loading_) return;

    // Only the top scene runs its update loop.
    // Lower scenes are suspended until they become the top again.
    if (Scene* top = GetTopScene()) {
        top->OnUpdate(cachedDeltaTime_, true);
    }
}

// =============================================================================
// Rendering
// =============================================================================

void SceneService::CalculateLetterboxing() {
    auto& app = registry_.Get<IApplicationService>();
    const auto& config = app.GetConfig();

    int windowWidth = std::max(1, app.GetWindowWidth());
    int windowHeight = std::max(1, app.GetWindowHeight());
    const int fbWidth = std::max(1, framebuffer_.Width());
    const int fbHeight = std::max(1, framebuffer_.Height());

    float screenAspect = (float)windowWidth / windowHeight;
    float framebufferAspect = (float)fbWidth / fbHeight;

    if (framebufferAspect > screenAspect) {
        scaleX_ = scaleY_ = (float)windowWidth / fbWidth;
        float scaledHeight = fbHeight * scaleY_;
        offset_.x = 0;
        offset_.y = (windowHeight - scaledHeight) * 0.5f;
        letterboxRect_ = Rectangle{offset_.x, offset_.y, (float)windowWidth, scaledHeight};
    } else {
        scaleX_ = scaleY_ = (float)windowHeight / fbHeight;
        float scaledWidth = fbWidth * scaleX_;
        offset_.x = (windowWidth - scaledWidth) * 0.5f;
        offset_.y = 0;
        letterboxRect_ = Rectangle{offset_.x, offset_.y, scaledWidth, (float)windowHeight};
    }

    // The game screen, fitted into the framebuffer and extended to fill it (the identity in
    // the editor, whose framebuffer is its viewport panel's and lays the screen out in the
    // world instead).
    fit_ = ScreenFit{};
    fit_.layout = fit_.screen = {(float)config.screenWidth, (float)config.screenHeight};
    fit_.framebuffer = {(float)fbWidth, (float)fbHeight};
    if (app.GetMode() == AppMode::Play && config.screenWidth > 0 && config.screenHeight > 0) {
        fit_.scale = std::min((float)fbWidth / config.screenWidth, (float)fbHeight / config.screenHeight);
        fit_.screen = {fbWidth / fit_.scale, fbHeight / fit_.scale};
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

    // The mode can switch at runtime (F1/F2): the editor renders at the viewport panel's
    // size, play at the configured resolution (by default, natively: the window's size).
    int width = app.GetWindowWidth(), height = app.GetWindowHeight();
    if (app.GetMode() == AppMode::Editor && requestedWidth_ > 0 && requestedHeight_ > 0) {
        width = requestedWidth_;
        height = requestedHeight_;
    } else if (app.GetMode() == AppMode::Play) {
        FixedResolution(config.resolution, width, height);
    }
    width = std::max(1, width);
    height = std::max(1, height);
    if (width != framebuffer_.Width() || height != framebuffer_.Height()) {
        framebuffer_ = Framebuffer(width, height);
    }
    // Every frame: the window, the framebuffer or the mode may have changed.
    CalculateLetterboxing();

    auto screenRect = Rectangle{0, 0, (float)framebuffer_.Width(), (float)framebuffer_.Height()};

    // Render all scenes to framebuffer (bottom-to-top)
    ::BeginTextureMode(ToRaylib(framebuffer_));
    // The bottom scene drawn decides what's behind everything.
    Scene* loadingScene = loading_ ? loading_->scene : nullptr;
    Scene* base = app.GetMode() == AppMode::Editor ? editorScene_
                : loadingScene                       ? loadingScene
                : (sceneStack_.empty() ? nullptr : sceneStack_.front());
    Color clearColor = base && base->GetBackgroundColor() ? *base->GetBackgroundColor() : config.backgroundColor;
    ClearBackground(ToRaylib(clearColor));

    if (app.GetMode() == AppMode::Editor) {
        if (editorScene_) editorScene_->OnDraw(screenRect);
    } else if (loading_) {
        if (loadingScene) loadingScene->OnDraw(screenRect);
    } else {
        // A scene still waiting on its script would show as its XML left it (UI the script
        // hides, say), so it isn't drawn until it's set up.
        for (Scene* scene : sceneStack_) {
            if (scene && scene->IsSetUp()) {
                scene->OnDraw(screenRect);
            }
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

void SceneService::SetFramebufferSize(int width, int height) {
    requestedWidth_ = width;
    requestedHeight_ = height;
}

Vector2 SceneService::ScreenToFramebuffer(Vector2 screenPos) const {
    if (viewportRect_.width <= 0.0f || viewportRect_.height <= 0.0f) return {0.0f, 0.0f};

    // Use viewport rect to translate screen coords to framebuffer coords
    float fbX = (screenPos.x - viewportRect_.x) / viewportRect_.width * framebuffer_.Width();
    float fbY = (screenPos.y - viewportRect_.y) / viewportRect_.height * framebuffer_.Height();

    return Vector2{fbX, fbY};
}

void SceneService::ProcessInput() {
    Profile;
    if (sceneStack_.empty())
        return;

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
        // In game screen pixels, as scripts and the UI lay things out.
        Vector2 fbPos = fit_.ToScreen(ScreenToFramebuffer(mousePos));

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
            Vector2 fbLastMousePos = fit_.ToScreen(ScreenToFramebuffer(lastMousePos));
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

    CancelLoading();
    while (!sceneStack_.empty()) PopAndFree();
}

}  // namespace Elysium::Services
