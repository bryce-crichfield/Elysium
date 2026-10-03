---@type SceneScript
-- Settings: volumes (stored for when audio lands) and antialiasing. The sliders and checkbox
-- are prefab placements in Scenes/Settings.xml, bound here by their placement ids.
local Ui = require("Scripts/Menu/Ui")
local Settings = require("Scripts/Menu/Settings")
local Widgets = require("Scripts/Components/Widgets")

local SettingsScene = {}

local function BindSetting(id, key)
    Widgets.Bind(id, function() return Settings[key] end, function(v) Settings[key] = v end)
end

function SettingsScene:Initialize()
    self.time = 0
    BindSetting("MasterVolume", "masterVolume")
    BindSetting("MusicVolume", "musicVolume")
    BindSetting("SfxVolume", "sfxVolume")
    Widgets.Bind("Msaa", IsMsaaEnabled, SetMsaaEnabled)
    self.widgets = {
        Ui.Button("Back", Ui.W / 2 - 120, 580, 240, 52, function() SceneReplace("MainMenu") end),
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
        SceneReplace("MainMenu")
        return true
    end
    return false
end

return SettingsScene
