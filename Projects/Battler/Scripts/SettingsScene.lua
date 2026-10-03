---@type SceneScript
-- Settings: the mixer channels' volumes and antialiasing. The sliders and checkbox
-- are prefab placements in Scenes/Settings.xml, bound here by their placement ids.
local Ui = require("Scripts/Menu/Ui")
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
    self.time = 0
    BindVolume("MasterVolume", "masterVolume")
    BindVolume("MusicVolume", "musicVolume")
    BindVolume("EffectsVolume", "effectsVolume")
    BindVolume("AmbientVolume", "ambientVolume")
    BindVolume("DialogueVolume", "dialogueVolume")
    Widgets.Bind("Msaa", IsMsaaEnabled, SetMsaaEnabled)
    self.widgets = {
        Ui.Button("Back", Ui.W / 2 - 120, 620, 240, 52, function() SceneReplace("MainMenu") end),
    }
end

function SettingsScene:Update(dt)
    self.time = self.time + dt
    Ui.Update(self.widgets)
end

function SettingsScene:Render()
    Ui.Background()
    Ui.Title("Settings", Ui.W / 2, 90, 72, Ui.COLORS.gold)
    Ui.Draw(self.widgets, self.time)
end

function SettingsScene:OnEvent(event)
    if event.type == "KeyPressed" and event.key == KEY_ESCAPE then
        Sfx.Play(Sfx.CANCEL)
        SceneReplace("MainMenu")
        return true
    end
    return false
end

return SettingsScene
