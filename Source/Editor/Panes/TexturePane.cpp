#include <algorithm>
#include <cmath>
#include <filesystem>

#include "Core/Graphics.h"
#include "Core/Path.h"
#include "Editor/Panes/ContentPane.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "Editor/EditorApplication.h"

namespace Elysium {

namespace {

// A texture preview: the image centered over a checkerboard (so transparency shows), fit
// to the pane or at a chosen zoom. Nothing to edit, so nothing to save.
class TexturePane : public ContentPane {
   public:
    TexturePane(ServiceLocator& services, const EditorDocument& document)
        : services_(services), fullPath_(document.fullPath), path_(Path::FromFullPath(document.fullPath)) {
        services_.Get<Services::IAssetService>().LoadAsset<Texture>(path_);
    }

    void DrawToolbar() override {
        if (ToggleIconButton(ICON_FA_EXPAND, fit_, "Fit to the pane")) fit_ = true;
        ImGui::SameLine();
        if (ToggleIconButton("1:1", !fit_ && zoom_ == 1.0f, "Actual size")) {
            fit_ = false;
            zoom_ = 1.0f;
        }
        if (const Texture* texture = services_.Get<Services::IAssetService>().Get<Texture>(path_)) {
            char info[64];
            snprintf(info, sizeof(info), "%d x %d   %.0f%%", texture->width, texture->height, shownZoom_ * 100.0f);
            AlignRight(ImGui::CalcTextSize(info).x);
            ImGui::AlignTextToFramePadding();
            ColoredText(Editor::Palette().TextMuted, info);
        }
    }

    void Draw() override {
        const Texture* texture = services_.Get<Services::IAssetService>().Get<Texture>(path_);
        if (!texture || texture->id == 0) {
            EmptyState("Loading...");
            return;
        }

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("##preview", avail);
        // Scroll to zoom, from wherever the fit left it.
        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
            zoom_ = std::clamp(shownZoom_ * std::pow(1.15f, ImGui::GetIO().MouseWheel), 0.05f, 32.0f);
            fit_ = false;
        }

        const float fitZoom = std::min(avail.x / texture->width, avail.y / texture->height) * 0.95f;
        shownZoom_ = fit_ ? fitZoom : zoom_;
        const ImVec2 size(texture->width * shownZoom_, texture->height * shownZoom_);
        const ImVec2 min(origin.x + (avail.x - size.x) * 0.5f, origin.y + (avail.y - size.y) * 0.5f);
        const ImVec2 max(min.x + size.x, min.y + size.y);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), true);
        const float cell = 12.0f;
        const ImU32 light = Editor::Palette().ToU32(Editor::Palette().Surface1);
        const ImU32 dark = Editor::Palette().ToU32(Editor::Palette().Surface0);
        // Only the cells on screen: zoomed in, the image can be far bigger than the pane.
        const float startX = min.x + std::floor(std::max(0.0f, origin.x - min.x) / cell) * cell;
        const float startY = min.y + std::floor(std::max(0.0f, origin.y - min.y) / cell) * cell;
        const float endX = std::min(max.x, origin.x + avail.x), endY = std::min(max.y, origin.y + avail.y);
        for (float y = startY; y < endY; y += cell) {
            for (float x = startX; x < endX; x += cell) {
                const bool odd = ((int)((x - min.x) / cell) + (int)((y - min.y) / cell)) % 2;
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + cell, max.x), std::min(y + cell, max.y)), odd ? dark : light);
            }
        }
        drawList->AddImage((ImTextureID)(intptr_t)texture->id, min, max);
        drawList->AddRect(min, max, Editor::Palette().ToU32(Editor::Palette().Border));
        drawList->PopClipRect();
    }

    // Nothing to edit, so Save As is a copy of the image.
    bool SaveAs(const std::string& fullPath) override {
        std::error_code ec;
        return std::filesystem::copy_file(fullPath_, fullPath, ec) && !ec;
    }

   private:
    ServiceLocator& services_;
    std::string fullPath_;
    Path path_;
    bool fit_ = true;
    float zoom_ = 1.0f;
    float shownZoom_ = 1.0f;
};

}  // namespace

std::unique_ptr<ContentPane> MakeTexturePane(ServiceLocator& services, const EditorDocument& document) {
    return std::make_unique<TexturePane>(services, document);
}

}  // namespace Elysium
