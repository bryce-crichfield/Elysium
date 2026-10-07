#pragma once
#include <string>
#include <vector>

#ifndef ASSETS_PATH
#define ASSETS_PATH "./Assets/"
#endif

namespace Elysium {

// Determines which root directory a Path resolves relative to.
enum class PathRoot {
    Assets,   // Resolves relative to the current project's asset root (read-only game content)
    Engine,   // Resolves relative to the compile-time ASSETS_PATH, regardless of
              // which project is loaded (editor-only resources: fonts, icons, etc.)
};

class Path {
   public:
    Path() = default;
    explicit Path(const std::string& relativePath, PathRoot root = PathRoot::Assets);
    explicit Path(const char* relativePath, PathRoot root = PathRoot::Assets);

    // The inverse of GetFullPath(): a full path (asset root + relative) back to a Path,
    // normalized so different spellings of one file compare equal.
    static Path FromFullPath(const std::string& fullPath);

    std::string GetFullPath() const;
    const std::string& GetRelativePath() const { return relativePath_; }
    PathRoot GetRoot() const { return root_; }

    // Overrides the root that PathRoot::Assets resolves against (defaults to the
    // compile-time ASSETS_PATH). Must end in a trailing '/', and should be set
    // before any Path is resolved — e.g. from the project manifest at startup.
    static void SetAssetsRoot(const std::string& root);
    static const std::string& GetAssetsRoot();

    std::string GetFilename(const std::string& extension = "") const;

    const char* c_str() const;

    bool operator==(const Path& other) const {
        return GetFullPath() == other.GetFullPath();
    }

    std::vector<Path> GetFiles() const;
    std::vector<Path> GetFolders() const;

   private:
    std::string relativePath_;
    PathRoot root_ = PathRoot::Assets;
    mutable std::string cachedFullPath_;
    mutable bool fullPathCached_ = false;

    void UpdateFullPath() const;
};

// Plain full-path (string) helpers.

// Directory portion of a file path, with trailing separator ("" if none).
std::string DirectoryOf(const std::string& path);
// `path` relative to `baseDir`, with forward slashes (falls back to `path` itself).
std::string RelativeTo(const std::string& path, const std::string& baseDir);
// True if two paths name the same file.
bool SamePath(const std::string& a, const std::string& b);

}  // namespace Elysium

namespace std {
template <>
struct hash<Elysium::Path> {
    std::size_t operator()(const Elysium::Path& p) const {
        return std::hash<std::string>()(p.GetFullPath());
    }
};
}  // namespace std
