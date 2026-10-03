#include "Core/Framebuffer.h"

#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <type_traits>
#include <utility>

// raylib's rlGenTextureMipmaps logs every call, which is too noisy for buffers that rebuild
// their mips every frame; call GL directly (GLFW is linked into raylib on desktop).
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

void Framebuffer::SetLinearFilter(bool linear) {
    if (textureId_ == 0) return;
    const int filter = linear ? RL_TEXTURE_FILTER_LINEAR : RL_TEXTURE_FILTER_NEAREST;
    rlTextureParameters(textureId_, RL_TEXTURE_MIN_FILTER, filter);
    rlTextureParameters(textureId_, RL_TEXTURE_MAG_FILTER, filter);
    rlTextureParameters(textureId_, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(textureId_, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
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

// =============================================================================
// MultisampleFramebuffer: rlgl has no multisampled targets, so this is plain GL.
// =============================================================================

namespace {

constexpr unsigned int kFramebuffer = 0x8D40;          // GL_FRAMEBUFFER
constexpr unsigned int kReadFramebuffer = 0x8CA8;      // GL_READ_FRAMEBUFFER
constexpr unsigned int kDrawFramebuffer = 0x8CA9;      // GL_DRAW_FRAMEBUFFER
constexpr unsigned int kRenderbuffer = 0x8D41;         // GL_RENDERBUFFER
constexpr unsigned int kRgba8 = 0x8058;                // GL_RGBA8
constexpr unsigned int kDepth24 = 0x81A6;              // GL_DEPTH_COMPONENT24
constexpr unsigned int kColorAttachment0 = 0x8CE0;     // GL_COLOR_ATTACHMENT0
constexpr unsigned int kDepthAttachment = 0x8D00;      // GL_DEPTH_ATTACHMENT
constexpr unsigned int kColorBufferBit = 0x00004000;   // GL_COLOR_BUFFER_BIT
constexpr unsigned int kDepthBufferBit = 0x00000100;   // GL_DEPTH_BUFFER_BIT
constexpr unsigned int kNearest = 0x2600;              // GL_NEAREST
constexpr unsigned int kInternalFormat = 0x8D44;       // GL_RENDERBUFFER_INTERNAL_FORMAT
constexpr unsigned int kMaxSamples = 0x8D57;           // GL_MAX_SAMPLES

struct Gl {
    void (*genFramebuffers)(int, unsigned int*);
    void (*deleteFramebuffers)(int, const unsigned int*);
    void (*bindFramebuffer)(unsigned int, unsigned int);
    void (*genRenderbuffers)(int, unsigned int*);
    void (*deleteRenderbuffers)(int, const unsigned int*);
    void (*bindRenderbuffer)(unsigned int, unsigned int);
    void (*renderbufferStorageMultisample)(unsigned int, int, unsigned int, int, int);
    void (*getRenderbufferParameteriv)(unsigned int, unsigned int, int*);
    void (*framebufferRenderbuffer)(unsigned int, unsigned int, unsigned int, unsigned int);
    void (*blitFramebuffer)(int, int, int, int, int, int, int, int, unsigned int, unsigned int);
    void (*clearColor)(float, float, float, float);
    void (*clearDepth)(double);
    void (*clear)(unsigned int);
    void (*getIntegerv)(unsigned int, int*);

    bool Loaded() const {
        return genFramebuffers && deleteFramebuffers && bindFramebuffer && genRenderbuffers && deleteRenderbuffers &&
               bindRenderbuffer && renderbufferStorageMultisample && getRenderbufferParameteriv &&
               framebufferRenderbuffer && blitFramebuffer && clearColor && clearDepth && clear && getIntegerv;
    }
};

const Gl& GL() {
    static const Gl gl = [] {
        Gl g{};
        auto load = [](auto& fn, const char* name) { fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(glfwGetProcAddress(name)); };
        load(g.genFramebuffers, "glGenFramebuffers");
        load(g.deleteFramebuffers, "glDeleteFramebuffers");
        load(g.bindFramebuffer, "glBindFramebuffer");
        load(g.genRenderbuffers, "glGenRenderbuffers");
        load(g.deleteRenderbuffers, "glDeleteRenderbuffers");
        load(g.bindRenderbuffer, "glBindRenderbuffer");
        load(g.renderbufferStorageMultisample, "glRenderbufferStorageMultisample");
        load(g.getRenderbufferParameteriv, "glGetRenderbufferParameteriv");
        load(g.framebufferRenderbuffer, "glFramebufferRenderbuffer");
        load(g.blitFramebuffer, "glBlitFramebuffer");
        load(g.clearColor, "glClearColor");
        load(g.clearDepth, "glClearDepth");
        load(g.clear, "glClear");
        load(g.getIntegerv, "glGetIntegerv");
        return g;
    }();
    return gl;
}

}  // namespace

MultisampleFramebuffer::MultisampleFramebuffer(int width, int height, int samples, unsigned int depthFormat) {
    const Gl& gl = GL();
    if (!gl.Loaded() || width <= 0 || height <= 0 || samples <= 0) return;
    depthFormat_ = depthFormat ? depthFormat : kDepth24;
    gl.genFramebuffers(1, &id_);
    gl.bindFramebuffer(kFramebuffer, id_);
    gl.genRenderbuffers(1, &colorBufferId_);
    gl.bindRenderbuffer(kRenderbuffer, colorBufferId_);
    gl.renderbufferStorageMultisample(kRenderbuffer, samples, kRgba8, width, height);
    gl.framebufferRenderbuffer(kFramebuffer, kColorAttachment0, kRenderbuffer, colorBufferId_);
    gl.genRenderbuffers(1, &depthBufferId_);
    gl.bindRenderbuffer(kRenderbuffer, depthBufferId_);
    gl.renderbufferStorageMultisample(kRenderbuffer, samples, depthFormat_, width, height);
    gl.framebufferRenderbuffer(kFramebuffer, kDepthAttachment, kRenderbuffer, depthBufferId_);
    gl.bindRenderbuffer(kRenderbuffer, 0);
    gl.bindFramebuffer(kFramebuffer, 0);
    width_ = width;
    height_ = height;
    samples_ = samples;
}

MultisampleFramebuffer::~MultisampleFramebuffer() { Destroy(); }

MultisampleFramebuffer::MultisampleFramebuffer(MultisampleFramebuffer&& other) noexcept { *this = std::move(other); }

MultisampleFramebuffer& MultisampleFramebuffer::operator=(MultisampleFramebuffer&& other) noexcept {
    if (this != &other) {
        Destroy();
        id_ = std::exchange(other.id_, 0);
        colorBufferId_ = std::exchange(other.colorBufferId_, 0);
        depthBufferId_ = std::exchange(other.depthBufferId_, 0);
        depthFormat_ = std::exchange(other.depthFormat_, 0);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
        samples_ = std::exchange(other.samples_, 0);
    }
    return *this;
}

void MultisampleFramebuffer::Ensure(int width, int height, int samples, const Framebuffer& depthOf) {
    const Gl& gl = GL();
    if (!gl.Loaded()) return;
    static const int maxSamples = [&] {
        int n = 0;
        gl.getIntegerv(kMaxSamples, &n);
        return n;
    }();
    samples = std::min(samples, maxSamples);
    unsigned int depthFormat = kDepth24;
    if (depthOf.DepthBufferId() != 0) {
        int format = 0;
        gl.bindRenderbuffer(kRenderbuffer, depthOf.DepthBufferId());
        gl.getRenderbufferParameteriv(kRenderbuffer, kInternalFormat, &format);
        gl.bindRenderbuffer(kRenderbuffer, 0);
        if (format != 0) depthFormat = (unsigned int)format;
    }
    if (id_ != 0 && width_ == width && height_ == height && samples_ == samples && depthFormat_ == depthFormat) return;
    *this = MultisampleFramebuffer(width, height, samples, depthFormat);
}

void MultisampleFramebuffer::Begin() {
    if (id_ == 0) return;
    const Gl& gl = GL();
    rlDrawRenderBatchActive();
    gl.bindFramebuffer(kFramebuffer, id_);
    gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl.clearDepth(1.0);
    gl.clear(kColorBufferBit | kDepthBufferBit);
}

void MultisampleFramebuffer::Resolve(const Framebuffer* color, const Framebuffer* depth) {
    if (id_ == 0) return;
    const Gl& gl = GL();
    rlDrawRenderBatchActive();
    gl.bindFramebuffer(kReadFramebuffer, id_);
    if (color && color->IsValid()) {
        gl.bindFramebuffer(kDrawFramebuffer, color->Id());
        gl.blitFramebuffer(0, 0, width_, height_, 0, 0, color->Width(), color->Height(), kColorBufferBit, kNearest);
    }
    if (depth && depth->IsValid()) {
        gl.bindFramebuffer(kDrawFramebuffer, depth->Id());
        gl.blitFramebuffer(0, 0, width_, height_, 0, 0, depth->Width(), depth->Height(), kDepthBufferBit, kNearest);
    }
    const Framebuffer* bound = depth ? depth : color;
    gl.bindFramebuffer(kFramebuffer, bound ? bound->Id() : 0);
}

void MultisampleFramebuffer::Destroy() {
    const Gl& gl = GL();
    if (gl.Loaded()) {
        if (colorBufferId_) gl.deleteRenderbuffers(1, &colorBufferId_);
        if (depthBufferId_) gl.deleteRenderbuffers(1, &depthBufferId_);
        if (id_) gl.deleteFramebuffers(1, &id_);
    }
    id_ = colorBufferId_ = depthBufferId_ = 0;
    width_ = height_ = samples_ = 0;
}

}  // namespace Elysium
