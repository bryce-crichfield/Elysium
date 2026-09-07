#pragma once

#include "Core/Graphics.h"

// Offscreen render-target lifecycle. Elysium::Framebuffer (Core/Graphics.h) is a plain
// GL-shaped handle: id / textureId / depthBufferId are the backend object names. These
// helpers are the seam — a raylib LoadRenderTexture today, glGenFramebuffers later —
// and are implemented in Framebuffer.cpp so raylib.h stays out of every header.

namespace Elysium {

Framebuffer CreateFramebuffer(int width, int height);
void DestroyFramebuffer(Framebuffer& fb);
inline bool IsFramebufferValid(const Framebuffer& fb) { return fb.id != 0; }

}  // namespace Elysium
