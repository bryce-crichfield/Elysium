#pragma once

#include <algorithm>
#include <string>

#include <vector>
#include "Core/Graphics.h"
#include "Core/Math/MathTypes.h"
#include "Core/Framebuffer.h"

namespace Elysium {

class ServiceLocator;
class World;
class Shader;

// The raylib draw-call surface (matrix/blend/scissor stacks) plus the ambient state
// Renderable Render/Pick functions need: services, world.
class RenderContext {
public:
    RenderContext(ServiceLocator& services, const World& world);
    ~RenderContext();

    ServiceLocator& GetServices() const { return services_; }
    const World& GetWorld() const { return world_; }

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
    bool HasScissor() const { return !_scissorStack.empty(); }
    // Only valid while HasScissor(). Lets a caller save/restore the active clip around a
    // render-target detour without assuming who pushed it.
    Rectangle CurrentScissor() const { return _scissorStack.back(); }

    // Opacity, 0 to 1, multiplied into every draw's color (not DrawFramebuffer's, which blits
    // what was already drawn): the drawing entity's (LayerComponent::opacity, inherited).
    void SetOpacity(float opacity) { _opacity = opacity; }
    float GetOpacity() const { return _opacity; }

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
    void DrawEllipse(float centerX, float centerY, float radiusH, float radiusV, Color color);
    void DrawEllipseLines(float centerX, float centerY, float radiusH, float radiusV, Color color);
    void DrawText(const char* text, float x, float y, int fontSize, Color color);
    // Text in a named font from Assets/Fonts ("Ancient-Medium", with or without the
    // extension), loaded on first use. An empty or unknown name is the default font.
    void DrawText(const char* text, float x, float y, int fontSize, Color color, const std::string& font);
    static float MeasureText(const char* text, int fontSize, const std::string& font);
    void DrawTexturePro(const Texture& texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint);
    // Draws raylib DrawTriangle for each consecutive triple of vertices (3 per triangle).
    void DrawTriangleList(const std::vector<Vector2>& triangleVerts, Color color);
    // One quad over dest with texcoords 0..1 (top-left origin), meant to be drawn under a
    // pushed shader that computes every pixel itself. `texture` is bound as texture0;
    // null binds the backend's 1x1 white texture.
    void DrawShaderQuad(Rectangle dest, const Texture* texture, Color tint);
    // Same, with the corners given explicitly (top-left, bottom-left, bottom-right,
    // top-right in texcoord terms) so the quad can be rotated, scaled or mirrored.
    void DrawShaderQuad(const Vector2 (&corners)[4], const Texture* texture, Color tint);
    // Blits a framebuffer's color texture into dest (V-flipped for GL origin).
    void DrawFramebuffer(const Framebuffer& fb, Rectangle dest, Color tint);

    // Render target
    void BeginRenderTarget(const Framebuffer& target);
    void EndRenderTarget();

    // Clears the whole active target. Only meaningful right after a BeginRenderTarget.
    void ClearTarget(Color color);

    // Shader Stack. While a shader is pushed, every subsequent draw goes through it.
    // Pop restores the shader underneath, or the default one.
    void PushShader(const Shader& shader);
    void PopShader();

private:
    ServiceLocator& services_;
    const World& world_;

    std::vector<BlendMode> _blendStack;
    std::vector<Rectangle> _scissorStack;
    std::vector<const Shader*> _shaderStack;
    float _opacity = 1.0f;

    Color Faded(Color color) const {
        if (_opacity < 1.0f) color.a = (unsigned char)(color.a * std::max(0.0f, _opacity) + 0.5f);
        return color;
    }
};

} // namespace Elysium
