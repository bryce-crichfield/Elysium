-- Player settings, shared by every scene: a required module is loaded once, so this table
-- outlives scene changes. Not saved to disk yet. Volumes are 0..1, for when audio lands.
local Settings = {
    masterVolume = 0.8,
    musicVolume = 0.7,
    sfxVolume = 0.8,
    joinAddress = "127.0.0.1",   -- the Versus lobby's address box
}

return Settings
