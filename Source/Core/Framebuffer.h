#pragma once

// Offscreen render-target lifecycle. RAII wrapper around a backend framebuffer object —
// a raylib RenderTexture today, a glGenFramebuffers handle later — implemented in
// Framebuffer.cpp so raylib.h stays out of every header. Move-only: exactly one owner
// of the backend handle at a time.

namespace Elysium {

class Framebuffer {
   public:
    Framebuffer() = default;
    // hdr: a 16-bit float color buffer, for values past 1 (emission, light).
    Framebuffer(int width, int height, bool hdr = false);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    bool IsValid() const { return id_ != 0; }

    unsigned int Id() const { return id_; }
    unsigned int TextureId() const { return textureId_; }
    unsigned int DepthBufferId() const { return depthBufferId_; }
    int Width() const { return width_; }
    int Height() const { return height_; }

    // Builds the color texture's mip chain from its current contents and samples it
    // trilinearly, for shaders that read it blurred (textureLod).
    void GenerateMipmaps();

    // Destroys and recreates the backend object at the new size. No-op if already that size.
    void Resize(int width, int height);
    bool IsHdr() const { return hdr_; }

   private:
    void Destroy();

    unsigned int id_ = 0;
    unsigned int textureId_ = 0;
    unsigned int depthBufferId_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool hdr_ = false;
};

}  // namespace Elysium
