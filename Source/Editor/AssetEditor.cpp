#include "AssetEditor.h"
#include "Core/Application.h"
#include "Core/Path.h"
#include "Core/Prefab.h"
#include "Core/Xml.h"
#include "Core/World.h"
#include "Components/PrefabInstanceComponent.h"
#include "Services/AssetService.h"
#include "Services/EditorService.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"
#include "Services/TaskService.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <set>

namespace Elysium {

namespace fs = std::filesystem;
using namespace Services;

AssetEditor::AssetEditor() : Editor("Asset Browser") {}

namespace {
std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Heuristic: prefab files live under a "Prefabs/" directory, same convention the design
// doc's migration used for Projects/DemoGame/Scenes/Prefabs/*.xml.
bool IsPrefabPath(const std::string& relativePath) {
    return relativePath.find("Prefabs/") != std::string::npos && ToLower(relativePath).ends_with(".xml");
}

// Builds a fresh, unused instance id from the prefab's own filename (e.g.
// "Prefabs/Unit.xml" -> "Unit"), disambiguating against instance ids already present
// in `world` so repeated placements of the same prefab don't collide.
std::string GenerateInstanceId(World* world, const std::string& relativePath) {
    std::string base = relativePath;
    size_t slash = base.find_last_of('/');
    if (slash != std::string::npos) base = base.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    if (base.empty()) base = "Instance";

    std::set<std::string> existingIds;
    world->Query<PrefabInstanceComponent>([&](Entity, PrefabInstanceComponent& pic) {
        existingIds.insert(pic.instanceId);
    });

    if (!existingIds.count(base)) return base;
    int suffix = 1;
    std::string candidate;
    do {
        candidate = base + "_" + std::to_string(suffix++);
    } while (existingIds.count(candidate));
    return candidate;
}

// Instantiates the prefab at `relativePath` (asset-root-relative) into the active
// scene's World, mirroring the runtime PrefabInstance load path, then selects the
// spawned entities so the user can immediately start overriding fields in WorldEditor.
void InstantiatePrefabIntoScene(Application& app, const std::string& relativePath) {
    auto& sceneService = app.GetService<SceneService>();
    Scene* scene = sceneService.GetTopScene();
    if (!scene) {
        LOG_WARNING("AssetEditor", "No active scene to instantiate a prefab into.");
        return;
    }

    std::string scenePath = sceneService.GetScenePath(scene);
    std::string sceneBase = GetBasePath(scenePath);
    const std::string& assetsRoot = Path::GetAssetsRoot();

    // Both scenePath (an on-disk path) and relativePath (asset-root-relative) may or may
    // not share the assets root as a literal prefix depending on how each was resolved;
    // best-effort strip it so `src` comes out relative to the scene's own directory,
    // matching how <PrefabInstance src="..."> is resolved on load/save.
    std::string src = relativePath;
    std::string sceneBaseRelative = sceneBase;
    if (sceneBaseRelative.rfind(assetsRoot, 0) == 0) {
        sceneBaseRelative = sceneBaseRelative.substr(assetsRoot.size());
    }
    if (!sceneBaseRelative.empty() && relativePath.rfind(sceneBaseRelative, 0) == 0) {
        src = relativePath.substr(sceneBaseRelative.size());
    }

    std::string fullPath = Path(relativePath).GetFullPath();
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* entitiesRoot = nullptr;
    if (!LoadPrefabFile(fullPath, doc, entitiesRoot)) {
        return;
    }

    World* world = scene->GetWorld();
    std::string instanceId = GenerateInstanceId(world, relativePath);
    PrefabIdMap idToEntity = LoadPrefabEntities(entitiesRoot, world, instanceId);

    auto& editorService = app.GetService<EditorService>();
    editorService.ClearSelection();

    for (const auto& [localId, entity] : idToEntity) {
        PrefabInstanceComponent pic;
        pic.src = src;
        pic.instanceId = instanceId;
        pic.localEntityId = localId;
        world->AddComponent<PrefabInstanceComponent>(entity, pic);
        editorService.SelectEntity(entity, true);
    }
}

DiskCache ScanDiskCache(fs::path rootPath) {
    DiskCache cache;
    for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
        std::string parentPath = entry.path().parent_path().generic_string();

        DiskFile file;
        file.path = entry.path();
        file.relativePath = fs::relative(entry.path(), rootPath).generic_string();
        file.isDirectory = entry.is_directory();

        cache[parentPath].push_back(file);
    }

    // Folders first, then files, alphabetically (case-insensitive) within each group.
    for (auto& [parentPath, files] : cache) {
        std::sort(files.begin(), files.end(), [](const DiskFile& a, const DiskFile& b) {
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
            return ToLower(a.path.filename().string()) < ToLower(b.path.filename().string());
        });
    }

    return cache;
}
}  // namespace

void AssetEditor::Draw(Application& app) {
    auto& assetService = app.GetService<AssetService>();

    // Discovery & Polling — rooted at the current project's asset root, not the
    // engine's own Assets/ (which holds editor-only resources loaded via
    // PathRoot::Engine and shouldn't show up as browsable project content).
    if (rootPath_.empty()) {
        const std::string& assetsRoot = Path::GetAssetsRoot();
        if (fs::exists(assetsRoot)) rootPath_ = fs::canonical(assetsRoot);
    }

    if (rootPath_.empty()) return;

    // Scan runs on TaskService's worker thread — a synchronous recursive scan here
    // can stall the whole app for a frame or more (e.g. on a OneDrive-synced folder),
    // which reads as periodic freezes during unrelated interactions like gizmo dragging.
    if (!refreshInFlight_ && app.GetTime() - lastRefreshTime_ > refreshInterval_) {
        refreshInFlight_ = true;
        lastRefreshTime_ = app.GetTime();

        auto& taskService = app.GetService<TaskService>();
        fs::path rootPath = rootPath_;
        taskService.Submit<DiskCache>(std::function<DiskCache()>([rootPath]() {
                          return ScanDiskCache(rootPath);
                      }))
            .Then([this](const DiskCache& cache) {
                directoryCache_ = cache;
                refreshInFlight_ = false;
            });
    }

    if (ImGui::Begin(name_.c_str())) {
        RenderTreeRecursive(rootPath_, app);
    }
    ImGui::End();
}

void AssetEditor::RenderTreeRecursive(const fs::path& currentPath, Application& app) {
    auto& assetService = app.GetService<AssetService>();
    
    std::string pathKey = currentPath.generic_string();
    if (directoryCache_.find(pathKey) == directoryCache_.end()) return;

    for (const auto& file : directoryCache_[pathKey]) {
        // --- FIX: PUSH UNIQUE ID ---
        // This prevents the "id != 0" assert by ensuring every item has a unique path hash
        ImGui::PushID(file.relativePath.c_str());

        std::string name = file.path.filename().string();
        
        if (file.isDirectory) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
            if (ImGui::TreeNodeEx(name.c_str(), flags)) {
                RenderTreeRecursive(file.path, app);
                ImGui::TreePop();
            }
        } else {
            bool isPrefab = IsPrefabPath(file.relativePath);

            // Optimization: AssetService has pathToName_, use that if you can
            // otherwise we'll stick to the scan for now.
            const Asset* activeAsset = nullptr;
            for (const auto& [assetName, asset] : assetService.GetAllAssets()) {
                // Compare relative paths (GetPath returns full path, use GetRelativePath instead)
                if (asset.GetPath().GetRelativePath() == file.relativePath) {
                    activeAsset = &asset;
                    break;
                }
            }

            bool isLoaded = (activeAsset != nullptr && activeAsset->IsLoaded());

            // Apply status coloring — prefabs get their own color since "loaded" isn't a
            // meaningful concept for them (they're re-read from disk on demand).
            if (isPrefab) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.75f, 1.0f, 1.0f));
            else if (isLoaded) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
            else ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

            bool isSelected = (selectedFile_ == file.relativePath);
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                selectedFile_ = file.relativePath;
            }

            ImGui::PopStyleColor();

            // Right Click Context Menu
            if (ImGui::BeginPopupContextItem("AssetCtx")) {
                if (isPrefab) {
                    if (ImGui::MenuItem("Edit Prefab")) {
                        app.GetService<EditorService>().OpenPrefabForEditing(file.relativePath);
                    }
                    if (ImGui::MenuItem("Instantiate Prefab")) {
                        InstantiatePrefabIntoScene(app, file.relativePath);
                    }
                } else if (!isLoaded) {
                    if (ImGui::MenuItem("Load into Elysium")) {
                        AssetType type = AssetType::TEXTURE;
                        std::string ext = file.path.extension().string();
                        if (ext == ".wav") type = AssetType::SOUND;
                        else if (ext == ".xml") type = AssetType::SPRITE;
                        else if (ext == ".lua") type = AssetType::SCRIPT;

                        assetService.LoadAsset(type, Path(file.relativePath));
                    }
                } else {
                    if (ImGui::MenuItem("Reload Asset")) {
                        assetService.ReloadAsset(activeAsset->GetType(), activeAsset->GetPath());
                    }
                    if (ImGui::MenuItem("Unload Asset")) {
                        assetService.GetAsset(activeAsset->GetPath())->Unload();
                    }
                }
                ImGui::EndPopup();
            }
        }

        // --- FIX: POP UNIQUE ID ---
        ImGui::PopID();
    }
}

} // namespace Elysium