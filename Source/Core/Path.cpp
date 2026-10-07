#include "Core/Path.h"
#include <filesystem>
#include <string>
#include <vector>

namespace Elysium {

namespace {
std::string& AssetsRootMutable() {
    static std::string root = ASSETS_PATH;
    return root;
}
}  // namespace

void Path::SetAssetsRoot(const std::string& root) {
    AssetsRootMutable() = root;
}

const std::string& Path::GetAssetsRoot() {
    return AssetsRootMutable();
}

Path::Path(const std::string& relativePath, PathRoot root)
    : relativePath_(relativePath), root_(root) {
}

Path::Path(const char* relativePath, PathRoot root)
    : relativePath_(relativePath), root_(root) {
}

std::string Path::GetFilename(const std::string& extension) const {
    if (extension.empty()) {
        return relativePath_.substr(relativePath_.find_last_of('\\') + 1);
    } else {
        size_t pos = relativePath_.find_last_of('.');
        if (pos != std::string::npos) {
            std::string ext = relativePath_.substr(pos);
            if (ext == extension) {
                return relativePath_.substr(relativePath_.find_last_of('\\') + 1);
            }
        }
    }
    return "";
}

std::vector<Path> Path::GetFiles() const {
    std::vector<Path> files;
    std::string fullPath = GetFullPath();
    for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
        if (entry.is_regular_file()) {
            files.emplace_back(entry.path().string());
        }
    }
    return files;
}

std::vector<Path> Path::GetFolders() const {
    std::vector<Path> folders;
    std::string fullPath = GetFullPath();
    for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
        if (entry.is_directory()) {
            folders.emplace_back(entry.path().string());
        }
    }
    return folders;
}

void Path::UpdateFullPath() const {
    if (!fullPathCached_) {
        std::string base;
        switch (root_) {
            case PathRoot::Engine: base = ASSETS_PATH; break;
            default:               base = AssetsRootMutable(); break;
        }
        cachedFullPath_ = base + relativePath_;
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

Path Path::FromFullPath(const std::string& fullPath) {
    const std::string& root = AssetsRootMutable();
    std::string relative = RelativeTo(fullPath, root);
    if (relative == fullPath && fullPath.rfind(root, 0) == 0) relative = fullPath.substr(root.size());
    return Path(std::filesystem::path(relative).lexically_normal().generic_string());
}

std::string DirectoryOf(const std::string& path) {
    auto sep = path.find_last_of("/\\");
    return sep == std::string::npos ? "" : path.substr(0, sep + 1);
}

std::string RelativeTo(const std::string& path, const std::string& baseDir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path relative = fs::relative(fs::path(path), fs::path(baseDir.empty() ? "." : baseDir), ec);
    return ec || relative.empty() ? path : relative.generic_string();
}

bool SamePath(const std::string& a, const std::string& b) {
    std::error_code ea, eb;
    auto ca = std::filesystem::weakly_canonical(a, ea), cb = std::filesystem::weakly_canonical(b, eb);
    return !ea && !eb ? ca == cb : a == b;
}

}  // namespace Elysium
