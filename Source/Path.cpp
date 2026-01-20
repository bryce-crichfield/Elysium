#include "Path.h"

namespace Elysium {

Path::Path(const std::string& relativePath) : relativePath_(relativePath) {
}

Path::Path(const char* relativePath) : relativePath_(relativePath) {
}

void Path::UpdateFullPath() const {
    if (!fullPathCached_) {
        cachedFullPath_ = std::string(ASSETS_PATH) + relativePath_;
        fullPathCached_ = true;
    }
}

std::string Path::GetFullPath() const {
    UpdateFullPath();
    return cachedFullPath_;
}

const char* Path::c_str() const {
    UpdateFullPath();
    return cachedFullPath_.c_str();
}

}  // namespace Elysium