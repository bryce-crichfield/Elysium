#pragma once

#include <raylib.h>
#include <string>
#include <vector>
#include <memory>

namespace Elysium::Services {

struct AudioTrack {
    std::string name;
    std::string assetName;  // References AssetService asset
    float volume = 1.0f;
    bool isLooping = false;
    bool isMuted = false;
    bool isPlaying = false;
    bool shouldFadeOut = false;
    float fadeSpeed = 1.0f;
    Music music = {0};
    Sound sound = {0};
    bool isMusic = true;  // true for Music, false for Sound
    bool isLoaded = false;
};

class JukeboxService {
public:
    JukeboxService();
    ~JukeboxService();
    
    // Disable copy/move to avoid issues with unique_ptr vectors
    JukeboxService(const JukeboxService&) = delete;
    JukeboxService& operator=(const JukeboxService&) = delete;
    JukeboxService(JukeboxService&&) = delete;
    JukeboxService& operator=(JukeboxService&&) = delete;
    
    // Core functionality
    void Update();
    void OnDebugDraw();
    
    // Track management
    bool CreateTrack(const std::string& trackName, const std::string& assetName, bool isMusic = true);
    void RemoveTrack(const std::string& trackName);
    
    // Track control
    void PlayTrack(const std::string& name);
    void StopTrack(const std::string& name);
    void PauseTrack(const std::string& name);
    void ResumeTrack(const std::string& name);
    void SeekTrack(const std::string& name, float timeInSeconds);
    void SetTrackVolume(const std::string& name, float volume);
    void SetTrackLoop(const std::string& name, bool loop);
    void MuteTrack(const std::string& name, bool mute = true);
    
    // Track queries
    bool IsTrackPlaying(const std::string& name) const;
    bool IsTrackPaused(const std::string& name) const;
    float GetTrackLength(const std::string& name) const;
    float GetTrackPosition(const std::string& name) const;
    
    // Global controls
    void SetMasterVolume(float volume);
    float GetMasterVolume() const { return masterVolume_; }
    void StopAll();
    void PauseAll();
    void ResumeAll();
    
    // Legacy compatibility
    bool LoadSong(const std::string& xmlPath) { return false; } // Deprecated
    void Play() {} // Deprecated
    void Stop() { StopAll(); }
    void Clear(float fadeOutSeconds = 0.0f) { StopAll(); }
    bool IsPlaying() const;
    const char* GetCurrentSongId() const { return "jukebox"; }
    void SetGlobalEnergy(float energy) { SetMasterVolume(energy); }
    float GetGlobalEnergy() const { return GetMasterVolume(); }
    
private:
    void UpdateTrack(AudioTrack& track);
    AudioTrack* FindTrack(const std::string& name);
    const AudioTrack* FindTrack(const std::string& name) const;
    
    std::vector<std::unique_ptr<AudioTrack>> tracks_;
    float masterVolume_ = 1.0f;
    
    // Debug interface
    bool showDebugWindow_ = false;
};

} // namespace Elysium::Services