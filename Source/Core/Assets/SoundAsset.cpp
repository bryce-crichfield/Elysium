#include "Core/Assets/SoundAsset.h"
#include "Core/Asset.h"
#include "Services/LogService.h"

namespace Elysium {

bool SoundAsset::Load() {
    auto samples = std::make_shared<std::vector<float>>();
    if (!Audio::DecodeFile(GetPath().GetFullPath(), *samples)) {
        LOG_ERRORF("SoundAsset", "Failed to decode sound: %s", GetPath().c_str());
        return false;
    }

    sound_.frameCount = (uint32_t)(samples->size() / Audio::Channels);
    sound_.samples = std::move(samples);
    SetLoaded(true);
    LOG_DEBUGF("SoundAsset", "Sound loaded: %s (%u frames)", GetPath().c_str(), sound_.frameCount);
    return true;
}

void SoundAsset::Unload() {
    sound_ = Sound{};  // a voice still playing it holds its own reference to the samples
    SetLoaded(false);
}

REGISTER_ASSET_TYPE(Sound, SoundAsset);

}  // namespace Elysium
