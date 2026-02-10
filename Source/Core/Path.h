#pragma once
#include <string>

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
    const char* c_str() const;

    bool operator==(const Path& other) const {
        return GetFullPath() == other.GetFullPath();
    }

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
