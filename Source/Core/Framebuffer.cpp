#include "Core/Framebuffer.h"

#include "raylib.h"

namespace Elysium {

Framebuffer::Framebuffer(int width, int height) {
    ::RenderTexture2D rt = ::LoadRenderTexture(width, height);
    id_ = rt.id;
    textureId_ = rt.texture.id;
    depthBufferId_ = rt.depth.id;
    width_ = rt.texture.width;
    height_ = rt.texture.height;
}

Framebuffer::~Framebuffer() { Destroy(); }

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : id_(other.id_), textureId_(other.textureId_), depthBufferId_(other.depthBufferId_),
      width_(other.width_), height_(other.height_) {
    other.id_ = 0;
    other.textureId_ = 0;
    other.depthBufferId_ = 0;
    other.width_ = 0;
    other.height_ = 0;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        Destroy();
        id_ = other.id_;
        textureId_ = other.textureId_;
        depthBufferId_ = other.depthBufferId_;
        width_ = other.width_;
        height_ = other.height_;
        other.id_ = 0;
        other.textureId_ = 0;
        other.depthBufferId_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

void Framebuffer::Resize(int width, int height) {
    if (id_ != 0 && width_ == width && height_ == height) return;
    *this = Framebuffer(width, height);
}

void Framebuffer::Destroy() {
    if (id_ != 0) {
        // UnloadRenderTexture only needs .id (+ .texture.id); rlUnloadFramebuffer
        // queries and deletes the attachments from the FBO itself.
        ::RenderTexture2D rt{};
        rt.id = id_;
        rt.texture.id = textureId_;
        rt.texture.width = width_;
        rt.texture.height = height_;
        rt.depth.id = depthBufferId_;
        ::UnloadRenderTexture(rt);
    }
    id_ = 0;
    textureId_ = 0;
    depthBufferId_ = 0;
    width_ = 0;
    height_ = 0;
}

}  // namespace Elysium
