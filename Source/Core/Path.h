#pragma once
#include <string>
#include <vector>

#ifndef ASSETS_PATH
#define ASSETS_PATH "./Assets/"
#endif

namespace Elysium {

class Path {
   public:
    Path() = default;
    explicit Path(const std::string& relativePath);
    explicit Path(const char* relativePath);

    std::string GetFullPath() const;
    const std::string& GetRelativePath() const { return relativePath_; }
   
    std::string GetFilename(const std::string& extension = "") const;

    const char* c_str() const;

    bool operator==(const Path& other) const {
        return GetFullPath() == other.GetFullPath();
    }

    std::vector<Path> GetFiles() const;
    std::vector<Path> GetFolders() const;

   private:
    std::string relativePath_;
    mutable std::string cachedFullPath_;
    mutable bool fullPathCached_ = false;

    void UpdateFullPath() const;
};
}  // namespace Elysium

namespace std {
template <>
struct hash<Elysium::Path> {
    std::size_t operator()(const Elysium::Path& p) const {
        return std::hash<std::string>()(p.GetFullPath());
    }
};
}  // namespace std
