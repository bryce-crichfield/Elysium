#include "Core/Framebuffer.h"

#include "raylib.h"

namespace Elysium {

Framebuffer CreateFramebuffer(int width, int height) {
    ::RenderTexture2D rt = ::LoadRenderTexture(width, height);
    Framebuffer fb;
    fb.id = rt.id;
    fb.textureId = rt.texture.id;
    fb.depthBufferId = rt.depth.id;
    fb.width = rt.texture.width;
    fb.height = rt.texture.height;
    return fb;
}

void DestroyFramebuffer(Framebuffer& fb) {
    if (fb.id != 0) {
        // UnloadRenderTexture only needs .id (+ .texture.id); rlUnloadFramebuffer
        // queries and deletes the attachments from the FBO itself.
        ::RenderTexture2D rt{};
        rt.id = fb.id;
        rt.texture.id = fb.textureId;
        rt.texture.width = fb.width;
        rt.texture.height = fb.height;
        rt.depth.id = fb.depthBufferId;
        ::UnloadRenderTexture(rt);
    }
    fb = Framebuffer{};
}

}  // namespace Elysium
