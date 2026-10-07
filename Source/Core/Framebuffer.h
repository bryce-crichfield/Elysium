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

    // Samples the color texture bilinearly (true) or nearest (false, the default).
    void SetLinearFilter(bool linear);

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

// A multisampled (MSAA) color + depth target, for antialiased 3D. It can't be sampled: draw
// into it, then Resolve it into an ordinary Framebuffer. Move-only, like Framebuffer.
class MultisampleFramebuffer {
   public:
    MultisampleFramebuffer() = default;
    // `depthFormat`: a GL depth internal format, matching the Framebuffer whose depth it
    // resolves into (a depth blit needs the same format); 0 for 24-bit.
    MultisampleFramebuffer(int width, int height, int samples, unsigned int depthFormat = 0);
    ~MultisampleFramebuffer();

    MultisampleFramebuffer(const MultisampleFramebuffer&) = delete;
    MultisampleFramebuffer& operator=(const MultisampleFramebuffer&) = delete;
    MultisampleFramebuffer(MultisampleFramebuffer&& other) noexcept;
    MultisampleFramebuffer& operator=(MultisampleFramebuffer&& other) noexcept;

    bool IsValid() const { return id_ != 0; }
    int Width() const { return width_; }
    int Height() const { return height_; }
    int Samples() const { return samples_; }

    // Recreates it at this size and sample count (clamped to what the GPU allows), with the
    // depth format of `depthOf`'s depth buffer. No-op if nothing changed.
    void Ensure(int width, int height, int samples, const Framebuffer& depthOf);

    // Binds it as the render target and clears it: color to transparent, depth to far.
    void Begin();

    // Averages its samples into `color`'s color buffer, and copies its depth into `depth`'s
    // (either may be null). Leaves `depth`, or else `color`, bound as the render target.
    void Resolve(const Framebuffer* color, const Framebuffer* depth);

   private:
    void Destroy();

    unsigned int id_ = 0;
    unsigned int colorBufferId_ = 0;
    unsigned int depthBufferId_ = 0;
    unsigned int depthFormat_ = 0;
    int width_ = 0;
    int height_ = 0;
    int samples_ = 0;
};

}  // namespace Elysium
