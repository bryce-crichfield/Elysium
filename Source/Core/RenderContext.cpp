#include "Core/RenderContext.h"
#include "Core/Path.h"
#include "Core/RaylibConvert.h"
#include "Core/Shader.h"
#include "rlgl.h"
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace Elysium {

RenderContext::RenderContext(ServiceLocator& services, const World& world)
    : services_(services), world_(world) {
}

RenderContext::~RenderContext() {
    // Ensure stacks are unwound?
}

// Matrix Stack
void RenderContext::PushMatrix() {
    rlPushMatrix();
}

void RenderContext::PopMatrix() {
    rlPopMatrix();
}

void RenderContext::Translate(float x, float y, float z) {
    rlTranslatef(x, y, z);
}

void RenderContext::Rotate(float angle, float x, float y, float z) {
    rlRotatef(angle, x, y, z);
}

void RenderContext::Scale(float x, float y, float z) {
    rlScalef(x, y, z);
}

void RenderContext::MultiplyMatrix(const Matrix& mat) {
    // Elysium::Matrix::data is already GL column-major (translation in [12..14]),
    // which is exactly what rlMultMatrixf expects — no conversion needed.
    rlMultMatrixf(mat.data);
}

Matrix RenderContext::GetCurrentMatrix() const {
    return Matrix::Identity(); // Placeholder if not strictly needed by RenderSystem logic yet.
}

// Blend Mode Stack
void RenderContext::PushBlendMode(BlendMode mode) {
    _blendStack.push_back(mode);
    BeginBlendMode(ToRaylib(mode));
}

void RenderContext::PopBlendMode() {
    if (!_blendStack.empty()) {
        _blendStack.pop_back();
        if (!_blendStack.empty()) {
            BeginBlendMode(ToRaylib(_blendStack.back()));
        } else {
            EndBlendMode(); // Revert to default
        }
    } else {
        EndBlendMode();
    }
}

// Scissor Stack
void RenderContext::PushScissorMode(int x, int y, int width, int height) {
    _scissorStack.push_back({ (float)x, (float)y, (float)width, (float)height });
    BeginScissorMode(x, y, width, height);
}

void RenderContext::PopScissorMode() {
    if (!_scissorStack.empty()) {
        _scissorStack.pop_back();
        if (!_scissorStack.empty()) {
            Rectangle r = _scissorStack.back();
            BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
        } else {
            EndScissorMode();
        }
    } else {
        EndScissorMode();
    }
}

// Drawing
void RenderContext::DrawRectangle(float x, float y, float w, float h, Color color) {
    DrawRectangleV({x, y}, {w, h}, ToRaylib(color));
}

void RenderContext::DrawRectangleLines(float x, float y, float w, float h, Color color) {
    ::DrawRectangleLines(x, y, w, h, ToRaylib(color));
}

void RenderContext::DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color) {
    ::DrawRectangleLinesEx(ToRaylib(rec), lineThick, ToRaylib(color));
}

void RenderContext::DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color) {
    ::DrawRectangleRounded(ToRaylib(rec), roundness, segments, ToRaylib(color));
}

void RenderContext::DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float lineThick, Color color) {
    ::DrawRectangleRoundedLinesEx(ToRaylib(rec), roundness, segments, lineThick, ToRaylib(color));
}

void RenderContext::DrawLine(float x1, float y1, float x2, float y2, Color color) {
    DrawLineV({x1, y1}, {x2, y2}, ToRaylib(color));
}

void RenderContext::DrawLineEx(float x1, float y1, float x2, float y2, float thick, Color color) {
    ::DrawLineEx({x1, y1}, {x2, y2}, thick, ToRaylib(color));
}

void RenderContext::DrawCircle(float x, float y, float radius, Color color) {
    DrawCircleV({x, y}, radius, ToRaylib(color));
}

void RenderContext::DrawCircleLines(float x, float y, float radius, Color color) {
    DrawCircleLinesV({x, y}, radius, ToRaylib(color));
}

void RenderContext::DrawEllipse(float centerX, float centerY, float radiusH, float radiusV, Color color) {
    ::DrawEllipse((int)centerX, (int)centerY, radiusH, radiusV, ToRaylib(color));
}

void RenderContext::DrawEllipseLines(float centerX, float centerY, float radiusH, float radiusV, Color color) {
    ::DrawEllipseLines((int)centerX, (int)centerY, radiusH, radiusV, ToRaylib(color));
}

void RenderContext::DrawText(const char* text, float x, float y, int fontSize, Color color) {
    ::DrawText(text, (int)x, (int)y, fontSize, ToRaylib(color));
}

// Fonts, loaded once (on the GL thread) at a large base size so they scale down cleanly. A
// path with a folder ("Fonts/EnchantedLand-Regular.ttf") is a project font asset; a bare name
// is an engine font in Assets/Fonts. One that fails to load is remembered as missing and falls
// back to the default font.
static const ::Font* FindFont(const std::string& name) {
    if (name.empty()) return nullptr;
    static std::unordered_map<std::string, ::Font> fonts;
    auto it = fonts.find(name);
    if (it == fonts.end()) {
        ::Font font{};
        const bool projectFont = name.find('/') != std::string::npos;
        for (const char* ext : {"", ".ttf", ".otf"}) {
            std::string path = projectFont ? Path(name).GetFullPath() + ext : "Assets/Fonts/" + name + ext;
            if (!std::filesystem::is_regular_file(path)) continue;
            font = ::LoadFontEx(path.c_str(), 96, nullptr, 0);
            if (font.texture.id != 0) {
                ::GenTextureMipmaps(&font.texture);
                ::SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
                break;
            }
        }
        it = fonts.emplace(name, font).first;
    }
    return it->second.texture.id != 0 ? &it->second : nullptr;
}

static float FontSpacing(int fontSize) { return fontSize / 10.0f; }

void RenderContext::DrawText(const char* text, float x, float y, int fontSize, Color color, const std::string& font) {
    const ::Font* f = FindFont(font);
    if (!f) { DrawText(text, x, y, fontSize, color); return; }
    ::DrawTextEx(*f, text, ::Vector2{x, y}, (float)fontSize, FontSpacing(fontSize), ToRaylib(color));
}

float RenderContext::MeasureText(const char* text, int fontSize, const std::string& font) {
    const ::Font* f = FindFont(font);
    if (!f) return (float)::MeasureText(text, fontSize);
    return ::MeasureTextEx(*f, text, (float)fontSize, FontSpacing(fontSize)).x;
}

void RenderContext::DrawTexturePro(const Texture& texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint) {
    ::DrawTexturePro(ToRaylib(texture), ToRaylib(source), ToRaylib(dest), ToRaylib(origin), rotation, ToRaylib(tint));
}

void RenderContext::DrawTriangleList(const std::vector<Vector2>& triangleVerts, Color color) {
    ::Color rlColor = ToRaylib(color);
    for (size_t i = 0; i + 2 < triangleVerts.size(); i += 3) {
        ::DrawTriangle(ToRaylib(triangleVerts[i]), ToRaylib(triangleVerts[i + 1]),
                       ToRaylib(triangleVerts[i + 2]), rlColor);
    }
}

void RenderContext::DrawFramebuffer(const Framebuffer& fb, Rectangle dest, Color tint) {
    ::Rectangle src{0, 0, (float)fb.Width(), -(float)fb.Height()};
    ::DrawTexturePro(ToRaylibColorTexture(fb), src, ToRaylib(dest), ::Vector2{0, 0}, 0.0f, ToRaylib(tint));
}

void RenderContext::BeginRenderTarget(const Framebuffer& target) {
    ::BeginTextureMode(ToRaylib(target));
}

void RenderContext::EndRenderTarget() {
    ::EndTextureMode();
}

void RenderContext::DrawShaderQuad(Rectangle dest, const Texture* texture, Color tint) {
    float left = dest.x, top = dest.y;
    float right = dest.x + dest.width, bottom = dest.y + dest.height;
    const Vector2 corners[4] = { {left, top}, {left, bottom}, {right, bottom}, {right, top} };
    DrawShaderQuad(corners, texture, tint);
}

void RenderContext::DrawShaderQuad(const Vector2 (&corners)[4], const Texture* texture, Color tint) {
    unsigned int textureId = (texture && texture->id != 0) ? texture->id : rlGetTextureIdDefault();

    rlSetTexture(textureId);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(0.0f, 0.0f); rlVertex2f(corners[0].x, corners[0].y);
    rlTexCoord2f(0.0f, 1.0f); rlVertex2f(corners[1].x, corners[1].y);
    rlTexCoord2f(1.0f, 1.0f); rlVertex2f(corners[2].x, corners[2].y);
    rlTexCoord2f(1.0f, 0.0f); rlVertex2f(corners[3].x, corners[3].y);
    rlEnd();
    rlSetTexture(0);
}

void RenderContext::ClearTarget(Color color) {
    ::ClearBackground(ToRaylib(color));
}

void RenderContext::PushShader(const Shader& shader) {
    _shaderStack.push_back(&shader);
    if (const void* handle = shader.NativeHandle()) {
        ::BeginShaderMode(*static_cast<const ::Shader*>(handle));
    }
}

void RenderContext::PopShader() {
    if (!_shaderStack.empty()) _shaderStack.pop_back();
    if (!_shaderStack.empty()) {
        if (const void* handle = _shaderStack.back()->NativeHandle()) {
            ::BeginShaderMode(*static_cast<const ::Shader*>(handle));
            return;
        }
    }
    ::EndShaderMode();
}

} // namespace Elysium
