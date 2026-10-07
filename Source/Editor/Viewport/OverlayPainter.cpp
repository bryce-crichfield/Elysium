#include "Editor/Viewport/OverlayPainter.h"
#include <algorithm>
#include "Editor/Editor.h"
#include "Editor/Style/Theme.h"

namespace Elysium {

using EditorStyle::Palette;

ImVec2 OverlayPainter::ToScreen(Vector2 world) const {
    Vector2 s = worldToScreen_(world);
    return ImVec2(s.x, s.y);
}

void OverlayPainter::Polygon(const std::vector<Vector2>& world, const ImVec4& color, float fillAlpha, float width, bool closed) {
    if (world.size() < 2) return;
    std::vector<ImVec2> screen;
    screen.reserve(world.size());
    for (const auto& p : world) screen.push_back(ToScreen(p));
    if (fillAlpha > 0.0f && screen.size() >= 3) {
        drawList_->AddConvexPolyFilled(screen.data(), (int)screen.size(), Palette::ToU32(Palette::WithAlpha(color, fillAlpha)));
    }
    drawList_->AddPolyline(screen.data(), (int)screen.size(), Palette::ToU32(color),
                           closed ? ImDrawFlags_Closed : ImDrawFlags_None, WidthOr(width));
}

void OverlayPainter::Handles(const std::vector<Vector2>& world, const ImVec4& color) {
    for (const auto& p : world) {
        ImVec2 s = ToScreen(p);
        drawList_->AddCircleFilled(s, HandleRadius, Palette::ToU32(Editor::Palette().Base));
        drawList_->AddCircle(s, HandleRadius, Palette::ToU32(color), 0, lineWidth_);
    }
}

void OverlayPainter::Line(Vector2 a, Vector2 b, const ImVec4& color, float width) {
    drawList_->AddLine(ToScreen(a), ToScreen(b), Palette::ToU32(color), WidthOr(width));
}

void OverlayPainter::Cross(Vector2 world, const ImVec4& color, float pixels) {
    ImVec2 c = ToScreen(world);
    ImU32 u = Palette::ToU32(color);
    drawList_->AddLine(ImVec2(c.x - pixels, c.y), ImVec2(c.x + pixels, c.y), u, lineWidth_);
    drawList_->AddLine(ImVec2(c.x, c.y - pixels), ImVec2(c.x, c.y + pixels), u, lineWidth_);
}

void OverlayPainter::Circle(Vector2 world, float pixels, const ImVec4& color, bool filled) {
    ImVec2 c = ToScreen(world);
    if (filled) drawList_->AddCircleFilled(c, pixels, Palette::ToU32(color));
    else        drawList_->AddCircle(c, pixels, Palette::ToU32(color), 0, 1.0f);
}

void OverlayPainter::Rect(Vector2 topLeft, Vector2 bottomRight, const ImVec4& color, float fillAlpha, float outlineWidth) {
    ImVec2 a = ToScreen(topLeft), b = ToScreen(bottomRight);
    if (fillAlpha > 0.0f) drawList_->AddRectFilled(a, b, Palette::ToU32(Palette::WithAlpha(color, fillAlpha)));
    if (outlineWidth > 0.0f) drawList_->AddRect(a, b, Palette::ToU32(color), 0.0f, 0, outlineWidth);
}

}  // namespace Elysium
