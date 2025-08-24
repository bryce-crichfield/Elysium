#include "Services/JukeboxService.h"
#include "Services/LogService.h"
#include "Services/AssetService.h"
#include "Application.h"
#include "Asset.h"
#include "imgui.h"
#include <algorithm>

namespace Elysium::Services {

JukeboxService::JukeboxService() {
    name_ = "JukeboxService";
    LOG_INFO("JukeboxService", "Service created successfully");
}

JukeboxService::~JukeboxService() {
    StopAll();
    tracks_.clear();
    LOG_INFO("JukeboxService", "Service destroyed successfully");
}

void JukeboxService::Initialize() {
    isVisible_ = false;
}

void JukeboxService::Shutdown() {
    StopAll();
    tracks_.clear();
}

void JukeboxService::Update(float deltaTime) {
    for (auto& track : tracks_) {
        UpdateTrack(*track);
    }
}

void JukeboxService::UpdateTrack(AudioTrack& track) {
    auto& assetService = Application::GetInstance().GetService<Elysium::Services::AssetService>("AssetService");

    // Ensure track is loaded from AssetService
    if (!track.isLoaded && assetService.IsAssetLoaded(track.assetName)) {
        if (track.isMusic) {
            track.music = assetService.GetMusic(track.assetName);
        } else {
            track.sound = assetService.GetSound(track.assetName);
        }
        track.isLoaded = true;
        LOG_INFOF("JukeboxService", "Loaded track '%s' from asset '%s'",
                track.name.c_str(), track.assetName.c_str());
    }

    if (!track.isLoaded) return;

    // Handle fade out
    if (track.shouldFadeOut) {
        track.volume -= track.fadeSpeed * GetFrameTime();
        if (track.volume <= 0.0f) {
            track.volume = 0.0f;
            track.shouldFadeOut = false;
            StopTrack(track.name);
            return;
        }
    }

    // Update music streams and apply volume
    if (track.isPlaying) {
        float finalVolume = track.volume * masterVolume_;
        if (track.isMuted) finalVolume = 0.0f;

        if (track.isMusic) {
            UpdateMusicStream(track.music);
            SetMusicVolume(track.music, finalVolume);

            // Handle looping
            if (track.isLooping && !IsMusicStreamPlaying(track.music)) {
                PlayMusicStream(track.music);
            }
        } else {
            SetSoundVolume(track.sound, finalVolume);
        }
    }
}

bool JukeboxService::CreateTrack(const std::string& trackName, const std::string& assetName, bool isMusic) {
    // Check if track already exists
    if (FindTrack(trackName) != nullptr) {
        LOG_WARNINGF("JukeboxService", "Track '%s' already exists", trackName.c_str());
        return false;
    }

    // Check if asset exists
    auto& assetService = Application::GetInstance().GetService<Elysium::Services::AssetService>("AssetService");
    if (!assetService.IsAssetLoaded(assetName)) {
        LOG_ERRORF("JukeboxService", "Asset '%s' not found in AssetService", assetName.c_str());
        return false;
    }

    auto track = std::make_unique<AudioTrack>();
    track->name = trackName;
    track->assetName = assetName;
    track->isMusic = isMusic;

    tracks_.push_back(std::move(track));
    LOG_INFOF("JukeboxService", "Created track '%s' using asset '%s'", trackName.c_str(), assetName.c_str());
    return true;
}

void JukeboxService::RemoveTrack(const std::string& trackName) {
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
        [&](const auto& track) { return track->name == trackName; });

    if (it != tracks_.end()) {
        StopTrack(trackName);
        tracks_.erase(it);
        LOG_INFOF("JukeboxService", "Removed track '%s'", trackName.c_str());
    }
}

void JukeboxService::PlayTrack(const std::string& name) {
    AudioTrack* track = FindTrack(name);
    if (!track || !track->isLoaded) return;

    if (track->isMusic) {
        PlayMusicStream(track->music);
    } else {
        PlaySound(track->sound);
    }
    track->isPlaying = true;
    track->shouldFadeOut = false;
}

void JukeboxService::StopTrack(const std::string& name) {
    AudioTrack* track = FindTrack(name);
    if (!track || !track->isLoaded) return;

    if (track->isMusic) {
        StopMusicStream(track->music);
    } else {
        StopSound(track->sound);
    }
    track->isPlaying = false;
}

void JukeboxService::PauseTrack(const std::string& name) {
    AudioTrack* track = FindTrack(name);
    if (!track || !track->isLoaded) return;

    if (track->isMusic) {
        PauseMusicStream(track->music);
    } else {
        PauseSound(track->sound);
    }
}

void JukeboxService::ResumeTrack(const std::string& name) {
    AudioTrack* track = FindTrack(name);
    if (!track || !track->isLoaded) return;

    if (track->isMusic) {
        ResumeMusicStream(track->music);
    } else {
        ResumeSound(track->sound);
    }
}

void JukeboxService::SeekTrack(const std::string& name, float timeInSeconds) {
    AudioTrack* track = FindTrack(name);
    if (!track || !track->isLoaded || !track->isMusic) return;

    SeekMusicStream(track->music, timeInSeconds);
}

void JukeboxService::SetTrackVolume(const std::string& name, float volume) {
    AudioTrack* track = FindTrack(name);
    if (!track) return;

    track->volume = std::clamp(volume, 0.0f, 1.0f);
}

void JukeboxService::SetTrackLoop(const std::string& name, bool loop) {
    AudioTrack* track = FindTrack(name);
    if (!track) return;

    track->isLooping = loop;
}

void JukeboxService::MuteTrack(const std::string& name, bool mute) {
    AudioTrack* track = FindTrack(name);
    if (!track) return;

    track->isMuted = mute;
}

bool JukeboxService::IsTrackPlaying(const std::string& name) const {
    const AudioTrack* track = FindTrack(name);
    if (!track || !track->isLoaded) return false;

    if (track->isMusic) {
        return IsMusicStreamPlaying(track->music);
    } else {
        return IsSoundPlaying(track->sound);
    }
}

bool JukeboxService::IsTrackPaused(const std::string& name) const {
    const AudioTrack* track = FindTrack(name);
    if (!track || !track->isLoaded || !track->isMusic) return false;

    return !IsMusicStreamPlaying(track->music) && track->isPlaying;
}

float JukeboxService::GetTrackLength(const std::string& name) const {
    const AudioTrack* track = FindTrack(name);
    if (!track || !track->isLoaded || !track->isMusic) return 0.0f;

    return GetMusicTimeLength(track->music);
}

float JukeboxService::GetTrackPosition(const std::string& name) const {
    const AudioTrack* track = FindTrack(name);
    if (!track || !track->isLoaded || !track->isMusic) return 0.0f;

    return GetMusicTimePlayed(track->music);
}

void JukeboxService::SetMasterVolume(float volume) {
    masterVolume_ = std::clamp(volume, 0.0f, 1.0f);
}

void JukeboxService::StopAll() {
    for (auto& track : tracks_) {
        if (track->isPlaying) {
            StopTrack(track->name);
        }
    }
}

void JukeboxService::PauseAll() {
    for (auto& track : tracks_) {
        if (track->isPlaying) {
            PauseTrack(track->name);
        }
    }
}

void JukeboxService::ResumeAll() {
    for (auto& track : tracks_) {
        if (track->isPlaying) {
            ResumeTrack(track->name);
        }
    }
}

bool JukeboxService::IsPlaying() const {
    return std::any_of(tracks_.begin(), tracks_.end(),
        [](const auto& track) { return track->isPlaying; });
}

AudioTrack* JukeboxService::FindTrack(const std::string& name) {
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
        [&](const auto& track) { return track->name == name; });
    return it != tracks_.end() ? it->get() : nullptr;
}

const AudioTrack* JukeboxService::FindTrack(const std::string& name) const {
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
        [&](const auto& track) { return track->name == name; });
    return it != tracks_.end() ? it->get() : nullptr;
}


void JukeboxService::OnDebugDraw() {

        // Global Controls
        ImGui::Text("Global Controls");
        ImGui::Separator();

        if (ImGui::Button("Stop All")) StopAll();
        ImGui::SameLine();
        if (ImGui::Button("Pause All")) PauseAll();
        ImGui::SameLine();
        if (ImGui::Button("Resume All")) ResumeAll();

        ImGui::SliderFloat("Master Volume", &masterVolume_, 0.0f, 1.0f);

        // Available Assets Section
        ImGui::Spacing();
        ImGui::Text("Available Assets (from AssetService)");
        ImGui::Separator();

        auto& assetService = Application::GetInstance().GetService<Elysium::Services::AssetService>("AssetService");
        static char newTrackName[64] = "";
        static char selectedAsset[64] = "";
        static bool isMusic = false;

        // List available assets from AssetService
        const auto& allAssets = assetService.GetAllAssets();

        if (allAssets.empty()) {
            ImGui::Text("No assets loaded yet. Load a scene to see available assets.");
        } else {
            // Create filtered list - only show SOUND and MUSIC assets
            std::vector<std::pair<AssetName, const Asset*>> audioAssets;
            for (const auto& [assetName, asset] : allAssets) {
                if (asset.IsLoaded()) { // Only show loaded assets
                    AssetType type = asset.GetType();
                    if (type == AssetType::SOUND || type == AssetType::MUSIC) {
                        audioAssets.emplace_back(assetName, &asset);
                    }
                }
            }

            // Sort audio assets: MUSIC first, then SOUND, then by name
            std::sort(audioAssets.begin(), audioAssets.end(),
                [](const auto& a, const auto& b) {
                    AssetType typeA = a.second->GetType();
                    AssetType typeB = b.second->GetType();

                    // MUSIC before SOUND
                    if (typeA != typeB) {
                        if (typeA == AssetType::MUSIC) return true;
                        if (typeB == AssetType::MUSIC) return false;
                    }

                    // Within same type, sort by name
                    return a.first < b.first;
                });

            if (audioAssets.empty()) {
                ImGui::Text("No audio assets (SOUND/MUSIC) found. Load scenes with audio files.");
            } else {
                ImGui::Text("Audio Assets - Click to select for track creation:");
                ImGui::Columns(3, "AssetColumns", true);
                ImGui::Text("Asset Name");
                ImGui::NextColumn();
                ImGui::Text("Type");
                ImGui::NextColumn();
                ImGui::Text("Action");
                ImGui::NextColumn();
                ImGui::Separator();

                for (const auto& [assetName, assetPtr] : audioAssets) {
                    const Asset& asset = *assetPtr;

                    ImGui::Text("%s", assetName.c_str());
                    ImGui::NextColumn();

                    // Show asset type (we know it's audio)
                    const char* typeStr = (asset.GetType() == AssetType::MUSIC) ? "MUSIC" : "SOUND";
                    ImGui::Text("%s", typeStr);
                    ImGui::NextColumn();

                    // Select button
                    std::string buttonLabel = "Select##" + assetName;
                    if (ImGui::Button(buttonLabel.c_str())) {
                        strcpy(selectedAsset, assetName.c_str());
                        // Auto-set the checkbox based on asset type
                        isMusic = (asset.GetType() == AssetType::MUSIC);
                    }
                    ImGui::NextColumn();
                }
            }

            ImGui::Columns(1);
        }

        // Track Creation
        ImGui::Spacing();
        ImGui::Text("Create New Track");
        ImGui::Separator();

        ImGui::InputText("Track Name", newTrackName, sizeof(newTrackName));
        ImGui::InputText("Asset Name", selectedAsset, sizeof(selectedAsset));
        ImGui::Checkbox("Create as Music (unchecked = Sound)", &isMusic);

        if (ImGui::Button("Create Track")) {
            if (strlen(newTrackName) > 0 && strlen(selectedAsset) > 0) {
                if (CreateTrack(newTrackName, selectedAsset, isMusic)) {
                    newTrackName[0] = '\0';
                    selectedAsset[0] = '\0';
                }
            }
        }

        // Active Tracks Section
        ImGui::Spacing();
        ImGui::Text("Active Tracks (%zu)", tracks_.size());
        ImGui::Separator();

        for (auto& track : tracks_) {
            ImGui::PushID(track->name.c_str());

            // Track Header
            ImGui::Text("Track: %s", track->name.c_str());
            ImGui::SameLine();
            ImGui::TextColored(track->isPlaying ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                              track->isPlaying ? "PLAYING" : "STOPPED");
            ImGui::SameLine();
            ImGui::TextColored(track->isLoaded ? ImVec4(0,1,0,1) : ImVec4(1,1,0,1),
                              track->isLoaded ? "LOADED" : "LOADING...");

            ImGui::Text("Asset: %s (%s)", track->assetName.c_str(), track->isMusic ? "Music" : "Sound");

            if (track->isLoaded) {
                // Playback Controls
                if (ImGui::Button(track->isPlaying ? "■ Stop" : "▶ Play")) {
                    if (track->isPlaying) StopTrack(track->name);
                    else PlayTrack(track->name);
                }
                ImGui::SameLine();

                if (track->isMusic) {
                    if (ImGui::Button("⏸ Pause")) PauseTrack(track->name);
                    ImGui::SameLine();
                    if (ImGui::Button("⏵ Resume")) ResumeTrack(track->name);
                    ImGui::SameLine();
                }

                // Delete button (prominent placement)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("🗑 Delete")) {
                    RemoveTrack(track->name);
                    ImGui::PopStyleColor(2);
                    ImGui::PopID();
                    break; // Exit loop since we modified the container
                }
                ImGui::PopStyleColor(2);

                // Track Properties
                bool loop = track->isLooping;
                if (ImGui::Checkbox("Loop", &loop)) {
                    SetTrackLoop(track->name, loop);
                }
                ImGui::SameLine();

                bool mute = track->isMuted;
                if (ImGui::Checkbox("Mute", &mute)) {
                    MuteTrack(track->name, mute);
                }

                // Volume Control
                float volume = track->volume;
                if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
                    SetTrackVolume(track->name, volume);
                }

                // Timeline Scrubber (for all tracks, since Sounds can have length too)
                if (track->isMusic) {
                    float length = GetTrackLength(track->name);
                    float position = GetTrackPosition(track->name);

                    // Timeline display
                    ImGui::Text("Timeline: %.1fs / %.1fs", position, length);

                    if (length > 0.0f) {
                        // Make the timeline scrubber wider and more prominent
                        ImGui::PushItemWidth(-1); // Full width
                        float seekPos = position;
                        if (ImGui::SliderFloat("##Timeline", &seekPos, 0.0f, length, "%.1fs")) {
                            SeekTrack(track->name, seekPos);
                        }
                        ImGui::PopItemWidth();

                        // Quick jump buttons
                        if (ImGui::Button("Start")) SeekTrack(track->name, 0.0f);
                        ImGui::SameLine();
                        if (ImGui::Button("25%")) SeekTrack(track->name, length * 0.25f);
                        ImGui::SameLine();
                        if (ImGui::Button("50%")) SeekTrack(track->name, length * 0.5f);
                        ImGui::SameLine();
                        if (ImGui::Button("75%")) SeekTrack(track->name, length * 0.75f);
                        ImGui::SameLine();
                        if (ImGui::Button("End")) SeekTrack(track->name, length);
                    } else {
                        ImGui::Text("Timeline: Not available (length unknown)");
                    }
                } else {
                    // For Sound tracks, show basic playback info
                    ImGui::Text("Timeline: Sound effect (no scrubbing available)");
                }
            }

            ImGui::PopID();
            ImGui::Separator();
        }

        if (tracks_.empty()) {
            ImGui::Text("No tracks created. Use the controls above to create tracks from loaded assets.");
        }
}

} // namespace Elysium::Services
