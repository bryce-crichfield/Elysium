---@type SceneScript
-- Versus lobby: host a game or join one at an address. As soon as the two connect, both go to
-- the battle, which finds the connection open and plays versus.
local Ui = require("Scripts/Menu/Ui")
local Music = require("Scripts/Menu/Music")
local Net = require("Scripts/Battler/Net")
local Settings = require("Scripts/Menu/Settings")
local Sfx = require("Scripts/Menu/Sfx")

local Lobby = {}

function Lobby:Initialize()
    Music.Play(Music.MENU)
    self.time = 0
    self.status, self.state = "Host a game, or join one", "idle"
    local x = Ui.W / 2 - 220
    local idle = function() return self.state == "idle" end
    self.widgets = {
        Ui.Field("Address", x, 250, 440,
            function() return Settings.joinAddress end, function(v) Settings.joinAddress = v end),
        Ui.Button("Host", x, 330, 210, 56, function() self:Host() end, idle),
        Ui.Button("Join", x + 230, 330, 210, 56, function() self:Join() end, idle),
        Ui.Button("Cancel", x, 410, 440, 52, function() self:Cancel() end,
            function() return self.state ~= "idle" end),
        Ui.Button("Back", Ui.W / 2 - 120, 580, 240, 52, function() self:Back() end),
    }
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
    self.time = self.time + dt
    for _, e in ipairs(Net.Poll()) do
        if e.type == "connected" then
            -- Both sides hear this; the battle reads the open connection and starts versus.
            SceneReplace("BattleFree")
            return
        elseif e.type == "disconnected" and self.state == "joining" then
            Net.Stop()
            self.state, self.status = "idle", "Couldn't connect to " .. Settings.joinAddress
        end
    end
    Ui.Update(self.widgets)
end

function Lobby:Render()
    Ui.Background()
    Ui.Title("Versus", Ui.W / 2, 90, 72, Ui.COLORS.gold)
    Ui.Draw(self.widgets, self.time)
    Ui.Title(self.status, Ui.W / 2, 490, 26, Ui.COLORS.text)
end

function Lobby:OnEvent(event)
    if event.type ~= "KeyPressed" then return false end
    if Ui.Key(self.widgets, event.key) then return true end
    if event.key == KEY_ESCAPE then
        Sfx.Play(Sfx.CANCEL)
        self:Back()
        return true
    end
    return false
end

return Lobby
