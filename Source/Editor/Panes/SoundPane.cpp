#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

#include "Core/Audio.h"
#include "Core/Path.h"
#include "Editor/Panes/ContentPane.h"
#include "Editor/Widgets/Widgets.h"
#include "Interfaces/IAssetService.h"
#include "Interfaces/IAudioService.h"
#include "Editor/EditorApplication.h"

namespace Elysium {

namespace {

// "1:05.20"
static std::string FormatTime(float seconds) {
    const int minutes = (int)(seconds / 60.0f);
    char text[32];
    snprintf(text, sizeof(text), "%d:%05.2f", minutes, seconds - minutes * 60.0f);
    return text;
}

// A sound preview: its waveform, with play/pause, stop and loop on the toolbar, and a
// timeline to click or drag to scrub. The clip plays through the AudioService like any
// other sound, so it plays at the master volume. Nothing to edit, so nothing to save.
class SoundPane : public ContentPane {
   public:
    SoundPane(ServiceLocator& services, const EditorDocument& document)
        : services_(services), fullPath_(document.fullPath), path_(Path::FromFullPath(document.fullPath)) {
        services_.Get<Services::IAssetService>().LoadAsset<Sound>(path_);
    }

    ~SoundPane() override {
        if (id_ != Services::InvalidSound) Player().Stop(id_);
    }

    void DrawToolbar() override {
        const auto playback = Poll();
        const bool playing = playback && !playback->paused;
        if (IconButton(playing ? ICON_FA_PAUSE : ICON_FA_PLAY, playing ? "Pause (Space)" : "Play (Space)")) TogglePlay();
        ImGui::SameLine();
        if (IconButton(ICON_FA_STOP, "Stop, back to the start")) StopPlayback();
        ImGui::SameLine();
        if (ToggleIconButton(ICON_FA_REPEAT, loop_, "Loop")) {
            loop_ = !loop_;
            if (id_ != Services::InvalidSound) Player().SetLooping(id_, loop_);
        }

        if (const Sound* sound = GetSound()) {
            const float duration = (float)sound->frameCount / Audio::SampleRate;
            const std::string info = FormatTime(Position()) + " / " + FormatTime(duration);
            AlignRight(ImGui::CalcTextSize(info.c_str()).x);
            ImGui::AlignTextToFramePadding();
            ColoredText(Editor::Palette().TextMuted, info.c_str());
        }
    }

    void Draw() override {
        const Sound* sound = GetSound();
        if (!sound || !sound->samples || sound->frameCount == 0) {
            EmptyState("Loading...");
            return;
        }
        if (peaks_.empty()) BuildPeaks(*sound);
        const float duration = (float)sound->frameCount / Audio::SampleRate;

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput &&
            ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
            TogglePlay();
        }

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float pad = 16.0f;
        const ImVec2 min(origin.x + pad, origin.y + pad);
        const ImVec2 max(origin.x + avail.x - pad, origin.y + avail.y - pad);
        if (max.x - min.x < 8.0f || max.y - min.y < 8.0f) return;
        const float width = max.x - min.x;

        // Click or drag anywhere on it to scrub.
        ImGui::SetCursorScreenPos(min);
        ImGui::InvisibleButton("##timeline", ImVec2(width, max.y - min.y));
        if (ImGui::IsItemActive()) {
            const float t = std::clamp((ImGui::GetIO().MousePos.x - min.x) / width, 0.0f, 1.0f);
            SeekTo(t * duration);
        }
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        const auto& palette = Editor::Palette();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(min, max, palette.ToU32(palette.Surface0), 4.0f);
        drawList->AddRect(min, max, palette.ToU32(palette.Border), 4.0f);

        // The waveform, one column per pixel; what's played so far is in the sound color.
        const float playheadX = min.x + width * std::clamp(Position() / duration, 0.0f, 1.0f);
        const float midY = (min.y + max.y) * 0.5f, halfHeight = (max.y - min.y) * 0.5f - 6.0f;
        ImVec4 dim = palette.AssetSound;
        dim.w = 0.4f;
        const ImU32 played = palette.ToU32(palette.AssetSound), unplayed = palette.ToU32(dim);
        const int columns = (int)width;
        for (int x = 0; x < columns; x++) {
            const size_t first = (size_t)x * peaks_.size() / columns;
            const size_t last = std::max(first + 1, (size_t)(x + 1) * peaks_.size() / columns);
            float lo = 0.0f, hi = 0.0f;
            for (size_t i = first; i < last && i < peaks_.size(); i++) {
                lo = std::min(lo, peaks_[i].lo);
                hi = std::max(hi, peaks_[i].hi);
            }
            const float px = min.x + x + 0.5f;
            drawList->AddLine(ImVec2(px, midY - hi * halfHeight), ImVec2(px, midY - lo * halfHeight + 1.0f),
                              px <= playheadX ? played : unplayed);
        }
        drawList->AddLine(ImVec2(min.x, midY), ImVec2(max.x, midY), palette.ToU32(palette.Border));
        drawList->AddLine(ImVec2(playheadX, min.y), ImVec2(playheadX, max.y), palette.ToU32(palette.Text), 2.0f);
    }

    // Nothing to edit, so Save As is a copy of the file.
    bool SaveAs(const std::string& fullPath) override {
        std::error_code ec;
        return std::filesystem::copy_file(fullPath_, fullPath, ec) && !ec;
    }

   private:
    struct Peak {
        float lo = 0.0f, hi = 0.0f;
    };

    Services::IAudioService& Player() { return services_.Get<Services::IAudioService>(); }
    const Sound* GetSound() { return services_.Get<Services::IAssetService>().Get<Sound>(path_); }

    // The sound's playback, forgetting it once it has finished (the playhead goes back to
    // the start).
    std::optional<Services::IAudioService::Playback> Poll() {
        if (id_ == Services::InvalidSound) return std::nullopt;
        auto playback = Player().GetPlayback(id_);
        if (playback) {
            position_ = playback->position;
        } else {
            id_ = Services::InvalidSound;
            position_ = 0.0f;
        }
        return playback;
    }

    float Position() {
        Poll();
        return position_;
    }

    void TogglePlay() {
        if (auto playback = Poll()) {
            Player().SetPaused(id_, !playback->paused);
            return;
        }
        StartAt(position_, false);
    }

    void StopPlayback() {
        if (id_ != Services::InvalidSound) Player().Stop(id_);
        id_ = Services::InvalidSound;
        position_ = 0.0f;
    }

    // Scrubbing a sound that isn't playing starts it paused there, so Play carries on from it.
    void SeekTo(float seconds) {
        position_ = seconds;
        if (Poll()) Player().Seek(id_, seconds);
        else StartAt(seconds, true);
    }

    void StartAt(float seconds, bool paused) {
        auto& audio = Player();
        id_ = audio.Play(path_, 1.0f, loop_);
        audio.SetPaused(id_, paused);
        audio.Seek(id_, seconds);
        position_ = seconds;
    }

    // Min/max of the (mono-mixed) samples in a fixed number of buckets, so drawing costs the
    // same however long the clip is.
    void BuildPeaks(const Sound& sound) {
        constexpr size_t Buckets = 4096;
        const std::vector<float>& samples = *sound.samples;
        const size_t count = std::min<size_t>(Buckets, sound.frameCount);
        peaks_.assign(count, Peak{});
        for (size_t b = 0; b < count; b++) {
            const size_t first = b * sound.frameCount / count, last = (b + 1) * sound.frameCount / count;
            Peak peak{1.0f, -1.0f};
            for (size_t f = first; f < last; f++) {
                const float v = (samples[f * 2] + samples[f * 2 + 1]) * 0.5f;
                peak.lo = std::min(peak.lo, v);
                peak.hi = std::max(peak.hi, v);
            }
            peaks_[b] = peak;
        }
    }

    ServiceLocator& services_;
    std::string fullPath_;
    Path path_;
    std::vector<Peak> peaks_;
    Services::SoundId id_ = Services::InvalidSound;  // the preview's sound, while it's playing or paused
    float position_ = 0.0f;   // seconds; where Play starts from when nothing is playing
    bool loop_ = false;
};

}  // namespace

std::unique_ptr<ContentPane> MakeSoundPane(ServiceLocator& services, const EditorDocument& document) {
    return std::make_unique<SoundPane>(services, document);
}

}  // namespace Elysium
