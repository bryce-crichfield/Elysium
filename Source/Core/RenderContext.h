#pragma once

#include <vector>
#include "Core/Graphics.h"
#include "Core/MathTypes.h"
#include "Core/Framebuffer.h"

namespace Elysium {

class ServiceLocator;
class World;

// The raylib draw-call surface (matrix/blend/scissor stacks) plus the ambient state
// Renderable Render/Pick functions need: services, world, isIsometric.
class RenderContext {
public:
    RenderContext(ServiceLocator& services, const World& world, bool isIsometric = false);
    ~RenderContext();

    ServiceLocator& GetServices() const { return services_; }
    const World& GetWorld() const { return world_; }
    bool IsIsometric() const { return isIsometric_; }

    // Matrix Stack
    void PushMatrix();
    void PopMatrix();
    void Translate(float x, float y, float z);
    void Rotate(float angle, float x, float y, float z);
    void Scale(float x, float y, float z);
    void MultiplyMatrix(const Matrix& mat);
    Matrix GetCurrentMatrix() const;

    // Blend Mode Stack
    void PushBlendMode(BlendMode mode);
    void PopBlendMode();

    // Scissor Stack
    void PushScissorMode(int x, int y, int width, int height);
    void PopScissorMode();

    // Drawing
    void DrawRectangle(float x, float y, float w, float h, Color color);
    void DrawRectangleLines(float x, float y, float w, float h, Color color);
    void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color);
    void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color);
    void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float lineThick, Color color);
    void DrawLine(float x1, float y1, float x2, float y2, Color color);
    void DrawLineEx(float x1, float y1, float x2, float y2, float thick, Color color);
    void DrawCircle(float x, float y, float radius, Color color);
    void DrawCircleLines(float x, float y, float radius, Color color);
    void DrawCircleGradient(float x, float y, float radius, Color color1, Color color2);
    // color1 = inner (center), color2 = outer (edge)
    void DrawEllipseGradient(float cx, float cy, float radiusH, float radiusV, Color inner, Color outer);
    void DrawEllipse(float centerX, float centerY, float radiusH, float radiusV, Color color);
    void DrawEllipseLines(float centerX, float centerY, float radiusH, float radiusV, Color color);
    void DrawText(const char* text, float x, float y, int fontSize, Color color);
    void DrawTexturePro(const Texture& texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint);
    // Draws raylib DrawTriangle for each consecutive triple of vertices (3 per triangle).
    void DrawTriangleList(const std::vector<Vector2>& triangleVerts, Color color);
    // Blits a framebuffer's color texture into dest (V-flipped for GL origin).
    void DrawFramebuffer(const Framebuffer& fb, Rectangle dest, Color tint);

    // Render target
    void BeginRenderTarget(const Framebuffer& target);
    void EndRenderTarget();

private:
    ServiceLocator& services_;
    const World& world_;
    bool isIsometric_;

    std::vector<BlendMode> _blendStack;
    std::vector<Rectangle> _scissorStack;
};

} // namespace Elysium
