#include "Core/AssetKind.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Elysium {

const std::vector<AssetKindFolder>& AssetKindFolders() {
    static const std::vector<AssetKindFolder> folders = {
        {"Scenes", AssetKind::Scene, {".xml"}},
        {"Prefabs", AssetKind::Prefab, {".xml"}},
        {"Scripts", AssetKind::Script, {".lua"}},
        {"Sounds", AssetKind::Sound, {".wav", ".mp3", ".ogg"}},
        {"Sprites", AssetKind::Sprite, {".xml"}},
        {"Textures", AssetKind::Texture, {".png", ".jpg", ".jpeg"}},
        {"Shaders", AssetKind::Shader, {".fs", ".vs", ".glsl"}},
        {"Models", AssetKind::Model, {".glb", ".gltf", ".obj"}},
    };
    return folders;
}

std::optional<AssetKind> AssetKindOf(const std::string& relativePath) {
    const std::filesystem::path path(relativePath);
    if (path.begin() == path.end()) return std::nullopt;
    const std::string top = path.begin()->generic_string();
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    for (const auto& folder : AssetKindFolders()) {
        if (top != folder.folder) continue;
        const bool matches = std::find(folder.extensions.begin(), folder.extensions.end(), ext) != folder.extensions.end();
        return matches ? std::optional(folder.kind) : std::nullopt;
    }
    return std::nullopt;
}

}  // namespace Elysium
