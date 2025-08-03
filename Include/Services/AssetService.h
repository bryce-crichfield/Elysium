#pragma once

#include "../Asset.h"
#include "raylib.h"
#include <string>

namespace Elysium::Services {

class AssetService {
public:
    void Initialize();
    void Shutdown();
    
    bool GetAsset(const std::string& path, Asset* out);
    Texture2D LoadTexture(const std::string& path);
    Sound LoadSound(const std::string& path);
    void UnloadTexture(const std::string& path);
    void UnloadSound(const std::string& path);
};

} // namespace Elysium::Services