-- Player settings, shared by every scene: a required module is loaded once, so this table
-- outlives scene changes. Not saved to disk yet. Volumes are 0..1, one per mixer channel.
local Settings = {
    masterVolume = 0.8,
    musicVolume = 0.7,
    effectsVolume = 0.8,
    ambientVolume = 0.8,
    dialogueVolume = 1.0,
    joinAddress = "127.0.0.1",   -- the Versus lobby's address box
}

-- Each volume setting and the mixer channel it drives.
Settings.CHANNELS = {
    masterVolume = CHANNEL_MASTER,
    musicVolume = CHANNEL_MUSIC,
    effectsVolume = CHANNEL_EFFECTS,
    ambientVolume = CHANNEL_AMBIENT,
    dialogueVolume = CHANNEL_DIALOGUE,
}

-- Sets a volume and its channel.
function Settings.SetVolume(key, v)
    Settings[key] = v
    SetChannelVolume(Settings.CHANNELS[key], v)
end

-- Pushes every volume to the mixer (the engine starts every channel at 1).
function Settings.Apply()
    for key, channel in pairs(Settings.CHANNELS) do SetChannelVolume(channel, Settings[key]) end
end

return Settings
