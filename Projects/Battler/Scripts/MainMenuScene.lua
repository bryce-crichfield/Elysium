---@type SceneScript
-- Main menu: the title and the way into everything else. The layout is Scenes/MainMenu.xml
-- (Button placements and text entities); this only gives the buttons their actions.
local Music = require("Scripts/Menu/Music")
local Settings = require("Scripts/Menu/Settings")
local Widgets = require("Scripts/Components/Widgets")
local Run = require("Scripts/Battler/Run")

local MainMenu = {}

function MainMenu:Initialize()
    Music.Play(Music.MENU)
    Settings.Apply()  -- the volume settings, not the engine's default of 1
    self.time = 0
    -- An adventure starts in the Town; Skirmish and Versus are one-off battles outside one.
    Widgets.BindButton("Adventure", function() Run.New() SceneReplace("Town") end)
    Widgets.BindButton("Skirmish", function() Run.Clear() SceneReplace("Battle") end)
    Widgets.BindButton("Versus", function() Run.Clear() SceneReplace("Lobby") end)
    Widgets.BindButton("Settings", function() SceneReplace("Settings") end)
    Widgets.BindButton("Quit", function() Quit() end)
end

function MainMenu:Update(dt)
    Widgets.CenterLayout("Background")
    self.time = self.time + dt
    -- The title's slow golden pulse.
    local glow = math.floor(200 + 55 * math.sin(self.time * 1.5))
    Widgets.SetText("Title", nil, {r = 255, g = glow, b = 110, a = 255})
end

function MainMenu:Render() end

function MainMenu:OnEvent(event)
    return false
end

return MainMenu
