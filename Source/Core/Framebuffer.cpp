#include "Core/Framebuffer.h"

#include "raylib.h"
#include "rlgl.h"

// raylib's rlGenTextureMipmaps logs every call, and lit layers rebuild their emission
// mips every frame; call GL directly (GLFW is linked into raylib on desktop).
extern "C" void* glfwGetProcAddress(const char* name);

namespace Elysium {

Framebuffer::Framebuffer(int width, int height, bool hdr) : hdr_(hdr) {
    if (hdr) {
        // LoadRenderTexture is RGBA8 only; the same object, with a half-float color buffer.
        id_ = rlLoadFramebuffer();
        if (id_ == 0) return;
        rlEnableFramebuffer(id_);
        textureId_ = rlLoadTexture(nullptr, width, height, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
        depthBufferId_ = rlLoadTextureDepth(width, height, true);
        rlFramebufferAttach(id_, textureId_, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(id_, depthBufferId_, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
        rlFramebufferComplete(id_);
        rlDisableFramebuffer();
        width_ = width;
        height_ = height;
        return;
    }
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
      width_(other.width_), height_(other.height_), hdr_(other.hdr_) {
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
        hdr_ = other.hdr_;
        other.id_ = 0;
        other.textureId_ = 0;
        other.depthBufferId_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

void Framebuffer::GenerateMipmaps() {
    if (textureId_ == 0) return;
    using GenerateMipmapFn = void (*)(unsigned int target);
    static const auto generateMipmap = reinterpret_cast<GenerateMipmapFn>(glfwGetProcAddress("glGenerateMipmap"));
    if (!generateMipmap) return;
    constexpr unsigned int kTexture2D = 0x0DE1;  // GL_TEXTURE_2D
    rlDrawRenderBatchActive();  // anything still batched into this texture lands first
    rlEnableTexture(textureId_);
    generateMipmap(kTexture2D);
    rlDisableTexture();
    rlTextureParameters(textureId_, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);
    rlTextureParameters(textureId_, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
}

void Framebuffer::Resize(int width, int height) {
    if (id_ != 0 && width_ == width && height_ == height) return;
    *this = Framebuffer(width, height, hdr_);
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
