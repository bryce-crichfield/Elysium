#pragma once

namespace Elysium {

    struct Skeleton;  // Core/Animation.h

    // Layout- and value-compatible with raylib's ::Color (8-bit RGBA, 0-255).
    struct Color {
        unsigned char r, g, b, a;
    };

    // Nested to avoid colliding with raylib's macros of the same names (RED, WHITE, ...).
    namespace Colors {
        inline constexpr Color Red         = { 230,  41,  55, 255 };
        inline constexpr Color Green       = {   0, 228,  48, 255 };
        inline constexpr Color Blue        = {   0, 121, 241, 255 };
        inline constexpr Color White       = { 255, 255, 255, 255 };
        inline constexpr Color Black       = {   0,   0,   0, 255 };
        inline constexpr Color Yellow      = { 253, 249,   0, 255 };
        inline constexpr Color Cyan        = {   0, 255, 255, 255 };
        inline constexpr Color Magenta     = { 255,   0, 255, 255 };
        inline constexpr Color Orange      = { 255, 161,   0, 255 };
        inline constexpr Color Purple      = { 200, 122, 255, 255 };
        inline constexpr Color Gray        = { 130, 130, 130, 255 };
        inline constexpr Color Transparent = {   0,   0,   0,   0 };
        inline constexpr Color Blank       = {   0,   0,   0,   0 };
    }

    enum class BlendMode {
        Alpha,
        Additive,
        Multiplicative,
        Subtractive
    };

    struct Texture {
        unsigned int id = 0;
        int width = 0;
        int height = 0;
        int mipmaps = 0;
        int format = 0;
    };

    struct Font {
        unsigned int textureId = 0;
        int baseSize = 0;
        int glyphCount = 0;
    };

    struct Model {
        int meshCount = 0;
        int materialCount = 0;
        // Model-space bounds over every mesh (the file's axes: y up).
        float boundsMin[3] = {0.0f, 0.0f, 0.0f};
        float boundsMax[3] = {0.0f, 0.0f, 0.0f};
        // The loaded raylib ::Model, for the .cpp files that draw or ray-cast it (see
        // Core/World3D.h). Null until the asset is finalized.
        void* native = nullptr;
        // The bones its meshes are skinned to (a .mesh with a .skel beside it), else null.
        // An AnimationComponent poses them.
        const Skeleton* skeleton = nullptr;
    };

    // Shader is not a POD handle like the above — it owns a compiled program plus its
    // reflected uniform list. See Core/Shader.h.

}
