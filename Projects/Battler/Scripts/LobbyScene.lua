---@type SceneScript
-- Versus lobby: host a game or join one at an address. As soon as the two connect, both go to
-- the battle, which finds the connection open and plays versus. The layout is Scenes/Lobby.xml
-- (the Address field, Button placements, the title and the Status line).
local Music = require("Scripts/Menu/Music")
local Net = require("Scripts/Battler/Net")
local Settings = require("Scripts/Menu/Settings")
local Sfx = require("Scripts/Menu/Sfx")
local Widgets = require("Scripts/Components/Widgets")

local Lobby = {}

function Lobby:Initialize()
    Music.Play(Music.MENU)
    self.time = 0
    self.status, self.state = "Host a game, or join one", "idle"
    local idle = function() return self.state == "idle" end
    Widgets.ResetHover()
    Widgets.Bind("Address", function() return Settings.joinAddress end, function(v) Settings.joinAddress = v end)
    Widgets.BindButton("Host", function() self:Host() end, idle)
    Widgets.BindButton("Join", function() self:Join() end, idle)
    Widgets.BindButton("Cancel", function() self:Cancel() end, function() return self.state ~= "idle" end)
    Widgets.BindButton("Back", function() self:Back() end)
end

function Lobby:Host()
    if Net.Host() then
        self.state, self.status = "hosting", "Waiting for an opponent on port " .. Net.PORT
    else
        self.status = "Couldn't host on port " .. Net.PORT
    end
end

function Lobby:Join()
    local address = Settings.joinAddress
    if address == "" then
        self.status = "Type the host's address first"
    elseif Net.Join(address) then
        self.state, self.status = "joining", "Connecting to " .. address
    else
        self.status = "Couldn't reach " .. address
    end
end

function Lobby:Cancel()
    Net.Stop()
    self.state, self.status = "idle", "Host a game, or join one"
end

function Lobby:Back()
    if Net.Active() then Net.Stop() end
    SceneReplace("MainMenu")
end

function Lobby:Update(dt)
    Widgets.CenterLayout("Background")
    self.time = self.time + dt
    for _, e in ipairs(Net.Poll()) do
        if e.type == "connected" then
            -- Both sides hear this; the battle reads the open connection and starts versus.
            SceneReplace("Battle")
            return
        elseif e.type == "disconnected" and self.state == "joining" then
            Net.Stop()
            self.state, self.status = "idle", "Couldn't connect to " .. Settings.joinAddress
        end
    end
end

function Lobby:Render()
    Widgets.SetText("Status", self.status)
end

function Lobby:OnEvent(event)
    if event.type ~= "KeyPressed" then return false end
    if Widgets.Typing() then return true end  -- the Address field has the keyboard
    if event.key == KEY_ESCAPE then
        Sfx.Play(Sfx.CANCEL)
        self:Back()
        return true
    end
    return false
end

return Lobby
