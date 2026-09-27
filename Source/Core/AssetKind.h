#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Elysium {

// The kinds of asset a project holds. The project layout is by convention: each kind has
// one folder at the project root, and only files with that kind's extensions inside it
// are assets of that kind.
enum class AssetKind { Folder, Scene, Prefab, Script, Sound, Sprite, Texture, Shader };

struct AssetKindFolder {
    const char* folder;  // at the project root
    AssetKind kind;
    std::vector<std::string> extensions;  // lower case, with the dot
};

// Every kind's folder, in the order the editor lists them.
const std::vector<AssetKindFolder>& AssetKindFolders();

// The kind of the file at `relativePath` (to the project root) by the convention, or none
// if it isn't in a kind's folder or doesn't have one of its extensions.
std::optional<AssetKind> AssetKindOf(const std::string& relativePath);

}  // namespace Elysium
