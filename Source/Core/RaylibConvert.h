#pragma once

// Backend-only bridge between Elysium's mirror types (Core/MathTypes.h, Core/Graphics.h)
// and raylib's native structs. Include this ONLY from .cpp files that actually call into
// raylib (RenderContext.cpp, the *Asset.cpp loaders, Application.cpp, the editor, ...).
// Never include it from a header — that would put both type families in scope for every
// downstream TU and reintroduce the ODR hazard this whole migration is removing.

#include "raylib.h"
#include "Core/Graphics.h"
#include "Core/Math/MathTypes.h"
#include "Core/Framebuffer.h"

namespace Elysium {

// --- math ---------------------------------------------------------------------

inline ::Vector2 ToRaylib(Vector2 v) { return {v.x, v.y}; }
inline ::Vector3 ToRaylib(Vector3 v) { return {v.x, v.y, v.z}; }
inline ::Rectangle ToRaylib(Rectangle r) { return {r.x, r.y, r.width, r.height}; }

inline Vector2 FromRaylib(::Vector2 v) { return {v.x, v.y}; }
inline Vector3 FromRaylib(::Vector3 v) { return {v.x, v.y, v.z}; }
inline Rectangle FromRaylib(::Rectangle r) { return {r.x, r.y, r.width, r.height}; }

inline ::Matrix ToRaylib(const Matrix& m) {
    // Both store 16 floats in GL column-major order; raylib's aggregate-init lists them
    // transposed (m0,m4,m8,m12 first), so assign by name to keep data[k] == m<k>.
    ::Matrix r;
    r.m0 = m.data[0];   r.m1 = m.data[1];   r.m2 = m.data[2];   r.m3 = m.data[3];
    r.m4 = m.data[4];   r.m5 = m.data[5];   r.m6 = m.data[6];   r.m7 = m.data[7];
    r.m8 = m.data[8];   r.m9 = m.data[9];   r.m10 = m.data[10]; r.m11 = m.data[11];
    r.m12 = m.data[12]; r.m13 = m.data[13]; r.m14 = m.data[14]; r.m15 = m.data[15];
    return r;
}

inline Matrix FromRaylib(const ::Matrix& m) {
    Matrix out;
    out.data[0] = m.m0;   out.data[1] = m.m1;   out.data[2] = m.m2;   out.data[3] = m.m3;
    out.data[4] = m.m4;   out.data[5] = m.m5;   out.data[6] = m.m6;   out.data[7] = m.m7;
    out.data[8] = m.m8;   out.data[9] = m.m9;   out.data[10] = m.m10; out.data[11] = m.m11;
    out.data[12] = m.m12; out.data[13] = m.m13; out.data[14] = m.m14; out.data[15] = m.m15;
    return out;
}

// --- color -------------------------------------------------------------------
// Layout- and value-identical (8-bit RGBA), but distinct types.

inline ::Color ToRaylib(Color c) { return {c.r, c.g, c.b, c.a}; }
inline Color FromRaylib(::Color c) { return {c.r, c.g, c.b, c.a}; }

// --- texture ---------------------------------------------------------------
// Same fields, distinct types. The mirror never owns anything raylib does; it's
// a value copy of the GPU handle + dimensions.

inline ::Texture2D ToRaylib(const Texture& t) {
    ::Texture2D r{};
    r.id = t.id;
    r.width = t.width;
    r.height = t.height;
    r.mipmaps = t.mipmaps;
    r.format = t.format;
    return r;
}
inline Texture FromRaylib(const ::Texture2D& t) {
    return Texture{t.id, t.width, t.height, t.mipmaps, t.format};
}

// --- framebuffer ------------------------------------------------------------
// The mirror stores GL object names; rebuild just enough of raylib's aggregate
// for BeginTextureMode / the color-texture blit.

inline ::Texture2D ToRaylibColorTexture(const Framebuffer& fb) {
    ::Texture2D t{};
    t.id = fb.TextureId();
    t.width = fb.Width();
    t.height = fb.Height();
    t.mipmaps = 1;
    t.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return t;
}

inline ::RenderTexture2D ToRaylib(const Framebuffer& fb) {
    ::RenderTexture2D rt{};
    rt.id = fb.Id();
    rt.texture = ToRaylibColorTexture(fb);
    rt.depth.id = fb.DepthBufferId();
    rt.depth.width = fb.Width();
    rt.depth.height = fb.Height();
    rt.depth.mipmaps = 1;
    return rt;
}

// --- blend mode -------------------------------------------------------------

inline ::BlendMode ToRaylib(BlendMode mode) {
    switch (mode) {
        case BlendMode::Additive:       return BLEND_ADDITIVE;
        case BlendMode::Multiplicative: return BLEND_MULTIPLIED;
        case BlendMode::Subtractive:    return BLEND_SUBTRACT_COLORS;
        case BlendMode::Alpha:
        default:                        return BLEND_ALPHA;
    }
}

}  // namespace Elysium
