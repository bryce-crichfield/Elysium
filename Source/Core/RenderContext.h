#pragma once

#include <vector>
#include "raylib.h"

namespace Elysium {

class RenderContext {
public:
    RenderContext();
    ~RenderContext();

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
    void DrawLine(float x1, float y1, float x2, float y2, Color color);
    void DrawCircle(float x, float y, float radius, Color color);
    void DrawCircleLines(float x, float y, float radius, Color color);
    void DrawCircleGradient(float x, float y, float radius, Color color1, Color color2);
    void DrawEllipseLines(float centerX, float centerY, float radiusH, float radiusV, Color color);
    void DrawText(const char* text, float x, float y, int fontSize, Color color);
    void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint);

    // Raw State
    void BeginTextureMode(RenderTexture2D& target);
    void EndTextureMode();

private:
    std::vector<BlendMode> _blendStack;
    std::vector<Rectangle> _scissorStack;
};

} // namespace Elysium
