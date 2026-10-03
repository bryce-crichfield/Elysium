---@type SceneScript
-- Battler POC, free movement: the same skirmish as Battle.lua, but off the grid. A unit moves
-- anywhere within a walking budget on the navmesh (NavReach), and ranges and blasts are radii.
--   Left-click a blue-ringed unit, then anywhere in the blue area to move (or on the unit to
--   stay put). Then 1 Attack / 2 Spell / 3 Wait. Right-click / Esc backs out.
--   Space ends the player phase. Middle-drag pans, right-drag turns, wheel zooms (and tilts). R restarts.
-- Versus: the lobby hosts (H) or joins (J) a game. The host plays the blue team and moves first,
-- the joiner plays red. Every order goes across as a command the moment it's issued (a move,
-- undoing one, an action) and the other side replays it with the same rules, so both run the
-- same battle (lockstep); a phase
-- ends with { kind = "end" } and a checksum of the units, to catch the two drifting apart.
local Board = require("Scripts/Battler/Board")
local Units = require("Scripts/Battler/Units")
local Rules = require("Scripts/Battler/FreeRules")
local Net = require("Scripts/Battler/Net")

local Battle = {}

local PLAYER, ENEMY = Units.PLAYER, Units.ENEMY
local JOIN_ADDRESS = "127.0.0.1"  -- where J connects; the host's LAN / VPN address for two machines
local SCREEN_W, SCREEN_H = 1280, 720
local FONT = "Fonts/EnchantedLand-Regular.ttf"  -- every HUD line is drawn in it
local SPEED = 215          -- ground units a second
local CELL = 8             -- the navmesh's cellSize (BattleFree.xml), for drawing reach runs
local COS = Board.PITCH_COS

-- Start spots, by the old board's tile coordinates.
local ROSTER = {
    { "Fighter", PLAYER, 1, 2 }, { "Archer", PLAYER, 2, 1 }, { "Mage", PLAYER, 1, 1 },
    { "Grunt", ENEMY, 9, 9 }, { "Poacher", ENEMY, 10, 7 }, { "Grunt", ENEMY, 7, 10 }, { "Hexer", ENEMY, 10, 10 },
}

local COLORS = {
    move = {r = 60, g = 140, b = 255, a = 60}, moveFar = {r = 60, g = 140, b = 255, a = 35},
    moveEdge = {r = 120, g = 190, b = 255, a = 200},
    attack = {r = 255, g = 60, b = 50, a = 45}, attackEdge = {r = 255, g = 110, b = 90, a = 200},
    spell = {r = 170, g = 90, b = 255, a = 40}, spellEdge = {r = 200, g = 150, b = 255, a = 200},
    splash = {r = 255, g = 170, b = 60, a = 90}, splashEdge = {r = 255, g = 200, b = 120, a = 220},
    target = {r = 255, g = 230, b = 120, a = 230}, bad = {r = 255, g = 80, b = 70, a = 200},
    path = {r = 255, g = 255, b = 255, a = 200}, ghost = {r = 255, g = 255, b = 255, a = 120},
    text = {r = 235, g = 235, b = 240, a = 255}, dim = {r = 160, g = 160, b = 175, a = 255},
    panel = {r = 14, g = 14, b = 22, a = 210}, gold = {r = 255, g = 210, b = 110, a = 255},
}

-- Health bar fills for the hero frames, by team.
local FRAME_FILLS = {
    [PLAYER] = {r = 70, g = 160, b = 255, a = 255},
    [ENEMY]  = {r = 235, g = 70, b = 60, a = 255},
}

-- --- Helpers ------------------------------------------------------------------------------

-- The HUD font everywhere unless a call names another.
local RawDrawText, RawMeasureText = DrawText, MeasureText
local function DrawText(text, x, y, size, color, layer, font) RawDrawText(text, x, y, size, color, layer, font or FONT) end
local function MeasureText(text, size, font) return RawMeasureText(text, size, font or FONT) end

local function Wait(seconds)
    local t = 0
    while t < seconds do t = t + coroutine.yield() end
end

local function Lerp(a, b, t) return a + (b - a) * t end
local function Smooth(t) return t * t * (3 - 2 * t) end

-- A circle of ground radius `r` around (x, y) at height z, drawn as lines (an ellipse on screen).
local function GroundCircle(x, y, z, r, color, layer)
    local cx, cy = Board.Lift(x, y, z)
    local n = math.max(24, math.floor(r / 6))
    local px, py
    for k = 0, n do
        local a = k / n * math.pi * 2
        local qx, qy = cx + math.cos(a) * r, cy + math.sin(a) * r * 0.5
        if px then DrawLine(px, py, qx, qy, color, layer or "overlay") end
        px, py = qx, qy
    end
end

local function GroundDisc(x, y, z, r, color, layer)
    Board.Disc(x, y, z, r, color, layer)
end

-- --- Lifecycle ----------------------------------------------------------------------------

function Battle:Initialize()
    self.time = 0
    self.board = Board.new()   -- only to know when the navmesh has settled
    self.state = "loading"
    self.loadTimer, self.loadChecks, self.lastCount = 0, 0, -1
    self.units = {}
    self.floaters = {}
    self.inbox = {}               -- the opponent's commands, applied in order by RemotePhase
    self.me, self.them = PLAYER, ENEMY
    self.net = false              -- a versus game (true) or solo against the AI
    Log("Battler (free): waiting for the navmesh...")
end

-- --- Lobby and network --------------------------------------------------------------------

function Battle:Lobby(sub)
    if Net.Active() then Net.Stop() end
    self.co, self.net, self.inbox = nil, false, {}
    self.state = "lobby"
    self:ShowBanner("H  Host     J  Join     Enter  Solo", sub)
end

function Battle:LobbyKey(key)
    if self.state == "lobby" then
        if key == KEY_H then
            if Net.Host() then
                self.state = "waiting"
                self:ShowBanner("Waiting for an opponent", "Port " .. Net.PORT .. "     Esc  Cancel")
            else
                self:ShowBanner("H  Host     J  Join     Enter  Solo", "Couldn't host on port " .. Net.PORT)
            end
        elseif key == KEY_J then
            if Net.Join(JOIN_ADDRESS) then
                self.state = "joining"
                self:ShowBanner("Connecting to " .. JOIN_ADDRESS, "Esc  Cancel")
            end
        elseif key == KEY_ENTER then
            self.me, self.them, self.net = PLAYER, ENEMY, false
            self.state = "battle"
            self:StartBattle()
        end
    elseif key == KEY_ESCAPE then
        self:Lobby()
    end
end

-- The host is blue and goes first; the joiner is red.
function Battle:BeginVersus(host)
    self.net = true
    self.me, self.them = host and PLAYER or ENEMY, host and ENEMY or PLAYER
    self.state = "battle"
    self.banner = nil
    self:StartBattle()
end

function Battle:Restart()
    self.co, self.inbox = nil, {}
    self.state = "battle"
    self.banner = nil
    self:StartBattle()
end

function Battle:PollNetwork()
    for _, e in ipairs(Net.Poll()) do
        if e.type == "connected" then
            if Net.IsHost() and self.state == "waiting" then
                Net.Send({ kind = "start" })
                self:BeginVersus(true)
            elseif self.state == "joining" then
                self:ShowBanner("Connected", "Waiting for the host...")
            end
        elseif e.type == "disconnected" then
            if self.state == "joining" then
                self:Lobby("Couldn't connect to " .. JOIN_ADDRESS)
            elseif self.net then
                self:Lobby("Your opponent left")
            end
        elseif e.type == "message" then
            local msg = e.msg
            if msg.kind == "start" and self.state == "joining" then
                self:BeginVersus(false)
            elseif msg.kind == "restart" and self.net then
                self:Restart()
            elseif msg.kind == "move" or msg.kind == "undo" or msg.kind == "act" or msg.kind == "end" then
                self.inbox[#self.inbox + 1] = msg
            end
        end
    end
end

-- A fingerprint of the battle both sides should agree on after every phase.
function Battle:Checksum()
    local parts = {}
    for i, u in ipairs(self.units) do
        parts[#parts + 1] = string.format("%d:%d:%.2f:%.2f", i, u.hp, u.x, u.y)
    end
    return table.concat(parts, "|")
end

-- The command for the selected unit's action, sent when it's chosen: what it does, and where it
-- stands doing it.
function Battle:Command(action, extra)
    local u = self.selected
    local cmd = { kind = "act", unit = u.index, action = action, x = u.x, y = u.y, z = u.z }
    for k, v in pairs(extra or {}) do cmd[k] = v end
    return cmd
end

-- Sends a command to the opponent, in a versus game.
function Battle:Send(cmd)
    if self.net then Net.Send(cmd) end
end

function Battle:Camera()
    if not self.camera then self.camera = GetEntityByName("CAMERA") end
    return self.camera
end

function Battle:StartBattle()
    for _, u in ipairs(self.units) do DestroyEntity(u.entity) end
    self.units = {}
    for _, r in ipairs(ROSTER) do
        local x, y = Board.Center(r[3], r[4])
        local z = NavFloorHeight(x, y, 0)
        if z then
            local u = Units.Spawn(r[1], r[2], { x = x, y = y, z = z })
            if u then
                self.units[#self.units + 1] = u
                u.index = #self.units  -- how commands name it; the roster spawns the same on both sides
            end
        end
    end
    self.turn = 0
    self.selected, self.mode, self.reach, self.viewed = nil, nil, nil, nil
    self:Run(function() self:BeginPhase(PLAYER) end)
end

function Battle:Run(fn) self.co = coroutine.create(fn) end

-- Whether a unit's ring (and so its hero frame) is lit: it's selected, acting, hovered (on the
-- field or by its frame), or the unit last picked out by clicking its frame.
function Battle:IsLit(u)
    return u == self.selected or u == self.focus or u == self.hoverUnit or u == self.viewed
end

-- The HUD's hero frames (Scripts/Components/HeroFrame.lua): the player units in the frames on
-- the left of the screen, the enemies in those on the right, top to bottom, each in its team's
-- colors and lit with its unit's ring.
function Battle:UpdateHeroFrames()
    if not HeroFrames then return end
    local teams = { [PLAYER] = {}, [ENEMY] = {} }
    for _, u in ipairs(self.units or {}) do
        local list = teams[u.team]
        if list then list[#list + 1] = u end
    end
    local slots = { [PLAYER] = 0, [ENEMY] = 0 }
    for _, frame in ipairs(HeroFrames.Frames()) do
        local x, _, w = HeroFrames.Rect(frame)
        local team = x + w / 2 < SCREEN_W / 2 and PLAYER or ENEMY
        slots[team] = slots[team] + 1
        local u = teams[team][slots[team]]
        local binding = HeroFrames.bindings[frame]
        if not binding or binding.unit ~= u then
            local glow = Units.RING_COLORS[team == PLAYER and "player" or "enemy"]
            HeroFrames.Bind(frame, u, glow, FRAME_FILLS[team])
            binding = HeroFrames.bindings[frame]
        end
        if binding then binding.lit = self:IsLit(u) end
    end
end

-- Clicking a hero frame: while aiming, it targets that unit, as clicking it on the field would.
-- Otherwise a ready player unit is selected and any other unit is picked out to look at; either
-- way the camera glides over to it.
function Battle:ClickFrame(u)
    if self.selected and (self.mode == "attack" or self.mode == "spell") then
        self:Click()  -- the hovered unit is the frame's
        return
    end
    local ready = u.team == self.me and self.phase == self.me and not u.acted and (self.mode == nil or self.mode == "move")
    if ready then
        self.viewed = nil
        self:Select(u)
    else
        self.viewed = u
        self:Follow({ x = u.x, y = u.y })
    end
end

function Battle:Update(dt)
    self.time = self.time + dt
    self:UpdateCamera(dt)
    self:UpdateHeroFrames()

    if self.state == "loading" then
        self.loadTimer = self.loadTimer + dt
        if self.loadTimer >= 0.25 then
            self.loadTimer = 0
            local count = self.board:Build()
            self.loadChecks = (count > 0 and count == self.lastCount) and self.loadChecks + 1 or 0
            self.lastCount = count
            if self.loadChecks >= 3 then self:Lobby() end
        end
        return
    end

    self:PollNetwork()

    if self.co then
        local ok, err = coroutine.resume(self.co, dt)
        if not ok then
            Log("Battler error: " .. tostring(err))
            self.co = nil
        elseif coroutine.status(self.co) == "dead" then
            self.co = nil
        end
    end

    for _, u in ipairs(self.units) do Units.Update(u, dt) end
    for i = #self.floaters, 1, -1 do
        local f = self.floaters[i]
        f.t = f.t + dt
        if f.t > 1.1 then table.remove(self.floaters, i) end
    end
    if self.banner then
        self.banner.t = self.banner.t + dt
        if self.banner.duration and self.banner.t > self.banner.duration then self.banner = nil end
    end

    Board.SyncView()
    local mouse = GetMousePosition()
    local w = ScreenToWorld(mouse)
    self.mouse = mouse
    self.frameUnit = HeroFrames and HeroFrames.At(mouse.x, mouse.y)
    if self.frameUnit then
        self.hoverPoint, self.hoverUnit = nil, self.frameUnit
    else
        self.hoverPoint = NavPick(w.x, w.y)
        self.hoverUnit = self:UnitUnder(w.x, w.y)
    end
    self:UpdatePreview()
    self:PollInput(mouse, w)
end

function Battle:PollInput(mouse, w)
    local function Feed(event)
        event.x, event.y, event.wx, event.wy = mouse.x, mouse.y, w.x, w.y
        local ok, err = pcall(self.HandleEvent, self, event)
        if not ok then Log("Battler input error: " .. tostring(err)) end
    end
    if IsMouseButtonPressed(MOUSE_LEFT) then Feed({ type = "MouseButtonPressed", button = MOUSE_LEFT }) end
    -- Right drag turns the camera (UpdateCamera), so a right click is one released without dragging.
    if IsMouseButtonPressed(MOUSE_RIGHT) then self.rightDragged = false end
    if IsMouseButtonReleased(MOUSE_RIGHT) and not self.rightDragged then
        Feed({ type = "MouseButtonPressed", button = MOUSE_RIGHT })
    end
    for _, key in ipairs({ KEY_R, KEY_ESCAPE, KEY_SPACE, KEY_TAB, KEY_1, KEY_2, KEY_3, KEY_H, KEY_J, KEY_ENTER }) do
        if IsKeyPressed(key) then Feed({ type = "KeyPressed", key = key }) end
    end
end

-- Middle drag pans, right drag turns, the wheel zooms. Pitch follows zoom: zoomed out looks
-- down from above, zoomed in looks across the ground.
local ZOOM_MIN, ZOOM_MAX = 0.5, 2.2
local PITCH_OUT, PITCH_IN = 75, 15  -- degrees, at ZOOM_MIN and ZOOM_MAX
local DRAG_THRESHOLD = 6            -- pixels a right press moves before it's a drag, not a click

local function PitchFor(zoom)
    local k = (math.log(zoom) - math.log(ZOOM_MIN)) / (math.log(ZOOM_MAX) - math.log(ZOOM_MIN))
    k = math.max(0, math.min(1, k))
    return PITCH_OUT + (PITCH_IN - PITCH_OUT) * k
end

function Battle:UpdateCamera(dt)
    local cam = self:Camera()
    if not cam then return end
    local t = GetComponent(cam, "Transform")
    local c = GetComponent(cam, "Camera")
    if not t or not c then return end

    -- Pans in screen directions, whichever way the camera faces: a screen offset (in pixels)
    -- as a ground offset.
    local function Pan(px, py)
        local a = ScreenToWorld(Vector2.new(SCREEN_W / 2, SCREEN_H / 2))
        local b = ScreenToWorld(Vector2.new(SCREEN_W / 2 + px, SCREEN_H / 2 + py))
        t.localX, t.localY = t.localX + (b.x - a.x), t.localY + (b.y - a.y)
    end

    -- The director: glide toward whatever the action is about (a unit, or a point it moves).
    if self.follow then
        local k = 1 - math.exp(-6 * dt)
        t.localX = t.localX + (self.follow.x - t.localX) * k
        t.localY = t.localY + (self.follow.y - t.localY) * k
    end

    self.zoomTarget = self.zoomTarget or c.zoom
    local wheel = GetMouseWheelMove()
    if wheel ~= 0 then self.zoomTarget = math.max(ZOOM_MIN, math.min(ZOOM_MAX, self.zoomTarget * 1.15 ^ wheel)) end
    c.zoom = c.zoom + (self.zoomTarget - c.zoom) * (1 - math.exp(-12 * dt))
    c.pitch = PitchFor(c.zoom)

    -- Right drag turns: the camera follows the mouse sideways.
    local m = GetMousePosition()
    self.yawTarget = self.yawTarget or c.yaw
    if IsMouseButtonDown(MOUSE_RIGHT) then
        self.turnPress = self.turnPress or { x = m.x, y = m.y }
        if math.abs(m.x - self.turnPress.x) + math.abs(m.y - self.turnPress.y) > DRAG_THRESHOLD then
            self.rightDragged = true
        end
        if self.rightDragged and self.turnAnchor then
            self.yawTarget = self.yawTarget + (m.x - self.turnAnchor) * 0.3
        end
        self.turnAnchor = m.x
    else
        self.turnPress, self.turnAnchor = nil, nil
    end
    c.yaw = c.yaw + (self.yawTarget - c.yaw) * (1 - math.exp(-14 * dt))
    if math.abs(self.yawTarget - c.yaw) < 0.01 then c.yaw = self.yawTarget end
    if c.yaw >= 360 or c.yaw <= -360 then
        local wrap = c.yaw >= 360 and -360 or 360
        c.yaw, self.yawTarget = c.yaw + wrap, self.yawTarget + wrap
    end

    if IsMouseButtonDown(MOUSE_MIDDLE) then
        self.follow = nil  -- the player took the camera
        if self.panAnchor then Pan(self.panAnchor.x - m.x, self.panAnchor.y - m.y) end
        self.panAnchor = { x = m.x, y = m.y }
    else
        self.panAnchor = nil
    end
end

-- --- Camera direction ---------------------------------------------------------------------

-- Keep the camera on `target` (anything with x, y: a unit, or a point the caller moves).
function Battle:Follow(target) self.follow = target end

-- A close, low shot across two units: the camera turns (the shorter way) until the line
-- between them runs across the screen, so both are seen side on. Returns the shot to undo.
function Battle:ActionShot(a, b)
    local c = self:Camera() and GetComponent(self:Camera(), "Camera")
    if not c then return nil end
    self.zoomTarget = self.zoomTarget or c.zoom
    self.yawTarget = self.yawTarget or c.yaw
    local home = { zoom = self.zoomTarget, yaw = self.yawTarget, follow = self.follow }

    self:Follow({ x = (a.x + b.x) / 2, y = (a.y + b.y) / 2 })
    -- Screen right on the ground is (cos yaw, sin yaw / 2): a ground y unit is half an x unit.
    local dx, dy = b.x - a.x, b.y - a.y
    if dx * dx + dy * dy > 1 then
        local yaw = math.deg(math.atan(2 * dy, dx))
        yaw = yaw + 180 * math.floor((home.yaw - yaw) / 180 + 0.5)
        self.yawTarget = yaw
    end
    -- Closer, and so lower (pitch follows zoom).
    self.zoomTarget = math.min(ZOOM_MAX, home.zoom * 1.35)
    return home
end

function Battle:EndShot(home)
    if not home then return end
    self.zoomTarget, self.yawTarget = home.zoom, home.yaw
    self.follow = home.follow
end

-- The unit whose body is under a picture point (nearest the camera), else one standing right
-- at the floor point there.
function Battle:UnitUnder(wx, wy)
    -- The body is a box on the screen from the feet up, however the camera faces.
    local m = self.mouse or GetMousePosition()
    local best, bestY = nil, -1e9
    for _, u in ipairs(self.units) do
        if u.alive then
            local fx, fy = ViewProject(u.x, u.y, u.z)
            local _, hy = ViewProject(u.x, u.y, u.z + 105)
            local tall = fy - hy   -- the body's height on screen, which sets its width too
            if math.abs(m.x - fx) <= tall * 0.24 and m.y <= fy + tall * 0.08 and m.y >= hy and fy > bestY then
                best, bestY = u, fy
            end
        end
    end
    if best then return best end
    local p = self.hoverPoint
    if p then
        for _, u in ipairs(self.units) do
            if u.alive and Rules.Distance(u, p) <= Rules.UNIT_RADIUS then return u end
        end
    end
    return nil
end

-- --- Phases -------------------------------------------------------------------------------

function Battle:Living(team)
    local list = {}
    for _, u in ipairs(self.units) do
        if u.alive and (team == nil or u.team == team) then list[#list + 1] = u end
    end
    return list
end

function Battle:ShowBanner(text, sub, duration)
    self.banner = { text = text, sub = sub, t = 0, duration = duration }
end

function Battle:CheckOver()
    local again = self.net and "R  Rematch     Esc  Leave" or "Press R to fight again"
    if #self:Living(self.them) == 0 then
        self.state = "over"
        self:ShowBanner("VICTORY", again)
        return true
    elseif #self:Living(self.me) == 0 then
        self.state = "over"
        self:ShowBanner("DEFEAT", again)
        return true
    end
    return false
end

-- Starts `team`'s phase: ours takes input, the other side's is the AI's (solo) or replays the
-- opponent's commands (versus). The blue team moves first, so its phase starts a new turn.
function Battle:BeginPhase(team)
    if team == PLAYER then self.turn = self.turn + 1 end
    self.phase = team
    for _, u in ipairs(self.units) do u.moved, u.acted = false, false end
    if team == self.me then
        self:ShowBanner(self.net and "YOUR PHASE" or "PLAYER PHASE", "Turn " .. self.turn, 1.2)
        Wait(0.9)
        self:SelectNext()
    elseif self.net then
        self:ShowBanner("OPPONENT'S PHASE", "Turn " .. self.turn, 1.2)
        self:RemotePhase()
    else
        self:EnemyPhase()
    end
end

-- Our phase is over: tell the other side (with our checksum) and hand it over.
function Battle:EndMyPhase()
    self.selected, self.mode, self.reach = nil, nil, nil
    if self.net then Net.Send({ kind = "end", sum = self:Checksum() }) end
    self:BeginPhase(self.them)
end

-- The opponent's phase: replays their commands as they arrive, until they end it.
function Battle:RemotePhase()
    while true do
        local msg = table.remove(self.inbox, 1)
        if not msg then
            coroutine.yield()
        elseif msg.kind == "move" or msg.kind == "undo" or msg.kind == "act" then
            self:Replay(msg)
            if self:CheckOver() then self.focus = nil return end
        elseif msg.kind == "end" then
            if msg.sum ~= self:Checksum() then
                Log("Battler: DESYNC after the opponent's phase\n  theirs: " .. tostring(msg.sum) .. "\n  ours:   " .. self:Checksum())
                self:Float("DESYNC", self.units[1].x, self.units[1].y, self.units[1].z + 40, COLORS.bad)
            end
            self.focus = nil
            return self:BeginPhase(self.me)
        end
    end
end

-- Plays one of the opponent's commands as they issued it. Each lands the unit exactly where
-- the sender had it (x, y, z), so the two sides never drift.
--   move: walk the path.   undo: snap back to before the move.   act: attack / spell / wait.
function Battle:Replay(cmd)
    local u = self.units[cmd.unit]
    if not u or not u.alive then return end
    self.focus = u
    if cmd.kind == "move" then
        self:Follow(u)
        if #cmd.path > 0 then self:MoveAlong(u, cmd.path) end
        u.x, u.y, u.z = cmd.x, cmd.y, cmd.z
    elseif cmd.kind == "undo" then
        u.x, u.y, u.z, u.moved = cmd.x, cmd.y, cmd.z, false
    else
        u.x, u.y, u.z = cmd.x, cmd.y, cmd.z
        local target = cmd.target and self.units[cmd.target]
        if cmd.action == "attack" and target then
            self:Attack(u, target)
        elseif cmd.action == "spell" and cmd.at then
            self:CastBolt(u, cmd.at)
        end
        u.acted, u.moved = true, true
        Wait(0.2)
    end
end

-- The next unit of ours still to act, in roster order after `after` (wrapping), else nil.
function Battle:NextReady(after)
    local list = self:Living(self.me)
    local start = 0
    for i, u in ipairs(list) do if u == after then start = i end end
    for k = 1, #list do
        local u = list[(start + k - 1) % #list + 1]
        if not u.acted then return u end
    end
    return nil
end

-- Hands the turn to the next unit in the queue: selects it, and the camera glides over.
function Battle:SelectNext(after)
    local u = self:NextReady(after)
    if u then self:Select(u) end
end

function Battle:EndPlayerPhase()
    self.selected, self.mode, self.reach = nil, nil, nil
    self:Run(function() self:EndMyPhase() end)
end

-- The AI's phase, solo only.
function Battle:EnemyPhase()
    self:ShowBanner("ENEMY PHASE", nil, 1.2)
    Wait(1.0)
    for _, u in ipairs(self:Living(self.them)) do
        if u.alive then
            local plan = Rules.Plan(self.units, u)
            if plan then
                self.focus = u
                self:Follow(u)
                Wait(0.45)  -- look at who's acting before they go
                if #plan.path > 0 then self:MoveAlong(u, plan.path) end
                if plan.action == "attack" and plan.target.alive then
                    self:Attack(u, plan.target)
                elseif plan.action == "spell" and plan.target.alive then
                    self:CastBolt(u, { x = plan.target.x, y = plan.target.y, z = plan.target.z })
                end
                u.acted = true
                Wait(0.2)
                if self:CheckOver() then self.focus = nil return end
            end
        end
    end
    self.focus = nil
    self:BeginPhase(self.me)
end

function Battle:AllPlayersDone()
    for _, u in ipairs(self:Living(self.me)) do
        if not u.acted then return false end
    end
    return true
end

-- --- Actions ------------------------------------------------------------------------------

-- Walks the waypoints at a steady ground speed, the clip and facing following each leg.
function Battle:MoveAlong(u, path)
    self:Follow(u)
    Units.Play(u, "Walk")
    for _, p in ipairs(path) do
        local x0, y0, z0 = u.x, u.y, u.z
        Units.Face(u, p.x, p.y)
        local duration = math.max(0.05, Rules.Distance({ x = x0, y = y0 }, p) / SPEED)
        local t = 0
        while t < duration do
            t = t + coroutine.yield()
            local k = math.min(1, t / duration)
            u.x, u.y = Lerp(x0, p.x, k), Lerp(y0, p.y, k)
            u.z = Lerp(z0, p.z, k)
        end
        u.x, u.y, u.z = p.x, p.y, p.z
    end
    Units.Play(u, "Idle")
    u.moved = true
end

function Battle:Float(text, x, y, z, color)
    self.floaters[#self.floaters + 1] = { text = text, x = x, y = y, z = z, t = 0, color = color }
end

function Battle:Hurt(target, dmg)
    target.hp = math.max(0, target.hp - dmg)
    target.shake = 0.25
    self:Float(tostring(dmg), target.x, target.y, target.z, {r = 255, g = 225, b = 120, a = 255})
    if target.hp <= 0 and target.alive then
        target.alive = false
        Units.Play(target, "Death")
        self:Float("DEFEATED", target.x, target.y, target.z + 20, {r = 255, g = 90, b = 80, a = 255})
    elseif target.alive then
        Units.Play(target, "Hurt")
    end
end

function Battle:Attack(att, def)
    local home = self:ActionShot(att, def)
    Units.Face(att, def.x, def.y)
    Wait(0.3)  -- let the camera settle in
    Units.Play(att, "Attack")
    local length = Units.ClipLength("Attack", att)
    Wait(length * 0.55)
    self:Hurt(def, Rules.Damage(att, att, def, def, Rules.AttackPower(att)))
    Wait(length * 0.45 + (def.alive and 0.3 or 0.8))  -- linger on a kill
    self:EndShot(home)
end

function Battle:CastBolt(caster, at)
    local spell = caster.class.spell
    local home = self:ActionShot(caster, at)
    Units.Face(caster, at.x, at.y)
    Units.Play(caster, spell.cast or "Attack")
    self:Float(spell.name .. "!", caster.x, caster.y, caster.z + 20, {r = 170, g = 210, b = 255, a = 255})
    Wait(0.35)

    local sx, sy, sz = caster.x, caster.y, caster.z + 55
    local ex, ey, ez = at.x, at.y, at.z + 30
    local bolt = SpawnPrefab("Prefabs/Bolt.xml", sx, sy, sz)
    local flight = 0.35 + Rules.Distance(caster, at) / 1100
    -- Ride along: the camera leans from the shot's middle toward where the bolt lands.
    local mid = { x = (sx + ex) / 2, y = (sy + ey) / 2 }
    local eye = { x = mid.x, y = mid.y }
    self:Follow(eye)
    local t = 0
    while t < flight do
        t = t + coroutine.yield()
        local k = math.min(1, t / flight)
        eye.x, eye.y = Lerp(mid.x, ex, k * 0.6), Lerp(mid.y, ey, k * 0.6)
        local tr = bolt and GetComponent(bolt, "Transform")
        if tr then
            tr.localX, tr.localY = Lerp(sx, ex, k), Lerp(sy, ey, k)
            tr.localZ = Lerp(sz, ez, k) + math.sin(k * math.pi) * 70
        end
    end
    if bolt then DestroyEntity(bolt) end

    for _, v in ipairs(Rules.Splash(self.units, at, Rules.SplashRadius(caster))) do
        if v.team ~= caster.team then
            self:Hurt(v, Rules.Damage(caster, caster, v, v, Rules.SplashPower(caster, v, at)))
        end
    end
    Wait(0.8)
    self:EndShot(home)
end

-- Commits the selected unit's turn: `cmd` (Battle:Command) goes to the opponent, who replays it,
-- while `fn` plays it here.
function Battle:PlayerAction(cmd, fn)
    local u = self.selected
    self:Send(cmd)
    self.mode, self.reach = nil, nil
    self:Run(function()
        fn()
        u.acted, u.moved = true, true
        self.selected = nil
        if self:CheckOver() then return end
        if self:AllPlayersDone() then
            Wait(0.3)
            self:EndMyPhase()
        else
            Wait(0.15)
            self:SelectNext(u)
        end
    end)
end

-- --- Input --------------------------------------------------------------------------------

function Battle:Busy() return self.co ~= nil or self.state ~= "battle" or self.phase ~= self.me end

function Battle:Select(u)
    self.selected, self.viewed = u, nil
    self:Follow({ x = u.x, y = u.y })  -- centre once; don't chase the move preview
    self.origin = { x = u.x, y = u.y, z = u.z }
    self.mode = "move"
    self.reach = Rules.Reach(self.units, u)
    self.runs = self.reach and self.reach:Runs() or {}
end

-- What clicking the hovered point would do in move mode: the path there and its cost.
function Battle:UpdatePreview()
    self.preview = nil
    local u, p = self.selected, self.hoverPoint
    if self.mode ~= "move" or not u or not p or not self.reach or self.co then return end
    if self.hoverUnit then return end
    local cost = self.reach:Cost(p.x, p.y, p.z)
    if cost then
        self.preview = { at = p, cost = cost, path = self.reach:PathTo(p.x, p.y, p.z) }
    else
        self.preview = { at = p, out = true }
    end
end

function Battle:MenuItems()
    local u = self.selected
    if not u then return {} end
    local items = { { key = "1", label = "Attack", mode = "attack" } }
    if u.class.spell then items[#items + 1] = { key = "2", label = u.class.spell.name, mode = "spell" } end
    items[#items + 1] = { key = "3", label = "Wait", mode = "wait" }
    for k, item in ipairs(items) do
        item.x, item.y, item.w, item.h = SCREEN_W - 230, SCREEN_H - 60 - (#items - k) * 42, 200, 36
    end
    return items
end

function Battle:Choose(mode)
    local u = self.selected
    if mode == "wait" then
        self:PlayerAction(self:Command("wait"), function() end)
    elseif mode == "spell" and not u.class.spell then
        return
    else
        self.mode = mode
        local range = mode == "spell" and Rules.SpellRange(u) or Rules.AttackRange(u)
        self.cover = self:BuildCover(u, range)
    end
end

function Battle:Back()
    local u = self.selected
    if not u then return end
    if self.mode == "attack" or self.mode == "spell" then
        self.mode = "menu"
    elseif self.mode == "menu" then
        u.x, u.y, u.z, u.moved = self.origin.x, self.origin.y, self.origin.z, false
        self:Send({ kind = "undo", unit = u.index, x = u.x, y = u.y, z = u.z })
        self:Select(u)
    else
        self.selected, self.mode, self.reach, self.viewed = nil, nil, nil, nil
    end
end

-- Where a spell aimed at the hovered point would land, if it's in range.
function Battle:SpellTarget()
    local u = self.selected
    local p = self.hoverUnit and { x = self.hoverUnit.x, y = self.hoverUnit.y, z = self.hoverUnit.z } or self.hoverPoint
    if u and p and Rules.Reaches(Rules.SpellRange(u), u, p) then return p end
    return nil
end

function Battle:Click()
    local u = self.selected
    local there = self.hoverUnit

    if self.mode == nil or self.mode == "move" then
        if there and there.team == self.me and not there.acted and there ~= u then
            self:Select(there)
            return
        end
        if not u then return end
        if there == u then
            self.mode = "menu"
        elseif self.preview and not self.preview.out and #self.preview.path > 0 then
            local path = self.preview.path
            self.mode, self.preview = nil, nil
            local walk = {}
            for _, p in ipairs(path) do walk[#walk + 1] = { x = p.x, y = p.y, z = p.z } end
            local last = walk[#walk]
            self:Send({ kind = "move", unit = u.index, path = walk, x = last.x, y = last.y, z = last.z })
            self:Run(function()
                self:MoveAlong(u, path)
                self.mode = "menu"
            end)
        end
    elseif self.mode == "attack" then
        if there and there.team ~= u.team and Rules.Reaches(Rules.AttackRange(u), u, there) then
            self:PlayerAction(self:Command("attack", { target = there.index }), function() self:Attack(u, there) end)
        end
    elseif self.mode == "spell" then
        local at = self:SpellTarget()
        if at then
            at = { x = at.x, y = at.y, z = at.z }
            self:PlayerAction(self:Command("spell", { at = at }), function() self:CastBolt(u, at) end)
        end
    end
end

function Battle:HandleEvent(event)
    if event.type == "KeyPressed" then
        if self.state == "lobby" or self.state == "waiting" or self.state == "joining" then
            self:LobbyKey(event.key)
            return true
        end
        if event.key == KEY_R and (self.state == "over" or (self.state == "battle" and not self.net)) then
            -- A versus restart is a rematch, and both sides start over together.
            if self.net then Net.Send({ kind = "restart" }) end
            self:Restart()
            return true
        end
        if event.key == KEY_ESCAPE and (self.state == "over" or (self.net and self:Busy())) then
            self:Lobby()
            return true
        end
        if self:Busy() then return false end
        if event.key == KEY_ESCAPE then self:Back() return true end
        if event.key == KEY_SPACE then self:EndPlayerPhase() return true end
        if event.key == KEY_TAB then
            for _, u in ipairs(self:Living(self.me)) do
                if not u.acted and u ~= self.selected then self:Select(u) break end
            end
            return true
        end
        if self.mode == "menu" then
            for _, item in ipairs(self:MenuItems()) do
                if (item.key == "1" and event.key == KEY_1) or (item.key == "2" and event.key == KEY_2)
                    or (item.key == "3" and event.key == KEY_3) then
                    self:Choose(item.mode)
                    return true
                end
            end
        end
    elseif event.type == "MouseButtonPressed" then
        if self:Busy() then return false end
        if event.button == MOUSE_RIGHT then self:Back() return true end
        if event.button ~= MOUSE_LEFT then return false end
        if self.frameUnit then self:ClickFrame(self.frameUnit) return true end
        if self.mode == "menu" or self.mode == "attack" or self.mode == "spell" then
            for _, item in ipairs(self:MenuItems()) do
                if event.x >= item.x and event.x <= item.x + item.w and event.y >= item.y and event.y <= item.y + item.h then
                    self:Choose(item.mode)
                    return true
                end
            end
        end
        if self.mode == "menu" then return true end
        self:Click()
        return true
    end
    return false
end

-- --- Drawing ---

-- The ground an attack with `range` reaches from `u`: a polar grid of cells around it, each
-- kept where the floor there is in range and in sight. Built once per mode, drawn each frame.
function Battle:BuildCover(u, range)
    local cells = {}
    local maxR = Rules.MaxReach(range, u)
    local angles = 56
    local step = range[1] == 0 and 12 or 22
    local r0 = range[1] == 0 and 0 or math.max(0, range[1] - step)
    for i = 0, angles - 1 do
        local a0, a1 = i / angles * math.pi * 2, (i + 1) / angles * math.pi * 2
        local r = r0
        while r < maxR do
            local r1 = math.min(maxR, r + step)
            local rm, am = (r + r1) * 0.5, (a0 + a1) * 0.5
            local x, y = u.x + math.cos(am) * rm, u.y + math.sin(am) * rm * 0.5
            local z = NavFloorHeight(x, y, u.z)
            if z and Rules.Reaches(range, u, { x = x, y = y, z = z }) then
                local pts = {}
                for _, c in ipairs({ { r, a0 }, { r1, a0 }, { r1, a1 }, { r, a1 } }) do
                    pts[#pts + 1] = { x = u.x + math.cos(c[2]) * c[1], y = u.y + math.sin(c[2]) * c[1] * 0.5, z = z }
                end
                cells[#cells + 1] = pts
            end
            r = r1
        end
    end
    return cells
end

function Battle:DrawCover(color)
    -- Lifted as drawn, so the cells follow the camera as it turns.
    for _, cell in ipairs(self.cover or {}) do
        local pts = {}
        for k, p in ipairs(cell) do
            local x, y = Board.Lift(p.x, p.y, p.z)
            pts[k] = { x = x, y = y }
        end
        DrawPolygon(pts, color, "overlay")
    end
end---------------------------------------------------------------------------

-- The reach as row runs, each lifted to its height; the far part of the budget a shade lighter.
function Battle:DrawReach(u)
    local budget = Rules.MoveBudget(u)
    for _, run in ipairs(self.runs or {}) do
        local c = run.cost > budget * 0.5 and COLORS.moveFar or COLORS.move
        local y0, y1 = run.y - CELL * 0.5, run.y + CELL * 0.5
        local ax, ay = Board.Lift(run.x0, y0, run.z)
        local bx, by = Board.Lift(run.x1, y0, run.z)
        local cx, cy = Board.Lift(run.x1, y1, run.z)
        local dx, dy = Board.Lift(run.x0, y1, run.z)
        DrawPolygon({ { x = ax, y = ay }, { x = bx, y = by }, { x = cx, y = cy }, { x = dx, y = dy } }, c, "overlay")
    end
end

function Battle:DrawPath(from, path, color)
    local prev = from
    for _, p in ipairs(path) do
        local x1, y1 = Board.Lift(prev.x, prev.y, prev.z)
        local x2, y2 = Board.Lift(p.x, p.y, p.z)
        DrawLine(x1, y1, x2, y2, color, "overlay")
        prev = p
    end
end

function Battle:Render()
    if self.state == "loading" then
        DrawText("Reading the battlefield...", SCREEN_W / 2 - 150, SCREEN_H / 2, 24, COLORS.text, "ui")
        return
    end

    local u = self.selected
    if u and self.mode == "move" then
        self:DrawReach(u)
        local pv = self.preview
        if pv and not pv.out then
            self:DrawPath(u, pv.path, COLORS.path)
            GroundCircle(pv.at.x, pv.at.y, pv.at.z, Rules.UNIT_RADIUS, COLORS.ghost)
        elseif pv and pv.out then
            GroundCircle(pv.at.x, pv.at.y, pv.at.z, Rules.UNIT_RADIUS, COLORS.bad)
        end
    elseif u and self.mode == "attack" then
        local r = Rules.AttackRange(u)
        self:DrawCover(COLORS.attack)
        for _, v in ipairs(self:Living()) do
            if v.team ~= u.team and Rules.Reaches(r, u, v) then
                GroundCircle(v.x, v.y, v.z, Rules.UNIT_RADIUS + 6, COLORS.target)
            end
        end
    elseif u and self.mode == "spell" then
        self:DrawCover(COLORS.spell)
        local at = self:SpellTarget()
        if at then
            local radius = Rules.SplashRadius(u)
            GroundDisc(at.x, at.y, at.z, radius, COLORS.splash)
            GroundCircle(at.x, at.y, at.z, radius, COLORS.splashEdge)
            local sx, sy = Board.Lift(u.x, u.y, u.z)
            local tx, ty = Board.Lift(at.x, at.y, at.z)
            DrawLine(sx, sy, tx, ty, COLORS.spellEdge, "overlay")
        elseif self.hoverPoint then
            local p = self.hoverPoint
            local sx, sy = Board.Lift(u.x, u.y, u.z)
            local tx, ty = Board.Lift(p.x, p.y, p.z)
            DrawLine(sx, sy, tx, ty, COLORS.bad, "overlay")
        end
    end

    for _, v in ipairs(self.units) do
        Units.DrawRing(v, self:IsLit(v), self.time)
    end

    for _, f in ipairs(self.floaters) do
        local x, y = ViewProject(f.x, f.y, f.z + Units.HEAD + 15)
        local a = math.floor(255 * math.max(0, 1 - f.t / 1.1))
        DrawText(f.text, x - 10, y - 15 - f.t * 40, 22, { r = f.color.r, g = f.color.g, b = f.color.b, a = a }, "ui")
    end

    self:DrawHud()
end

function Battle:Forecast()
    local u, v = self.selected, self.hoverUnit
    if not u then return nil end
    if self.mode == "attack" and v and v.team ~= u.team and Rules.Reaches(Rules.AttackRange(u), u, v) then
        return string.format("%s -> %s: %d damage", u.name, v.name, Rules.Damage(u, u, v, v, Rules.AttackPower(u)))
    elseif self.mode == "spell" then
        local at = self:SpellTarget()
        if not at then return nil end
        local total, n = 0, 0
        for _, w in ipairs(Rules.Splash(self.units, at, Rules.SplashRadius(u))) do
            if w.team ~= u.team then
                total, n = total + Rules.Damage(u, u, w, w, Rules.SplashPower(u, w, at)), n + 1
            end
        end
        return n > 0 and string.format("%s hits %d for %d total", u.class.spell.name, n, total) or "No targets"
    elseif self.mode == "move" and self.preview then
        if self.preview.out then return "Too far" end
        return string.format("Move %d / %d", math.floor(self.preview.cost), Rules.MoveBudget(u))
    end
    return nil
end

function Battle:DrawHud()
    FillRect(0, 0, SCREEN_W, 36, COLORS.panel, "ui")
    if self.state ~= "battle" and self.state ~= "over" then
        self:DrawBanner()
        return
    end
    local phase
    if self.phase == self.me then phase = self.net and "Your phase" or "Player phase"
    else phase = self.net and "Opponent's phase" or "Enemy phase" end
    DrawText(string.format("Turn %d  -  %s", math.max(1, self.turn), phase), 16, 8, 20, COLORS.gold, "ui")
    DrawText(string.format("Allies %d   Foes %d", #self:Living(self.me), #self:Living(self.them)),
        SCREEN_W - 230, 8, 20, COLORS.text, "ui")

    local shown = self.hoverUnit or self.selected or self.focus
    if shown then
        FillRect(16, SCREEN_H - 120, 300, 104, COLORS.panel, "ui")
        local c = shown.team == PLAYER and {r = 120, g = 190, b = 255, a = 255} or {r = 255, g = 120, b = 100, a = 255}
        DrawText(shown.name, 30, SCREEN_H - 110, 24, c, "ui")
        DrawText(string.format("HP %d / %d", shown.hp, shown.maxHp), 30, SCREEN_H - 80, 20, COLORS.text, "ui")
        local cl = shown.class
        DrawText(string.format("ATK %d  DEF %d  MOV %d", cl.atk, cl.def, Rules.MoveBudget(shown)),
            30, SCREEN_H - 56, 18, COLORS.dim, "ui")
        if cl.spell then
            local r = Rules.SpellRange(shown)
            DrawText(string.format("%s: %d power, range %d-%d", cl.spell.name, cl.spell.power, r[1], r[2]),
                30, SCREEN_H - 34, 16, COLORS.dim, "ui")
        end
    end

    if self.selected and (self.mode == "menu" or self.mode == "attack" or self.mode == "spell") then
        for _, item in ipairs(self:MenuItems()) do
            local active = item.mode == self.mode
            local over = self.mouse and self.mouse.x >= item.x and self.mouse.x <= item.x + item.w
                and self.mouse.y >= item.y and self.mouse.y <= item.y + item.h
            local bg = active and {r = 70, g = 60, b = 30, a = 230} or (over and {r = 40, g = 40, b = 60, a = 230} or COLORS.panel)
            FillRect(item.x, item.y, item.w, item.h, bg, "ui")
            DrawText(item.key .. "  " .. item.label, item.x + 14, item.y + 8, 20, active and COLORS.gold or COLORS.text, "ui")
        end
    end

    local forecast = self:Forecast()
    if forecast then
        FillRect(SCREEN_W / 2 - 180, SCREEN_H - 56, 360, 36, COLORS.panel, "ui")
        DrawText(forecast, SCREEN_W / 2 - 165, SCREEN_H - 48, 20, COLORS.gold, "ui")
    end

    local hint
    if self.phase == self.me and not self.co and self.state == "battle" then
        if not self.selected then hint = "Click a unit (Tab cycles)   Space: end phase"
        elseif self.mode == "move" then hint = "Click in the blue area to move, the unit itself to stay   Right-click: cancel"
        elseif self.mode == "menu" then hint = "Choose an action   Right-click: undo move"
        else hint = "Click a target   Right-click: back" end
    end
    if hint then DrawText(hint, 16, 44, 16, COLORS.dim, "ui") end

    self:DrawBanner()
end

function Battle:DrawBanner()
    if self.banner then
        local b = self.banner
        local a = b.duration and math.floor(255 * math.min(1, (b.duration - b.t) * 3, b.t * 4)) or 255
        a = math.max(0, math.min(255, a))
        FillRect(0, SCREEN_H / 2 - 50, SCREEN_W, 100, {r = 8, g = 8, b = 14, a = math.floor(a * 0.8)}, "ui")
        DrawText(b.text, SCREEN_W / 2 - MeasureText(b.text, 44) / 2, SCREEN_H / 2 - 38, 44,
            {r = 255, g = 215, b = 120, a = a}, "ui")
        if b.sub then
            DrawText(b.sub, SCREEN_W / 2 - MeasureText(b.sub, 22) / 2, SCREEN_H / 2 + 12, 22,
                {r = 220, g = 220, b = 230, a = a}, "ui")
        end
    end
end

return Battle
