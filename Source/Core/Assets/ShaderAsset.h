#pragma once

#include <string>
#include "Core/Asset.h"
#include "Core/Path.h"
#include "Core/Shader.h"

namespace Elysium {

// Key path of the SDF shader composed from Geometry/<geometry>.glsl and
// Material/<material>.glsl (engine Assets/Shaders/Sdf). Load it like any shader:
// LoadAsset<Shader>(ComposedShaderPath(...)) / Get<Shader>(...). No file exists at the
// path — ShaderAsset::Load recognises the .sdf extension and assembles the source.
Path ComposedShaderPath(const std::string& geometry, const std::string& material);
bool IsComposedShaderPath(const Path& path);

// Loads a GLSL fragment shader from disk. Two-phase on purpose: Load() runs on a worker
// thread and only reads text, while Finalize() runs on the main thread where a GL context
// exists and compiles (reflecting the uniform list).
//
// The asset's Path points at the fragment shader. An optional vertex shader is taken from
// the sibling file with the same stem and a .vs extension; without one raylib's default
// vertex shader is used, which is what a 2D post-process effect wants. A path ending in
// .sdf is a composition key instead — see ComposedShaderPath.
class ShaderAsset : public AssetBase<ShaderAsset> {
   public:
    using AssetBase::AssetBase;

    bool Load() override;
    bool Finalize() override;
    bool NeedsFinalize() const override { return true; }
    void Unload() override;

    Shader& GetData() { return shader_; }

   private:
    std::string vertexSource_;
    std::string fragmentSource_;
    Shader shader_;
};

}  // namespace Elysium
