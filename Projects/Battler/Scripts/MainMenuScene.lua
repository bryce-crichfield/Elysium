---@type SceneScript
-- Main menu: the title and the way into everything else.
local Ui = require("Scripts/Menu/Ui")
local Music = require("Scripts/Menu/Music")
local Settings = require("Scripts/Menu/Settings")

local MainMenu = {}

function MainMenu:Initialize()
    Music.Play(Music.MENU)
    Settings.Apply()  -- the volume settings, not the engine's default of 1
    self.time = 0
    self.widgets = Ui.Column(Ui.W / 2, 300, 320, 56, 16, {
        { "Adventure", function() SceneReplace("Overworld") end },
        { "Skirmish", function() SceneReplace("BattleFree") end },
        { "Versus", function() SceneReplace("Lobby") end },
        { "Settings", function() SceneReplace("Settings") end },
        { "Quit", function() Quit() end },
    })
end

function MainMenu:Update(dt)
    self.time = self.time + dt
    Ui.Update(self.widgets)
end

function MainMenu:Render()
    Ui.Background()
    local glow = math.floor(200 + 55 * math.sin(self.time * 1.5))
    Ui.Title("Elysium", Ui.W / 2, 110, 110, {r = 255, g = glow, b = 110, a = 255})
    Ui.Title("Tactics", Ui.W / 2, 220, 36, Ui.COLORS.dim)
    Ui.Draw(self.widgets, self.time)
    Ui.Text("v0.1 demo", 16, Ui.H - 32, 18, Ui.COLORS.off)
end

function MainMenu:OnEvent(event)
    return false
end

return MainMenu
