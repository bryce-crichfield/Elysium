#include "Core/RenderContext.h"
#include "rlgl.h"
#include "raymath.h"

namespace Elysium {

RenderContext::RenderContext() {
    // Initialize default states if necessary
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
    rlMultMatrixf(MatrixToFloat(mat));
}

Matrix RenderContext::GetCurrentMatrix() const {
    return MatrixIdentity(); // Placeholder if not strictly needed by RenderSystem logic yet.
}

// Blend Mode Stack
void RenderContext::PushBlendMode(BlendMode mode) {
    _blendStack.push_back(mode);
    BeginBlendMode(mode);
}

void RenderContext::PopBlendMode() {
    if (!_blendStack.empty()) {
        _blendStack.pop_back();
        if (!_blendStack.empty()) {
            BeginBlendMode(_blendStack.back());
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
    DrawRectangleV({x, y}, {w, h}, color);
}

void RenderContext::DrawRectangleLines(float x, float y, float w, float h, Color color) {
    ::DrawRectangleLines(x, y, w, h, color);
}

void RenderContext::DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color) {
    ::DrawRectangleLinesEx(rec, lineThick, color);
}

void RenderContext::DrawLine(float x1, float y1, float x2, float y2, Color color) {
    DrawLineV({x1, y1}, {x2, y2}, color);
}

void RenderContext::DrawCircle(float x, float y, float radius, Color color) {
    DrawCircleV({x, y}, radius, color);
}

void RenderContext::DrawCircleLines(float x, float y, float radius, Color color) {
    DrawCircleLinesV({x, y}, radius, color);
}

void RenderContext::DrawCircleGradient(float x, float y, float radius, Color color1, Color color2) {
    ::DrawCircleGradient((int)x, (int)y, radius, color1, color2);
}

void RenderContext::DrawEllipseGradient(float cx, float cy, float radiusH, float radiusV, Color inner, Color outer) {
    // Triangle fan from center to ellipse perimeter, same approach as Raylib's DrawCircleGradient
    // but with independent horizontal and vertical radii.
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < 360; i += 10) {
        rlColor4ub(inner.r, inner.g, inner.b, inner.a);
        rlVertex2f(cx, cy);
        rlColor4ub(outer.r, outer.g, outer.b, outer.a);
        rlVertex2f(cx + sinf(DEG2RAD * i)        * radiusH, cy + cosf(DEG2RAD * i)        * radiusV);
        rlColor4ub(outer.r, outer.g, outer.b, outer.a);
        rlVertex2f(cx + sinf(DEG2RAD * (i + 10)) * radiusH, cy + cosf(DEG2RAD * (i + 10)) * radiusV);
    }
    rlEnd();
}

void RenderContext::DrawEllipseLines(float centerX, float centerY, float radiusH, float radiusV, Color color) {
    ::DrawEllipseLines((int)centerX, (int)centerY, radiusH, radiusV, color);
}

void RenderContext::DrawText(const char* text, float x, float y, int fontSize, Color color) {
    ::DrawText(text, (int)x, (int)y, fontSize, color);
}

void RenderContext::DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint) {
    ::DrawTexturePro(texture, source, dest, origin, rotation, tint);
}

void RenderContext::BeginTextureMode(RenderTexture2D& target) {
    ::BeginTextureMode(target);
}

void RenderContext::EndTextureMode() {
    ::EndTextureMode();
}

} // namespace Elysium
