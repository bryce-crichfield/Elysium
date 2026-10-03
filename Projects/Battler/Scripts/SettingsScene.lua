---@type SceneScript
-- Settings: the mixer channels' volumes and antialiasing. The layout is Scenes/Settings.xml
-- (Slider, Checkbox and Button placements); this binds them by their placement ids.
local Music = require("Scripts/Menu/Music")
local Settings = require("Scripts/Menu/Settings")
local Widgets = require("Scripts/Components/Widgets")
local Sfx = require("Scripts/Menu/Sfx")

local SettingsScene = {}

local function BindVolume(id, key)
    Widgets.Bind(id, function() return Settings[key] end, function(v) Settings.SetVolume(key, v) end)
end

function SettingsScene:Initialize()
    Music.Play(Music.MENU)
    BindVolume("MasterVolume", "masterVolume")
    BindVolume("MusicVolume", "musicVolume")
    BindVolume("EffectsVolume", "effectsVolume")
    BindVolume("AmbientVolume", "ambientVolume")
    BindVolume("DialogueVolume", "dialogueVolume")
    Widgets.Bind("Msaa", IsMsaaEnabled, SetMsaaEnabled)
    Widgets.BindButton("Back", function() SceneReplace("MainMenu") end)
end

function SettingsScene:Update(dt) end

function SettingsScene:Render() end

function SettingsScene:OnEvent(event)
    if event.type == "KeyPressed" and event.key == KEY_ESCAPE then
        Sfx.Play(Sfx.CANCEL)
        SceneReplace("MainMenu")
        return true
    end
    return false
end

return SettingsScene
