#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Core/AssetKind.h"

namespace Elysium {

// Drag-drop payload type for an asset dragged out of the Assets panel: its project-relative
// path, null-terminated.
constexpr const char* kAssetDragPayload = "ASSET_PATH";

// Starts dragging the file at `relativePath` if the last item is being dragged. Call right
// after the item that represents it.
void AssetDragSource(AssetKind kind, const std::string& relativePath);

// Inside a BeginDragDropTarget: the project-relative path of a `kind` file dropped from the
// Assets panel on the frame it's dropped. Other kinds aren't accepted (no highlight).
std::optional<std::string> AcceptAssetDrop(AssetKind kind);

// Every `kind` file in the project, project-relative and sorted. Rescanned every few seconds.
const std::vector<std::string>& DiscoverAssets(AssetKind kind);

// The one widget for a reference to an asset: framed in the kind's color with its icon,
// a searchable dropdown of every file of that kind (plus None), and a drop target for files
// dragged from the Assets panel, which only takes the right kind. A path that isn't a file of
// that kind shows as missing. `filter` narrows the choices. Returns true when `path` changed.
bool AssetField(const char* id, AssetKind kind, std::string& path,
                const std::function<bool(const std::string&)>& filter = nullptr);

// An inspector row: PropertyLabel then AssetField.
bool AssetFieldRow(const char* label, AssetKind kind, std::string& path,
                   const std::function<bool(const std::string&)>& filter = nullptr);

}  // namespace Elysium
