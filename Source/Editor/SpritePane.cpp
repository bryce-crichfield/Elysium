#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <regex>
#include <set>
#include <tinyxml2.h>

#include "Core/Asset.h"
#include "Core/Graphics.h"
#include "Core/Path.h"
#include "Core/Xml.h"
#include "Editor/AssetField.h"
#include "Editor/AssetStyle.h"
#include "Editor/ContentPane.h"
#include "Editor/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IEditorService.h"

namespace Elysium {

namespace {

// A sequence's frames, in the loader's syntax: one row of the sheet, either all of it
// ("row:*") or some of its columns ("row:col" / "row:col,col,..."). Anything else makes
// the loader throw, so the editor won't save it.
struct Frames {
    int row = 0;
    bool all = false;
    std::vector<int> columns;  // in play order, when not `all`

    static std::optional<Frames> Parse(const char* text) {
        static const std::regex pattern(R"((\d+):(\*|\d+(,\d+)*))");
        std::cmatch match;
        if (!std::regex_match(text, match, pattern)) return std::nullopt;
        Frames frames;
        frames.row = std::stoi(match[1].str());
        frames.all = match[2].str() == "*";
        if (!frames.all) {
            const std::string list = match[2].str();
            for (size_t start = 0, comma; start <= list.size(); start = comma + 1) {
                comma = list.find(',', start);
                if (comma == std::string::npos) comma = list.size();
                frames.columns.push_back(std::stoi(list.substr(start, comma - start)));
            }
        }
        return frames;
    }

    std::string Format() const {
        std::string text = std::to_string(row) + ":";
        if (all) return text + "*";
        for (size_t i = 0; i < columns.size(); ++i) text += (i ? "," : "") + std::to_string(columns[i]);
        return text;
    }

    // Columns to play, for a sheet `sheetColumns` wide.
    std::vector<int> Played(int sheetColumns) const {
        if (!all) return columns;
        std::vector<int> played(sheetColumns);
        for (int i = 0; i < sheetColumns; ++i) played[i] = i;
        return played;
    }
};

// A sprite definition, edited visually: its animations (a sheet texture cut into a grid of
// frames) and the frame sequences every animation shares (e.g. one per facing). The sheet
// shows the selected animation's grid, where clicking frames builds the selected sequence,
// and the playback above it loops that sequence, where clicking sets the sprite's origin.
class SpritePane : public ContentPane {
   public:
    SpritePane(ServiceLocator& services, const Services::EditorDocument& document)
        : services_(services), fullPath_(document.fullPath) {
        Load();
        if (!animations_.empty()) selectedAnimation_ = 0;
        if (!sequences_.empty()) selectedSequence_ = 0;
    }

    void DrawToolbar() override {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        if (!status_.empty()) ColoredText(statusIsError_ ? Editor::Palette().Error : Editor::Palette().TextMuted, status_.c_str());
        else if (dirty_) ColoredText(Editor::Palette().TextMuted, "Unsaved changes");
    }

    void Draw() override {
        PushKindTheme(AssetKind::Sprite);
        const float formWidth = std::max(280.0f, ImGui::GetContentRegionAvail().x * 0.34f);
        ImGui::BeginChild("Form", ImVec2(formWidth, 0), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_ResizeX);
        DrawForm();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Preview", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawPreview();
        ImGui::EndChild();
        PopKindTheme();
    }

    bool CanSave() const override {
        return std::all_of(sequences_.begin(), sequences_.end(), [](const Sequence& s) { return Frames::Parse(s.frames).has_value(); });
    }

    bool SaveAs(const std::string& fullPath) override { return Write(fullPath); }

    bool Save() override {
        if (!Write(fullPath_)) return false;
        dirty_ = false;
        auto& assets = services_.Get<Services::IAssetService>();
        if (IAsset* asset = assets.GetAsset(Path::FromFullPath(fullPath_))) assets.ReloadAsset(asset);
        SetStatus("Saved");
        return true;
    }

   private:
    bool Write(const std::string& fullPath) {
        if (!CanSave()) {
            SetStatus("Fix the sequences marked in red first", true);
            return false;
        }
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLElement* root = doc.NewElement("Sprite");
        doc.InsertFirstChild(root);
        root->SetAttribute("name", name_);
        root->SetAttribute("originX", origin_[0]);
        root->SetAttribute("originY", origin_[1]);

        tinyxml2::XMLElement* sequences = doc.NewElement("Sequences");
        root->InsertEndChild(sequences);
        for (const auto& sequence : sequences_) {
            tinyxml2::XMLElement* el = doc.NewElement("Sequence");
            el->SetAttribute("name", sequence.name);
            el->SetAttribute("indicies", sequence.frames);  // the loader's spelling
            sequences->InsertEndChild(el);
        }
        for (const auto& animation : animations_) {
            tinyxml2::XMLElement* el = doc.NewElement("Sheet");
            el->SetAttribute("name", animation.name);
            el->SetAttribute("path", animation.texture.c_str());
            el->SetAttribute("rows", animation.rows);
            el->SetAttribute("columns", animation.columns);
            root->InsertEndChild(el);
        }

        if (!SaveXml(fullPath, doc)) {
            SetStatus("Failed to save", true);
            return false;
        }
        return true;
    }

    struct Animation {
        char name[64] = "";
        std::string texture;  // project-relative
        int rows = 1, columns = 1;
    };
    struct Sequence {
        char name[64] = "";
        char frames[128] = "0:*";
    };

    // ---- Form (left)

    void DrawForm() {
        KindSectionHeader(AssetKind::Sprite, "Sprite");
        PropertyLabel("Name");
        Edited(ImGui::InputText("##name", name_, sizeof(name_)));
        PropertyLabel("Origin");
        Edited(ImGui::DragFloat2("##origin", origin_, 0.005f, 0.0f, 1.0f, "%.3f"));
        ItemTooltip("The pivot, as a fraction of the frame (0,0 top left). Or click the playback.");

        KindSectionHeader(AssetKind::Sprite, "Animations");
        DrawAnimationList();

        KindSectionHeader(AssetKind::Sprite, "Sequences");
        DrawSequenceList();
    }

    void DrawAnimationList() {
        for (int i = 0; i < (int)animations_.size(); ++i) {
            char size[32];
            snprintf(size, sizeof(size), "%d x %d", animations_[i].columns, animations_[i].rows);
            if (ListRow(("anim" + std::to_string(i)).c_str(), selectedAnimation_ == i, ICON_FA_FILM, Editor::Palette().AssetSprite,
                        animations_[i].name[0] ? animations_[i].name : "(unnamed)", size)) {
                selectedAnimation_ = i;
            }
        }
        if (ImGui::Button(ICON_FA_PLUS "  Add Animation")) {
            animations_.emplace_back();
            selectedAnimation_ = (int)animations_.size() - 1;
            dirty_ = true;
        }

        if (selectedAnimation_ < 0 || selectedAnimation_ >= (int)animations_.size()) return;
        Animation& animation = animations_[selectedAnimation_];
        ImGui::Spacing();
        ImGui::PushID("animation");
        PropertyLabel("Name");
        Edited(ImGui::InputText("##name", animation.name, sizeof(animation.name)));
        if (AssetFieldRow("Texture", AssetKind::Texture, animation.texture)) {
            // A sheet in the conventional layout names its animation; default to that.
            if (!animation.name[0]) snprintf(animation.name, sizeof(animation.name), "%s", std::filesystem::path(animation.texture).stem().string().c_str());
            Edited(true);
        }
        PropertyLabel("Columns");
        Edited(ImGui::InputInt("##columns", &animation.columns));
        PropertyLabel("Rows");
        Edited(ImGui::InputInt("##rows", &animation.rows));
        animation.rows = std::clamp(animation.rows, 1, 256);
        animation.columns = std::clamp(animation.columns, 1, 256);
        if (ImGui::Button(ICON_FA_TRASH "  Remove Animation")) {
            animations_.erase(animations_.begin() + selectedAnimation_);
            selectedAnimation_ = std::min(selectedAnimation_, (int)animations_.size() - 1);
            dirty_ = true;
        }
        ImGui::PopID();
    }

    void DrawSequenceList() {
        for (int i = 0; i < (int)sequences_.size(); ++i) {
            const bool valid = Frames::Parse(sequences_[i].frames).has_value();
            if (ListRow(("seq" + std::to_string(i)).c_str(), selectedSequence_ == i, ICON_FA_LIST_OL,
                        valid ? Editor::Palette().AssetSprite : Editor::Palette().Error,
                        sequences_[i].name[0] ? sequences_[i].name : "(unnamed)", sequences_[i].frames)) {
                selectedSequence_ = i;
            }
        }
        if (ImGui::Button(ICON_FA_PLUS "  Add Sequence")) {
            sequences_.emplace_back();
            selectedSequence_ = (int)sequences_.size() - 1;
            dirty_ = true;
        }

        if (selectedSequence_ < 0 || selectedSequence_ >= (int)sequences_.size()) return;
        Sequence& sequence = sequences_[selectedSequence_];
        ImGui::Spacing();
        ImGui::PushID("sequence");
        PropertyLabel("Name");
        Edited(ImGui::InputText("##name", sequence.name, sizeof(sequence.name)));
        PropertyLabel("Frames");
        const bool valid = Frames::Parse(sequence.frames).has_value();
        if (!valid) ImGui::PushStyleColor(ImGuiCol_Text, Editor::Palette().Error);
        Edited(ImGui::InputText("##frames", sequence.frames, sizeof(sequence.frames)));
        if (!valid) ImGui::PopStyleColor();
        ItemTooltip("row:* (the whole row), row:col or row:col,col,... Or click frames on the sheet.");
        if (ImGui::Button(ICON_FA_TRASH "  Remove Sequence")) {
            sequences_.erase(sequences_.begin() + selectedSequence_);
            selectedSequence_ = std::min(selectedSequence_, (int)sequences_.size() - 1);
            dirty_ = true;
        }
        ImGui::PopID();
    }

    // ---- Preview (right)

    void DrawPreview() {
        const Animation* animation = selectedAnimation_ >= 0 && selectedAnimation_ < (int)animations_.size()
                                         ? &animations_[selectedAnimation_] : nullptr;
        if (!animation) {
            EmptyState("Add an animation to preview it");
            return;
        }
        if (animation->texture.empty()) {
            EmptyState("Pick a texture for this animation");
            return;
        }
        const Texture* texture = TextureOf(animation->texture);
        if (!texture || texture->id == 0) {
            EmptyState("Loading...");
            return;
        }

        Sequence* sequence = selectedSequence_ >= 0 && selectedSequence_ < (int)sequences_.size() ? &sequences_[selectedSequence_] : nullptr;
        const std::optional<Frames> frames = sequence ? Frames::Parse(sequence->frames) : std::nullopt;
        std::vector<int> played = frames ? frames->Played(animation->columns) : std::vector<int>{};
        std::erase_if(played, [&](int column) { return column < 0 || column >= animation->columns; });

        // Which frame is showing: advances at `fps_` while playing.
        if (playing_ && !played.empty()) {
            playTime_ += ImGui::GetIO().DeltaTime;
            frame_ = (int)(playTime_ * fps_) % (int)played.size();
        }
        frame_ = played.empty() ? 0 : std::min(frame_, (int)played.size() - 1);

        const float playbackHeight = std::floor(ImGui::GetContentRegionAvail().y * 0.45f);
        DrawPlayback(*animation, *texture, frames, played, playbackHeight);
        ImGui::Spacing();
        DrawSheet(*animation, *texture, sequence, frames, played);
    }

    void DrawPlayback(const Animation& animation, const Texture& texture, const std::optional<Frames>& frames,
                      const std::vector<int>& played, float height) {
        // Controls.
        if (IconButton(playing_ ? ICON_FA_PAUSE : ICON_FA_PLAY, playing_ ? "Pause" : "Play")) playing_ = !playing_;
        ImGui::SameLine();
        ImGui::BeginDisabled(played.empty());
        if (IconButton(ICON_FA_BACKWARD_STEP, "Previous frame")) Step(-1, (int)played.size());
        ImGui::SameLine();
        if (IconButton(ICON_FA_FORWARD_STEP, "Next frame")) Step(1, (int)played.size());
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("##fps", &fps_, 1.0f, 60.0f, "%.0f fps");
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        if (played.empty()) MutedText(frames ? "No frames in this row" : "Pick a valid sequence");
        else ImGui::TextDisabled("Frame %d / %d", frame_ + 1, (int)played.size());

        // The frame, fit and centered, with the origin marked.
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 area(ImGui::GetContentRegionAvail().x, height - ImGui::GetFrameHeightWithSpacing());
        if (area.x <= 0 || area.y <= 0) return;
        ImGui::InvisibleButton("##playback", area);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(origin, ImVec2(origin.x + area.x, origin.y + area.y), Editor::Palette().ToU32(Editor::Palette().Crust),
                                ImGui::GetStyle().FrameRounding);
        if (played.empty() || !frames) return;

        const float frameW = (float)texture.width / animation.columns, frameH = (float)texture.height / animation.rows;
        const float scale = std::min(area.x / frameW, area.y / frameH) * 0.9f;
        const ImVec2 size(frameW * scale, frameH * scale);
        const ImVec2 min(origin.x + (area.x - size.x) * 0.5f, origin.y + (area.y - size.y) * 0.5f);
        const ImVec2 max(min.x + size.x, min.y + size.y);
        const int column = played[frame_];
        const ImVec2 uv0(column * frameW / texture.width, frames->row * frameH / texture.height);
        const ImVec2 uv1((column + 1) * frameW / texture.width, (frames->row + 1) * frameH / texture.height);
        drawList->AddRect(min, max, Editor::Palette().ToU32(Editor::Palette().Border));
        drawList->AddImage((ImTextureID)(intptr_t)texture.id, min, max, uv0, uv1);

        // Clicking (or dragging) inside the frame moves the origin there.
        if (ImGui::IsItemActive()) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            origin_[0] = std::clamp((mouse.x - min.x) / size.x, 0.0f, 1.0f);
            origin_[1] = std::clamp((mouse.y - min.y) / size.y, 0.0f, 1.0f);
            dirty_ = true;
            status_.clear();
        }
        ItemTooltip("Click to set the origin");
        const ImVec2 pivot(min.x + origin_[0] * size.x, min.y + origin_[1] * size.y);
        const ImU32 marker = Editor::Palette().ToU32(Editor::Palette().AssetSprite);
        drawList->AddLine(ImVec2(pivot.x - 10, pivot.y), ImVec2(pivot.x + 10, pivot.y), marker, 2.0f);
        drawList->AddLine(ImVec2(pivot.x, pivot.y - 10), ImVec2(pivot.x, pivot.y + 10), marker, 2.0f);
        drawList->AddCircle(pivot, 4.0f, marker, 0, 2.0f);
    }

    void DrawSheet(const Animation& animation, const Texture& texture, Sequence* sequence, const std::optional<Frames>& frames,
                   const std::vector<int>& played) {
        if (sequence) MutedText("Click frames to add or remove them from the sequence; Shift+click takes the whole row.");
        else MutedText("Add a sequence to pick its frames here.");

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 area = ImGui::GetContentRegionAvail();
        if (area.x <= 0 || area.y <= 0) return;
        ImGui::InvisibleButton("##sheet", area);
        const bool hovered = ImGui::IsItemHovered(), clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        const float scale = std::min(area.x / texture.width, area.y / texture.height);
        const ImVec2 size(texture.width * scale, texture.height * scale);
        const ImVec2 min(origin.x + (area.x - size.x) * 0.5f, origin.y);
        const ImVec2 max(min.x + size.x, min.y + size.y);
        const float cellW = size.x / animation.columns, cellH = size.y / animation.rows;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, Editor::Palette().ToU32(Editor::Palette().Crust));
        drawList->AddImage((ImTextureID)(intptr_t)texture.id, min, max);

        // The sequence's frames, and the one playing.
        const ImVec4 purple = Editor::Palette().AssetSprite;
        auto cellRect = [&](int row, int column, ImU32 color, bool filled, float thickness = 1.0f) {
            const ImVec2 a(min.x + column * cellW, min.y + row * cellH), b(a.x + cellW, a.y + cellH);
            if (filled) drawList->AddRectFilled(a, b, color);
            else drawList->AddRect(a, b, color, 0.0f, 0, thickness);
        };
        if (frames) {
            for (int column : played) cellRect(frames->row, column, Editor::Palette().ToU32(Editor::Palette().WithAlpha(purple, 0.28f)), true);
            if (!played.empty()) cellRect(frames->row, played[frame_], Editor::Palette().ToU32(purple), false, 2.0f);
        }

        // Grid.
        const ImU32 line = Editor::Palette().ToU32(Editor::Palette().WithAlpha(Editor::Palette().Text, 0.18f));
        for (int c = 0; c <= animation.columns; ++c) drawList->AddLine(ImVec2(min.x + c * cellW, min.y), ImVec2(min.x + c * cellW, max.y), line);
        for (int r = 0; r <= animation.rows; ++r) drawList->AddLine(ImVec2(min.x, min.y + r * cellH), ImVec2(max.x, min.y + r * cellH), line);

        // Hover and click: toggle a frame in (or start) the sequence.
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        if (!hovered || !sequence || mouse.x < min.x || mouse.y < min.y || mouse.x >= max.x || mouse.y >= max.y) return;
        const int column = std::min((int)((mouse.x - min.x) / cellW), animation.columns - 1);
        const int row = std::min((int)((mouse.y - min.y) / cellH), animation.rows - 1);
        cellRect(row, column, Editor::Palette().ToU32(Editor::Palette().Text), false, 1.5f);
        ImGui::SetTooltip("Row %d, column %d", row, column);
        if (!clicked) return;

        Frames next;
        next.row = row;
        if (ImGui::GetIO().KeyShift) {
            next.all = true;
        } else if (frames && frames->row == row) {
            // Same row: toggle this column (a whole row becomes every column but this one).
            next.columns = frames->Played(animation.columns);
            auto it = std::find(next.columns.begin(), next.columns.end(), column);
            if (it != next.columns.end()) next.columns.erase(it);
            else next.columns.insert(std::upper_bound(next.columns.begin(), next.columns.end(), column), column);
            if (next.columns.empty()) next.columns.push_back(column);  // a sequence needs a frame
        } else {
            next.columns = {column};  // a sequence is one row: another row starts over
        }
        snprintf(sequence->frames, sizeof(sequence->frames), "%s", next.Format().c_str());
        dirty_ = true;
        status_.clear();
    }

    void Step(int delta, int count) {
        playing_ = false;
        frame_ = (frame_ + delta + count) % count;
    }

    // ---- Data

    const Texture* TextureOf(const std::string& path) {
        auto& assets = services_.Get<Services::IAssetService>();
        if (requested_.insert(path).second) assets.LoadAsset<Texture>(Path(path));
        return assets.Get<Texture>(Path(path));
    }

    void Load() {
        tinyxml2::XMLDocument doc;
        tinyxml2::XMLElement* root = doc.LoadFile(fullPath_.c_str()) == tinyxml2::XML_SUCCESS ? doc.RootElement() : nullptr;
        if (!root) {
            SetStatus("Couldn't read the sprite file", true);
            return;
        }
        auto copy = [](char* out, size_t size, const char* value) { snprintf(out, size, "%s", value ? value : ""); };
        copy(name_, sizeof(name_), root->Attribute("name"));
        origin_[0] = root->FloatAttribute("originX", 0.5f);
        origin_[1] = root->FloatAttribute("originY", 0.5f);
        if (auto* sequences = root->FirstChildElement("Sequences")) {
            for (auto* el = sequences->FirstChildElement("Sequence"); el; el = el->NextSiblingElement("Sequence")) {
                Sequence& sequence = sequences_.emplace_back();
                copy(sequence.name, sizeof(sequence.name), el->Attribute("name"));
                copy(sequence.frames, sizeof(sequence.frames), el->Attribute("indicies"));
            }
        }
        for (auto* el = root->FirstChildElement("Sheet"); el; el = el->NextSiblingElement("Sheet")) {
            Animation& animation = animations_.emplace_back();
            copy(animation.name, sizeof(animation.name), el->Attribute("name"));
            animation.texture = el->Attribute("path") ? el->Attribute("path") : "";
            animation.rows = el->IntAttribute("rows", 1);
            animation.columns = el->IntAttribute("columns", 1);
        }
    }

    void Edited(bool changed) {
        if (!changed) return;
        dirty_ = true;
        status_.clear();
    }

    void SetStatus(const std::string& message, bool isError = false) {
        status_ = message;
        statusIsError_ = isError;
    }

    ServiceLocator& services_;
    std::string fullPath_;
    char name_[64] = "";
    float origin_[2] = {0.5f, 0.5f};
    std::vector<Animation> animations_;
    std::vector<Sequence> sequences_;
    std::set<std::string> requested_;  // textures already asked of the asset service
    int selectedAnimation_ = -1, selectedSequence_ = -1;

    bool playing_ = true;
    float fps_ = 12.0f;
    float playTime_ = 0.0f;
    int frame_ = 0;

    bool dirty_ = false;
    std::string status_;
    bool statusIsError_ = false;
};

}  // namespace

std::unique_ptr<ContentPane> MakeSpritePane(ServiceLocator& services, const Services::EditorDocument& document) {
    return std::make_unique<SpritePane>(services, document);
}

}  // namespace Elysium
