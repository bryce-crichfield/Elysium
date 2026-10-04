---@type SceneScript
-- Campfire: the rest between floors, where a run starts and each cleared floor ends. The
-- party heals here (later: level up, trade, manage the inventory). Descend deals the next
-- floor and goes down to the Dungeon. Arriving with no run (the last one was lost) starts a
-- new one. The layout is Scenes/Campfire.xml.
local Music = require("Scripts/Menu/Music")
local Run = require("Scripts/Battler/Run")
local Sfx = require("Scripts/Menu/Sfx")
local Widgets = require("Scripts/Components/Widgets")

local Campfire = {}

function Campfire:Initialize()
    Music.Play(Music.MENU)
    Widgets.ResetHover()
    local fresh = not Run.Active()
    if fresh then Run.New() end
    local cleared = Run.Cleared() > 0
    if cleared then
        self.status = string.format("Floor %d cleared. The party rests.", Run.Floor())
    elseif fresh and Run.Floor() == 1 then
        self.status = "A new run begins. The dungeon waits below."
    else
        self.status = string.format("Floor %d of %d lies below.", Run.Floor(), Run.FLOORS)
    end
    Widgets.BindButton("Descend", function() self:Descend() end)
    Widgets.BindButton("Abandon", function() Run.Clear() SceneReplace("MainMenu") end)
end

function Campfire:Descend()
    if not Run.Active() then Run.New() end
    if Run.Descend() then
        SceneReplace("Dungeon")
    else
        -- All nine floors: the dungeon is beaten.
        Run.Clear()
        self.status = "The dungeon is beaten! Descend to start a new run."
    end
end

function Campfire:Update(dt) end

function Campfire:Render()
    Widgets.SetText("Status", self.status)
end

function Campfire:OnEvent(event)
    if event.type == "KeyPressed" and event.key == KEY_ESCAPE then
        Sfx.Play(Sfx.CANCEL)
        Run.Clear()
        SceneReplace("MainMenu")
        return true
    end
    return false
end

return Campfire
