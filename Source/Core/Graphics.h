#pragma once

namespace Elysium {

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
    };

    struct Shader {
        unsigned int id = 0;
    };

}
