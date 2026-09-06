#pragma once

namespace Elysium {

    struct Color {
        float r, g, b, a;
    };

    // Nested to avoid colliding with raylib's macros of the same names (RED, WHITE, ...).
    namespace Colors {
        inline constexpr Color Red         = { 1.0f, 0.0f, 0.0f, 1.0f };
        inline constexpr Color Green       = { 0.0f, 1.0f, 0.0f, 1.0f };
        inline constexpr Color Blue        = { 0.0f, 0.0f, 1.0f, 1.0f };
        inline constexpr Color White       = { 1.0f, 1.0f, 1.0f, 1.0f };
        inline constexpr Color Black       = { 0.0f, 0.0f, 0.0f, 1.0f };
        inline constexpr Color Yellow      = { 1.0f, 1.0f, 0.0f, 1.0f };
        inline constexpr Color Cyan        = { 0.0f, 1.0f, 1.0f, 1.0f };
        inline constexpr Color Magenta     = { 1.0f, 0.0f, 1.0f, 1.0f };
        inline constexpr Color Orange      = { 1.0f, 0.5f, 0.0f, 1.0f };
        inline constexpr Color Purple      = { 0.5f, 0.0f, 0.5f, 1.0f };
        inline constexpr Color Gray        = { 0.5f, 0.5f, 0.5f, 1.0f };
        inline constexpr Color Transparent = { 0.0f, 0.0f, 0.0f, 0.0f };
    }

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
    };

    struct Shader {
        unsigned int id = 0;
    };

}
