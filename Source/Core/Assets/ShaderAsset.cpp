#include "Core/Assets/ShaderAsset.h"

#include <fstream>
#include <sstream>
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

namespace {

bool ReadTextFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

// "Shaders/FireBorder.fs" -> "Shaders/FireBorder.vs"
std::string SiblingVertexPath(const std::string& fragmentPath) {
    size_t dot = fragmentPath.find_last_of('.');
    if (dot == std::string::npos) return fragmentPath + ".vs";
    return fragmentPath.substr(0, dot) + ".vs";
}

// "<dir>/RoundedRect+Stroke.sdf": the fragment source is assembled from GLSL chunks in
// <dir> (Prelude.glsl, Geometry/RoundedRect.glsl, Material/Stroke.glsl, Main.glsl)
// instead of read from a file. Nothing exists on disk at the key path itself.
bool ComposeSdfSource(const std::string& keyPath, std::string& out) {
    size_t slash = keyPath.find_last_of("/\\");
    std::string dir = slash == std::string::npos ? "" : keyPath.substr(0, slash + 1);
    std::string stem = keyPath.substr(dir.size());
    stem = stem.substr(0, stem.size() - 4);  // strip ".sdf"

    size_t plus = stem.find('+');
    if (plus == std::string::npos) {
        LOG_ERRORF("ShaderAsset", "Malformed SDF shader key (want Geometry+Material.sdf): %s", keyPath.c_str());
        return false;
    }

    const std::string chunks[] = {
        dir + "Prelude.glsl",
        dir + "Geometry/" + stem.substr(0, plus) + ".glsl",
        dir + "Material/" + stem.substr(plus + 1) + ".glsl",
        dir + "Main.glsl",
    };

    out.clear();
    for (const std::string& chunk : chunks) {
        std::string text;
        if (!ReadTextFile(chunk, text)) {
            LOG_ERRORF("ShaderAsset", "Missing SDF shader chunk: %s", chunk.c_str());
            return false;
        }
        out += text;
        out += '\n';
    }
    return true;
}

bool EndsWith(const std::string& text, const char* suffix) {
    size_t n = std::char_traits<char>::length(suffix);
    return text.size() >= n && text.compare(text.size() - n, n, suffix) == 0;
}

}  // namespace

Path ComposedShaderPath(const std::string& geometry, const std::string& material) {
    return Path("Shaders/Sdf/" + geometry + "+" + material + ".sdf", PathRoot::Engine);
}

bool IsComposedShaderPath(const Path& path) { return EndsWith(path.GetRelativePath(), ".sdf"); }

bool ShaderAsset::Load() {
    const std::string fragmentPath = GetPath().GetFullPath();
    if (IsComposedShaderPath(GetPath())) {
        vertexSource_.clear();
        return ComposeSdfSource(fragmentPath, fragmentSource_);
    }
    if (!ReadTextFile(fragmentPath, fragmentSource_)) {
        LOG_ERRORF("ShaderAsset", "Failed to read shader source: %s", fragmentPath.c_str());
        return false;
    }
    // Optional — most 2D effects are fragment-only.
    ReadTextFile(SiblingVertexPath(fragmentPath), vertexSource_);
    return true;
}

bool ShaderAsset::Finalize() {
    // WARNING: compiles and links synchronously, then reads the status back, which stalls the
    // main thread until the driver finishes (tens of ms per shader; bursts cause long frames while
    // loading). Fix: start the compile here, report "pending", and check the status on a later
    // frame (GL_KHR_parallel_shader_compile where available).
    std::string error;
    Shader compiled = Shader::FromSource(vertexSource_, fragmentSource_, &error);
    if (!compiled.IsValid()) {
        LOG_ERRORF("ShaderAsset", "Failed to compile shader '%s': %s", GetPath().c_str(), error.c_str());
        return false;
    }

    shader_ = std::move(compiled);
    SetLoaded(true);

    // Free the source text now that it's on the GPU; a reload re-reads from disk anyway.
    vertexSource_.clear();
    vertexSource_.shrink_to_fit();
    fragmentSource_.clear();
    fragmentSource_.shrink_to_fit();

    LOG_DEBUGF("ShaderAsset", "Compiled shader '%s' (id %u) with %d uniform(s)", GetPath().c_str(),
               shader_.Id(), (int)shader_.GetUniforms().size());
    return true;
}

void ShaderAsset::Unload() {
    shader_ = Shader{};  // move-assign an empty Shader; the old program is released by its dtor
    SetLoaded(false);
}

REGISTER_ASSET_TYPE(Shader, ShaderAsset);

}  // namespace Elysium
