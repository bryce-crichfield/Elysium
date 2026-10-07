#pragma once
#include <functional>
#include <vector>
#include "Core/Math/MathTypes.h"
#include "imgui.h"

namespace Elysium {

// Draws world-space shapes onto an ImGui draw list over the viewport image.
class OverlayPainter {
public:
    static constexpr float HandleRadius = 6.0f;

    // worldToScreen takes a ground position and a height.
    OverlayPainter(ImDrawList* drawList, std::function<Vector2(Vector3)> worldToScreen, float zoom, float lineWidth)
        : drawList_(drawList), worldToScreen_(std::move(worldToScreen)), zoom_(zoom), lineWidth_(lineWidth) {}

    ImVec2 ToScreen(Vector2 world) const { return ToScreen(Vector3{world.x, world.y, 0.0f}); }
    ImVec2 ToScreen(Vector3 world) const;
    float ToWorldDistance(float pixels) const { return pixels / std::max(zoom_, 0.01f); }
    float LineWidth() const { return lineWidth_; }

    void Polygon(const std::vector<Vector2>& world, const ImVec4& color, float fillAlpha = 0.0f, float width = 0.0f, bool closed = true);
    void Polygon(const std::vector<Vector3>& world, const ImVec4& color, float fillAlpha = 0.0f, float width = 0.0f, bool closed = true);
    void Handles(const std::vector<Vector2>& world, const ImVec4& color);
    void Line(Vector2 a, Vector2 b, const ImVec4& color, float width = 0.0f);
    void Cross(Vector2 world, const ImVec4& color, float pixels = 5.0f);
    void Circle(Vector2 world, float pixels, const ImVec4& color, bool filled = false);
    void Rect(Vector2 topLeft, Vector2 bottomRight, const ImVec4& color, float fillAlpha = 0.0f, float outlineWidth = 1.0f);

private:
    float WidthOr(float width) const { return width > 0.0f ? width : lineWidth_; }
    void Polygon(std::vector<ImVec2> screen, const ImVec4& color, float fillAlpha, float width, bool closed);

    ImDrawList* drawList_;
    std::function<Vector2(Vector3)> worldToScreen_;
    float zoom_;
    float lineWidth_;
};

}  // namespace Elysium
