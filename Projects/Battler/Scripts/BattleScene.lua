---@type SceneScript
-- Battler POC: a skirmish with free movement, fought with cards. Each unit has its own deck
-- (Scripts/Battler/Cards.lua) and hand, and acts by playing cards it can pay for with mana.
-- Every phase a side's units grow a mana crystal (refilled) and draw a card.
--   Click a blue-ringed unit to see its hand (Prefabs/Hand.xml), then click a card: Move asks
--   for a spot in the blue area, Strike for a foe, Bolt for a spot to blast. Right-click / Esc
--   puts the card back. End Turn (or Space) ends the phase. Middle-drag pans, right-drag
--   turns, wheel zooms (and tilts). R restarts.
-- The battlefield is an encounter prefab (Prefabs/Encounters), spawned at the origin: its room
-- and the spawn points the units start on. In a run (Scripts/Battler/Run.lua) it's the
-- encounter the party walked into, and the result goes back to the run: a win returns to the
-- Dungeon (or the Campfire, floor cleared), a loss ends the run. Otherwise it's the default.
-- Versus: entered from the Lobby scene with the connection already open (Skirmish comes in with
-- none and plays the AI). The host plays the blue team and moves first,
-- the joiner plays red. Every card played goes across as a command the moment it's played and
-- the other side replays it with the same rules (and the same shuffles: the decks are dealt
-- from a seed both sides share), so both run the same battle (lockstep); a phase
-- ends with { kind = "end" } and a checksum of the units, to catch the two drifting apart.
local Board = require("Scripts/Battler/Board")
local Units = require("Scripts/Battler/Units")
local Rules = require("Scripts/Battler/Rules")
local Net = require("Scripts/Battler/Net")
local Cards = require("Scripts/Battler/Cards")
local Run = require("Scripts/Battler/Run")
local Music = require("Scripts/Menu/Music")
local Sfx = require("Scripts/Menu/Sfx")
local Widgets = require("Scripts/Components/Widgets")

local Battle = {}

-- Sound effects (project-relative, under Sounds/).
local SFX = {
    ambiance = "Sounds/sfx_battle_ambiance_loop.wav",  -- looped under the whole battle
    select = Sfx.SELECT,                -- a unit picked up (click, Tab, hero frame)
    order = Sfx.ORDER,                  -- a card played on its target
    click = Sfx.CLICK,                  -- a card picked up to play
    cancel = Sfx.CANCEL,                -- backing out a step
    run = "Sounds/sfx_battle_unit_run.wav",          -- footsteps, looped while a unit walks
    swing = "Sounds/sfx_battle_knight_swing.wav",    -- a melee attack's swing
    hit = "Sounds/sfx_battle_unit_hit.wav",          -- a melee attack landing
    release = "Sounds/sfx_archer_release.mp3",       -- a ranged attack's arrow leaving the bow
    arrowHit = "Sounds/sfx_arrow_hit.wav",           -- the arrow landing
    cast = "Sounds/sfx_spell_cast_mage.wav",         -- a spell cast
    boltImpact = "Sounds/sfx_spell_bolt_impact.wav", -- the bolt landing
    phasePlayer = "Sounds/sfx_battle_phase_player.wav",  -- our phase begins
    phaseEnemy = "Sounds/sfx_battle_phase_enemy.wav",    -- the enemy's / opponent's phase begins
    victory = "Sounds/sfx_battle_victory.wav",
    defeat = "Sounds/sfx_battle_defeat.wav",
}

-- An effect on the Effects channel.
local function Play(sound, volume) Sfx.Play(sound, volume) end

local PLAYER, ENEMY = Units.PLAYER, Units.ENEMY
local FONT = "Fonts/EnchantedLand-Regular.ttf"  -- every HUD line is drawn in it
local SPEED = 215          -- ground units a second
local CELL = 8             -- the navmesh's cellSize (Battle.xml), for drawing reach runs
local COS = Board.PITCH_COS


-- The kinds of card that ask for a target before they're played (Scripts/Battler/Cards.lua).
local AIMED = { attack = true, bolt = true }

local COLORS = {
    move = {r = 60, g = 140, b = 255, a = 60}, moveFar = {r = 60, g = 140, b = 255, a = 35},
    moveEdge = {r = 120, g = 190, b = 255, a = 200},
    attack = {r = 255, g = 60, b = 50, a = 45}, attackEdge = {r = 255, g = 110, b = 90, a = 200},
    bolt = {r = 170, g = 90, b = 255, a = 40}, boltEdge = {r = 200, g = 150, b = 255, a = 200},
    splash = {r = 255, g = 170, b = 60, a = 90}, splashEdge = {r = 255, g = 200, b = 120, a = 220},
    target = {r = 255, g = 230, b = 120, a = 230}, bad = {r = 255, g = 80, b = 70, a = 200},
    path = {r = 255, g = 255, b = 255, a = 200}, ghost = {r = 255, g = 255, b = 255, a = 120},
    text = {r = 235, g = 235, b = 240, a = 255}, dim = {r = 160, g = 160, b = 175, a = 255},
    panel = {r = 14, g = 14, b = 22, a = 210}, gold = {r = 255, g = 210, b = 110, a = 255},
}

-- Health bar fills for the hero frames, by team.
local HURT_FLASH = 1.6     -- seconds a hurt unit's frame stays open, to watch its health drain
local HOVER_LINGER = 0.6   -- seconds a hovered unit's frame stays open after the pointer leaves
local HEALTH_FILL = {r = 235, g = 70, b = 60, a = 255}  -- every frame's health bar; the ring glow tells the teams apart

-- --- Helpers ------------------------------------------------------------------------------

-- The HUD font everywhere unless a call names another.
local RawDrawText = DrawText
local function DrawText(text, x, y, size, color, layer, font) RawDrawText(text, x, y, size, color, layer, font or FONT) end

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
    Music.Play(Music.BATTLE)
    self.time = 0
    self.board = Board.new()   -- only to know when the navmesh has settled
    self.state = "loading"
    self.loadTimer, self.loadChecks, self.lastCount = 0, 0, -1
    self.units = {}
    self.floaters = {}
    self.inbox = {}               -- the opponent's commands, applied in order by RemotePhase
    self.me, self.them = PLAYER, ENEMY
    self.net = false              -- a versus game (true) or solo against the AI
    self.ambiance = nil           -- the looping ambiance's sound id, once the battle starts
    self.matches = 0              -- battles fought this visit; with versus, seeds the decks
    self.encounter = Run.Encounter()
    self.roster = self:SpawnRoom(self.encounter.prefab)
    self:BindHud()
    Log("Battler (free): waiting for the navmesh...")
end

-- Spawns the encounter's room and returns its roster: { class, team, x, y } per spawn point,
-- the party (HeroN, in Run.PARTY order) then the foes (MobN:Class), each in number order so
-- both sides of a versus game index the units the same.
function Battle:SpawnRoom(prefab)
    local room = SpawnPrefab(prefab, 0, 0, 0)
    if not room then Log("Battler: couldn't spawn " .. prefab) return {} end
    local heroes, mobs = {}, {}
    for _, e in ipairs(GetChildren(room)) do
        local n = GetComponent(e, "Name")
        local t = GetComponent(e, "Transform")
        local name = n and n.name:gsub("^.*::", "") or ""
        local hero = name:match("^Hero(%d+)$")
        local mob, class = name:match("^Mob(%d+):(%w+)$")
        if t and hero then
            heroes[#heroes + 1] = { n = tonumber(hero), x = t.localX, y = t.localY }
        elseif t and mob and Units.Classes[class] then
            mobs[#mobs + 1] = { n = tonumber(mob), class = class, x = t.localX, y = t.localY }
        end
    end
    local byN = function(a, b) return a.n < b.n end
    table.sort(heroes, byN)
    table.sort(mobs, byN)
    local roster = {}
    for k, h in ipairs(heroes) do
        if Run.PARTY[k] then roster[#roster + 1] = { class = Run.PARTY[k], team = PLAYER, x = h.x, y = h.y } end
    end
    for _, m in ipairs(mobs) do roster[#roster + 1] = { class = m.class, team = ENEMY, x = m.x, y = m.y } end
    return roster
end

-- --- Network ------------------------------------------------------------------------------

-- Once the navmesh is ready: versus if the Lobby left a connection open, else solo.
function Battle:Begin()
    if not self.ambiance then self.ambiance = PlaySound(SFX.ambiance, 0.5, true, CHANNEL_AMBIENT) end
    if Net.Active() then
        self:BeginVersus(Net.IsHost())
    else
        self.me, self.them, self.net = PLAYER, ENEMY, false
        self.state = "battle"
        self:StartBattle()
    end
end

-- Back to the main menu, closing any versus connection.
function Battle:Leave()
    self:StopSteps()
    if self.ambiance then StopSound(self.ambiance) self.ambiance = nil end
    if Net.Active() then Net.Stop() end
    SceneReplace("MainMenu")
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
    self.matches = self.matches + 1
    self:StopSteps()
    self.co, self.inbox = nil, {}
    self.state = "battle"
    self.banner = nil
    self:StartBattle()
end

function Battle:PollNetwork()
    for _, e in ipairs(Net.Poll()) do
        if e.type == "disconnected" and self.net then
            Net.Stop()
            self:StopSteps()
            self.co, self.state = nil, "over"
            self:ShowBanner("Your opponent left", "Esc  Menu")
        elseif e.type == "message" then
            local msg = e.msg
            if msg.kind == "restart" and self.net then
                self:Restart()
            elseif msg.kind == "play" or msg.kind == "end" then
                self.inbox[#self.inbox + 1] = msg
            end
        end
    end
end

-- A fingerprint of the battle both sides should agree on after every phase.
function Battle:Checksum()
    local parts = {}
    for i, u in ipairs(self.units) do
        parts[#parts + 1] = string.format("%d:%d:%d:%d:%.2f:%.2f", i, u.hp, u.mana, #u.piles.hand, u.x, u.y)
    end
    return table.concat(parts, "|")
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
    -- Versus: both sides deal from the same seed (and count rematches the same way).
    local seed = self.net and (7919 + self.matches * 104729) or (os and os.time and os.time() or Random(1, 1000000))
    local rng = Cards.NewRng(seed)
    for _, r in ipairs(self.roster) do
        local z = NavFloorHeight(r.x, r.y, 0)
        if z then
            local u = Units.Spawn(r.class, r.team, { x = r.x, y = r.y, z = z })
            if u then
                self.units[#self.units + 1] = u
                u.index = #self.units  -- how commands name it; the roster spawns the same on both sides
                u.piles = Cards.NewPiles(u.class.deck, rng)
                local wounds = r.team == PLAYER and Run.Wounds(u.name)
                if wounds then u.hp, u.shownHp = math.min(u.hp, wounds), math.min(u.hp, wounds) end
            end
        end
    end
    self.rng = rng
    self.turn = 0
    self:ClearOrders()
    self.selected, self.viewed = nil, nil
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
        local team = x + w / 2 < (GetScreenSize()) / 2 and PLAYER or ENEMY
        slots[team] = slots[team] + 1
        local u = teams[team][slots[team]]
        local binding = HeroFrames.bindings[frame]
        if not binding or binding.unit ~= u then
            local glow = Units.RING_COLORS[team == PLAYER and "player" or "enemy"]
            HeroFrames.Bind(frame, u, glow, HEALTH_FILL)
            binding = HeroFrames.bindings[frame]
        end
        if binding then
            binding.lit = self:IsLit(u)
            binding.open = u ~= nil and u == self:OpenFrameUnit()
            binding.flash = u ~= nil and (u.flashUntil or 0) > self.time
        end
    end
end

-- The one unit whose hero frame is open: the one being hovered (its frame, else on the field),
-- else the one selected, acting or looked at. The rest stay tucked, rings still lit. A hovered
-- frame stays open HOVER_LINGER seconds after the pointer leaves it, so sliding from one frame
-- to the next doesn't flash the selected unit's open in between.
function Battle:OpenFrameUnit()
    local hovered = self.frameUnit or self.hoverUnit
    if hovered then return hovered end
    local last = self.lastHover
    if last and last.unit.alive and self.time - last.t < HOVER_LINGER then return last.unit end
    return self.selected or self.focus or self.viewed
end

-- Clicking a hero frame: while aiming a card, it targets that unit, as clicking it on the field
-- would. Otherwise one of ours is selected (in our phase) and any other unit is picked out to
-- look at (its hand shows, face down); either way the camera glides over to it.
function Battle:ClickFrame(u)
    if self.selected and AIMED[self.mode] then
        self:Click()  -- the hovered unit is the frame's
        return
    end
    if u.team == self.me and self.phase == self.me then
        self:Select(u)
    else
        self.viewed = u
        self:Follow({ x = u.x, y = u.y })
    end
end

function Battle:Update(dt)
    self.time = self.time + dt
    self.dt = dt  -- for what eases in Render (the HUD)
    self:UpdateCamera(dt)
    self:UpdateHeroFrames()

    if self.state == "loading" then
        self.loadTimer = self.loadTimer + dt
        if self.loadTimer >= 0.25 then
            self.loadTimer = 0
            local count = self.board:Build()
            self.loadChecks = (count > 0 and count == self.lastCount) and self.loadChecks + 1 or 0
            self.lastCount = count
            if self.loadChecks >= 3 then self:Begin() end
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

    for _, u in ipairs(self.units) do
        Units.Update(u, dt)
        u.spent = u.done or not self:CanPlayAny(u)
    end
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
    if Widgets.PointerOverUi() then
        self.hoverPoint, self.hoverUnit = nil, nil
    elseif self.frameUnit then
        self.hoverPoint, self.hoverUnit = nil, self.frameUnit
    else
        self.hoverPoint = NavPick(w.x, w.y)
        -- On the field only in our phase: in theirs the pointer sits still while the camera
        -- follows the action, and units slide under it. Their frames still answer the pointer.
        self.hoverUnit = self.phase == self.me and self:UnitUnder(w.x, w.y) or nil
    end
    local hovered = self.frameUnit or self.hoverUnit
    if hovered then self.lastHover = { unit = hovered, t = self.time } end
    Widgets.BindView("HoveredUnit", hovered and hovered.entity)  -- its map health bar shows x/X
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
    for _, key in ipairs({ KEY_R, KEY_ESCAPE, KEY_SPACE, KEY_TAB }) do
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
        local sw, sh = GetScreenSize()
        local a = ScreenToWorld(Vector2.new(sw / 2, sh / 2))
        local b = ScreenToWorld(Vector2.new(sw / 2 + px, sh / 2 + py))
        t.localX, t.localY = t.localX + (b.x - a.x), t.localY + (b.y - a.y)
    end

    -- The director: glide toward whatever the action is about (a unit, or a point it moves).
    if self.follow then
        local k = 1 - math.exp(-6 * dt)
        t.localX = t.localX + (self.follow.x - t.localX) * k
        t.localY = t.localY + (self.follow.y - t.localY) * k
    end

    self.zoomTarget = self.zoomTarget or c.zoom
    local wheel = Widgets.PointerOverUi() and 0 or GetMouseWheelMove()  -- over the hand, the wheel scrolls it
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
    local again = self.net and "R  Rematch     Esc  Leave" or "R  Fight again     Esc  Menu"
    if #self:Living(self.them) == 0 then
        self.state, self.won = "over", true
        Play(SFX.victory)
        self:ShowBanner("VICTORY", Run.Active() and "Click to go on" or again)
        return true
    elseif #self:Living(self.me) == 0 then
        self.state, self.won = "over", false
        Play(SFX.defeat)
        self:ShowBanner("DEFEAT", Run.Active() and "Your run is over.   Click to return to the campfire" or again)
        return true
    end
    return false
end

-- In a run, once it's over: back to the dungeon (or the campfire), or the run ends.
function Battle:Continue()
    self:StopSteps()
    if self.ambiance then StopSound(self.ambiance) self.ambiance = nil end
    local party = {}
    for _, u in ipairs(self.units) do if u.team == PLAYER then party[#party + 1] = u end end
    if self.won then
        SceneReplace(Run.Won(party))
    else
        Run.Lost()
        SceneReplace("Campfire")
    end
end

-- Starts `team`'s phase: ours takes input, the other side's is the AI's (solo) or replays the
-- opponent's commands (versus). The blue team moves first, so its phase starts a new turn.
-- Each of the team's units grows a mana crystal and refills its mana, and (after the opening hand)
-- draws a card. After the banner the mana sequence plays on their hero frames, then the phase goes on.
function Battle:BeginPhase(team)
    if team == PLAYER then self.turn = self.turn + 1 end
    self.phase = team
    for _, u in ipairs(self:Living(team)) do
        u.done = false
        local mana, max = u.mana, u.manaMax
        u.manaMax = math.min(u.manaCap, u.manaMax + 1)
        u.mana = u.manaMax
        if HeroFrames then HeroFrames.Recharge(u, mana, max) end
        if self.turn > 1 then Cards.Draw(u.piles, 1, self.rng) end
    end
    Play(team == self.me and SFX.phasePlayer or SFX.phaseEnemy)
    local title
    if team == self.me then title = self.net and "YOUR PHASE" or "PLAYER PHASE"
    else title = self.net and "OPPONENT'S PHASE" or "ENEMY PHASE" end
    self:ShowBanner(title, "Turn " .. self.turn, 1.2)
    Wait(1.2)
    self:Recharge()
    if team == self.me then
        local first = self:Living(self.me)[1]
        if first then self:Select(first) end
    elseif self.net then
        self:RemotePhase()
    else
        self:EnemyPhase()
    end
end

-- The mana sequence (Scripts/Components/HeroFrame.lua): every frame BeginPhase readied plays it
-- at once; this returns once they've all finished.
function Battle:Recharge()
    if not HeroFrames then return end
    HeroFrames.StartRecharges()
    local t = 0
    while HeroFrames.Recharging() and t < 6 do t = t + coroutine.yield() end
end

-- Our phase is over: tell the other side (with our checksum) and hand it over.
function Battle:EndMyPhase()
    self:ClearOrders()
    self.selected = nil
    if self.net then Net.Send({ kind = "end", sum = self:Checksum() }) end
    self:BeginPhase(self.them)
end

-- The opponent's phase: replays their commands as they arrive, until they end it.
function Battle:RemotePhase()
    while true do
        local msg = table.remove(self.inbox, 1)
        if not msg then
            coroutine.yield()
        elseif msg.kind == "play" then
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

-- Plays one of the opponent's cards as they played it: the same card from the same slot of
-- the same unit's hand, paid for the same way, landing the unit exactly where the sender had
-- it (x, y, z) so the two sides never drift.
function Battle:Replay(cmd)
    local u = self.units[cmd.unit]
    if not u or not u.alive then return end
    local card = u.piles.hand[cmd.slot]
    if not card or card.id ~= cmd.card then
        Log("Battler: DESYNC: " .. u.name .. " has no " .. tostring(cmd.card) .. " in slot " .. tostring(cmd.slot))
        return
    end
    self.focus = u
    self:Follow(u)
    self:PlayOut(u, cmd.slot)
    self:Perform(u, card, cmd)
    u.x, u.y, u.z = cmd.x, cmd.y, cmd.z
    Wait(0.2)
end

-- Plays `card` for `u` on its target, already paid for: cmd.path for a move, cmd.target (a
-- unit index) for an attack, cmd.at for a bolt.
function Battle:Perform(u, card, cmd)
    if card.kind == "move" then
        if cmd.path and #cmd.path > 0 then self:MoveAlong(u, cmd.path) end
    elseif card.kind == "attack" then
        local target = cmd.target and self.units[cmd.target]
        if target and target.alive then self:Attack(u, target, card) end
    elseif card.kind == "bolt" and cmd.at then
        self:CastBolt(u, cmd.at, card)
    end
end

function Battle:EndPlayerPhase()
    self:ClearOrders()
    self.selected = nil
    self:Run(function() self:EndMyPhase() end)
end

-- Whether `u` can pay for any card in its hand.
function Battle:CanPlayAny(u)
    for _, card in ipairs(u.piles.hand) do
        if Cards.Affordable(u, card) then return true end
    end
    return false
end

-- The attack cards in `u`'s hand it can pay for, one of each, for Rules.Plan.
function Battle:AiOptions(u)
    local list, seen = {}, {}
    for _, card in ipairs(u.piles.hand) do
        if AIMED[card.kind] and not seen[card.id] and Cards.Affordable(u, card) then
            seen[card.id] = true
            list[#list + 1] = { card = card }
        end
    end
    return list
end

-- The AI plays the first `id` card in `u`'s hand (Battle:PlayOut) and returns it.
function Battle:AiPlay(u, id)
    for slot, card in ipairs(u.piles.hand) do
        if card.id == id and Cards.Affordable(u, card) then return self:PlayOut(u, slot) end
    end
    return nil
end

-- Plays the card in `u`'s hand slot `slot`: pays for it, discards it, and puts on its show (the
-- Hand flies it to the middle of the screen and burns it, Scripts/Components/Hand.lua). Its
-- `released` says when the play should land. Returns the card and the flight.
function Battle:Launch(u, slot)
    local card = u.piles.hand[slot]
    local flight = { slot = slot, faceDown = u.team ~= self.me }
    self.flights[#self.flights + 1] = flight
    Cards.Pay(u, card)
    Cards.Discard(u.piles, slot)
    return card, flight
end

-- Waits (in the battle's coroutine) for a played card's show to reach its burn.
function Battle:AwaitFlight(flight)
    local t = 0
    while not flight.released and t < 4 do t = t + coroutine.yield() end
end

-- The other side plays a card (the AI's or the opponent's): their hand comes up, the card
-- leaves it for its show, and this returns it once the play should land.
function Battle:PlayOut(u, slot)
    if self:HandUnit() ~= u then
        self.viewed, self.focus = nil, u
        Wait(0.15)  -- their hand shows before the card leaves it
    end
    local card, flight = self:Launch(u, slot)
    self:AwaitFlight(flight)
    return card
end

local AI_PLAYS = 4  -- the most attack cards an AI unit plays in a phase

-- The AI's phase, solo only.
function Battle:EnemyPhase()
    for _, u in ipairs(self:Living(self.them)) do
        -- Walk (if it holds a move card) to where its best attack card reaches, then keep
        -- playing attack cards from there while it can pay and someone's in reach.
        local _, move = Cards.Find(u, "move")
        local budget = move and Rules.MoveBudget(move) or 0
        for play = 1, AI_PLAYS do
            if not u.alive then break end
            local plan = Rules.Plan(self.units, u, play == 1 and budget or 0, self:AiOptions(u))
            if not plan or (#plan.path == 0 and not plan.option) then break end
            self.focus = u
            self:Follow(u)
            if play == 1 then Wait(0.45) end  -- look at who's acting before they go
            if #plan.path > 0 and self:AiPlay(u, "Move") then self:MoveAlong(u, plan.path) end
            local card = plan.option and plan.target.alive and self:AiPlay(u, plan.option.card.id)
            if card then
                local at = { x = plan.target.x, y = plan.target.y, z = plan.target.z }
                self:Perform(u, card, { target = plan.target.index, at = at })
            end
            Wait(0.2)
            if self:CheckOver() then self.focus = nil return end
            if not card then break end
        end
    end
    self.focus = nil
    self:BeginPhase(self.me)
end

-- --- Actions ------------------------------------------------------------------------------

-- Walks the waypoints at a steady ground speed, the clip and facing following each leg.
-- The footsteps loop of the unit walking, if any. A move cut short (restart, leaving, the
-- opponent dropping) never reaches its own stop, so those stop it here too.
function Battle:StopSteps()
    if self.steps then StopSound(self.steps) self.steps = nil end
end

function Battle:MoveAlong(u, path)
    self:Follow(u)
    Units.Play(u, "Walk")
    self:StopSteps()
    self.steps = PlaySound(SFX.run, 0.6, true, CHANNEL_EFFECTS)
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
    self:StopSteps()
    Units.Play(u, "Idle")
end

function Battle:Float(text, x, y, z, color)
    self.floaters[#self.floaters + 1] = { text = text, x = x, y = y, z = z, t = 0, color = color }
end

function Battle:Hurt(target, dmg)
    target.hp = math.max(0, target.hp - dmg)
    target.shake = 0.25
    target.flashUntil = self.time + HURT_FLASH  -- its frame opens to show the bar drop
    self:Float(tostring(dmg), target.x, target.y, target.z, {r = 255, g = 225, b = 120, a = 255})
    if target.hp <= 0 and target.alive then
        target.alive = false
        Units.Play(target, "Death")
        self:Float("DEFEATED", target.x, target.y, target.z + 20, {r = 255, g = 90, b = 80, a = 255})
    elseif target.alive then
        Units.Play(target, "Hurt")
    end
end

function Battle:Attack(att, def, card)
    def.flashUntil = math.huge  -- its frame stays open through the blow (Hurt starts the close)
    local home = self:ActionShot(att, def)
    Units.Face(att, def.x, def.y)
    Wait(0.3)  -- let the camera settle in
    Units.Play(att, "Attack")
    local length = Units.ClipLength("Attack", att)
    local ranged = card.range[2] > 1
    if ranged then
        Wait(length * 0.45)
        Play(SFX.release)
        Wait(length * 0.10)
        Play(SFX.arrowHit)
    else
        Play(SFX.swing)
        Wait(length * 0.55)
        Play(SFX.hit)
    end
    self:Hurt(def, Rules.Damage(att, def, Rules.Power(att, card)))
    Wait(length * 0.45 + (def.alive and 0.3 or 0.8))  -- linger on a kill
    self:EndShot(home)
end

function Battle:CastBolt(caster, at, card)
    -- Whoever the blast will catch has their frame open from the cast (Hurt starts the close).
    for _, v in ipairs(Rules.Splash(self.units, at, Rules.SplashRadius(card))) do
        if v.team ~= caster.team then v.flashUntil = math.huge end
    end
    local home = self:ActionShot(caster, at)
    Units.Face(caster, at.x, at.y)
    Units.Play(caster, caster.class.cast or "Attack")
    Play(SFX.cast)
    self:Float(card.name .. "!", caster.x, caster.y, caster.z + 20, {r = 170, g = 210, b = 255, a = 255})
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
    Play(SFX.boltImpact)

    for _, v in ipairs(Rules.Splash(self.units, at, Rules.SplashRadius(card))) do
        if v.team ~= caster.team then
            self:Hurt(v, Rules.Damage(caster, v, Rules.SplashPower(caster, card, v, at)))
        end
    end
    Wait(0.8)
    self:EndShot(home)
end

-- --- Input --------------------------------------------------------------------------------

function Battle:Busy() return self.co ~= nil or self.state ~= "battle" or self.phase ~= self.me end

-- Forgets any card being played and what it was showing.
function Battle:ClearOrders()
    self.mode, self.card, self.reach, self.runs, self.cover, self.preview = nil, nil, nil, nil, nil, nil
end

function Battle:Select(u)
    if self.selected ~= u then Play(SFX.select) end
    self:ClearOrders()
    self.selected, self.viewed = u, nil
    self:Follow({ x = u.x, y = u.y })  -- centre once; don't chase the move preview
end

-- What clicking the hovered point would do with a move card held: the path there and its cost.
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

-- Whose hand shows: the unit selected, else the one picked out to look at, else whoever's
-- acting in the other side's phase.
function Battle:HandUnit()
    local u = self.selected or self.viewed or self.focus
    return u and u.alive and u or nil
end

-- A card clicked in the selected unit's hand: picked up to play (clicked again, put back).
-- A card that needs a target waits for one (Click); the mode shows where it reaches.
function Battle:PlayCard(slot)
    local u = self.selected
    if self:Busy() or not u or self:HandUnit() ~= u then return end
    local card = u.piles.hand[slot]
    if not card then return end
    if self.card and self.card.slot == slot then
        Play(SFX.cancel)
        self:ClearOrders()
        return
    end
    if not Cards.Affordable(u, card) then
        Play(SFX.cancel)
        return
    end
    Play(SFX.click)
    self:ClearOrders()
    self.card, self.mode = { slot = slot, card = card }, card.kind
    if card.kind == "move" then
        self.reach = Rules.Reach(self.units, u, Rules.MoveBudget(card))
        self.runs = self.reach and self.reach:Runs() or {}
    elseif AIMED[card.kind] then
        self.cover = self:BuildCover(u, Rules.Range(card))
    end
end

-- Plays the card being held on its target: pays for it, discards it, plays it out, then tells
-- the opponent (`extra`: the target, as Battle:Perform reads it, plus where the unit ended up).
function Battle:Commit(extra)
    local u, held = self.selected, self.card
    local cmd = { kind = "play", unit = u.index, slot = held.slot, card = held.card.id }
    for k, v in pairs(extra) do cmd[k] = v end
    Play(SFX.order)
    -- The card flies from where it waits to the middle of the screen and burns: then it lands.
    local _, flight = self:Launch(u, held.slot)
    self:ClearOrders()
    self:Run(function()
        self:AwaitFlight(flight)
        self:Perform(u, held.card, cmd)
        cmd.x, cmd.y, cmd.z = u.x, u.y, u.z
        self:Send(cmd)
        self:CheckOver()
    end)
end

-- Wait: the selected unit is done for this phase (its ring greys); the next one still to go is
-- selected, and once everyone's done the phase ends.
function Battle:WaitUnit()
    local u = self.selected
    if not u then return end
    u.done = true
    local list = self:Living(self.me)
    local at = 0
    for i, v in ipairs(list) do if v == u then at = i end end
    for k = 1, #list do
        local v = list[(at + k - 1) % #list + 1]
        if not v.done then self:Select(v) return end
    end
    self:EndPlayerPhase()
end

function Battle:Back()
    if not self.selected then return end
    Play(SFX.cancel)
    if self.card then
        self:ClearOrders()
    else
        self.selected, self.viewed = nil, nil
    end
end

-- Where a bolt aimed at the hovered point would land, if it's in range.
function Battle:BoltTarget()
    local u = self.selected
    local p = self.hoverUnit and { x = self.hoverUnit.x, y = self.hoverUnit.y, z = self.hoverUnit.z } or self.hoverPoint
    if u and p and self.card and Rules.Reaches(Rules.Range(self.card.card), u, p) then return p end
    return nil
end

function Battle:Click()
    local u = self.selected
    local there = self.hoverUnit

    if self.mode == nil then
        if there and there.team == self.me then
            self:Select(there)
        elseif there then
            self.viewed = there
        end
    elseif self.mode == "move" then
        if self.preview and not self.preview.out and #self.preview.path > 0 then
            local walk = {}
            for _, p in ipairs(self.preview.path) do walk[#walk + 1] = { x = p.x, y = p.y, z = p.z } end
            self:Commit({ path = walk })
        end
    elseif self.mode == "attack" then
        if there and there.team ~= u.team and Rules.Reaches(Rules.Range(self.card.card), u, there) then
            self:Commit({ target = there.index })
        end
    elseif self.mode == "bolt" then
        local at = self:BoltTarget()
        if at then self:Commit({ at = { x = at.x, y = at.y, z = at.z } }) end
    end
end

function Battle:HandleEvent(event)
    if self.state == "over" and Run.Active() then
        local go = (event.type == "MouseButtonPressed" and event.button == MOUSE_LEFT)
            or (event.type == "KeyPressed" and (event.key == KEY_SPACE or event.key == KEY_ENTER or event.key == KEY_ESCAPE))
        if go then self:Continue() end
        return go
    end
    if event.type == "KeyPressed" and event.key == KEY_R and Run.Active() then return true end  -- no do-overs in a run
    if event.type == "KeyPressed" then
        if event.key == KEY_R and self.net and not Net.Active() then return true end  -- they left
        if event.key == KEY_R and (self.state == "over" or (self.state == "battle" and not self.net)) then
            -- A versus restart is a rematch, and both sides start over together.
            if self.net then Net.Send({ kind = "restart" }) end
            self:Restart()
            return true
        end
        if event.key == KEY_ESCAPE and (self.state == "over" or (self.net and self:Busy())) then
            self:Leave()
            return true
        end
        if self:Busy() then return false end
        if event.key == KEY_ESCAPE then self:Back() return true end
        if event.key == KEY_SPACE then self:EndPlayerPhase() return true end
        if event.key == KEY_TAB then
            local list = self:Living(self.me)
            local at = 0
            for i, u in ipairs(list) do if u == self.selected then at = i end end
            if #list > 0 then self:Select(list[at % #list + 1]) end
            return true
        end
    elseif event.type == "MouseButtonPressed" then
        if event.button == MOUSE_RIGHT then
            if not self:Busy() then self:Back() end
            return true
        end
        if event.button ~= MOUSE_LEFT then return false end
        if Widgets.PointerOverUi() then return true end  -- a card or button takes it
        if self.frameUnit then
            if self:Busy() then self.viewed = self.frameUnit else self:ClickFrame(self.frameUnit) end
            return true
        end
        if self:Busy() then
            if self.hoverUnit then self.viewed = self.hoverUnit end
            return true
        end
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
    local budget = Rules.MoveBudget(self.card.card)
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
    self:UpdateHud()
    if self.state == "loading" then return end

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
        local r = Rules.Range(self.card.card)
        self:DrawCover(COLORS.attack)
        for _, v in ipairs(self:Living()) do
            if v.team ~= u.team and Rules.Reaches(r, u, v) then
                GroundCircle(v.x, v.y, v.z, Rules.UNIT_RADIUS + 6, COLORS.target)
            end
        end
    elseif u and self.mode == "bolt" then
        self:DrawCover(COLORS.bolt)
        local at = self:BoltTarget()
        if at then
            local radius = Rules.SplashRadius(self.card.card)
            GroundDisc(at.x, at.y, at.z, radius, COLORS.splash)
            GroundCircle(at.x, at.y, at.z, radius, COLORS.splashEdge)
            local sx, sy = Board.Lift(u.x, u.y, u.z)
            local tx, ty = Board.Lift(at.x, at.y, at.z)
            DrawLine(sx, sy, tx, ty, COLORS.boltEdge, "overlay")
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

end

function Battle:Forecast()
    local u, v = self.selected, self.hoverUnit
    if not u or not self.card then return nil end
    local card = self.card.card
    if self.mode == "attack" and v and v.team ~= u.team and Rules.Reaches(Rules.Range(card), u, v) then
        return string.format("%s -> %s: %d damage", card.name, v.name, Rules.Damage(u, v, Rules.Power(u, card)))
    elseif self.mode == "bolt" then
        local at = self:BoltTarget()
        if not at then return nil end
        local total, n = 0, 0
        for _, w in ipairs(Rules.Splash(self.units, at, Rules.SplashRadius(card))) do
            if w.team ~= u.team then
                total, n = total + Rules.Damage(u, w, Rules.SplashPower(u, card, w, at)), n + 1
            end
        end
        return n > 0 and string.format("%s hits %d for %d total", card.name, n, total) or "No targets"
    elseif self.mode == "move" and self.preview then
        if self.preview.out then return "Too far" end
        return string.format("Move %d / %d", math.floor(self.preview.cost), Rules.MoveBudget(card))
    end
    return nil
end

-- --- HUD ---
-- The HUD is prefab placements in Scenes/Battle.xml (TurnLabel, Forecast, the hero
-- frames, the Hand, EndTurn and the Banner); this binds the Hand and the button and fills the
-- rest in each frame.

function Battle:BindHud()
    Widgets.ResetHover()
    self.hud = {}
    self.flights = {}   -- played cards for the Hand to fly out (Battle:Commit)
    Widgets.BindButton("Wait",
        function() self:WaitUnit() end,
        function() return not self:Busy() and self.selected ~= nil and not self.card end)
    Widgets.BindButton("EndTurn",
        function() self:EndPlayerPhase() end,
        function() return not self:Busy() end)
    Widgets.BindView("Hand", {
        show = function()
            local u = self.state ~= "loading" and self:HandUnit()
            if not u then return nil end
            return { cards = u.piles.hand, faceUp = u.team == self.me, owner = u.name }
        end,
        playable = function(slot)
            local u = self:HandUnit()
            local card = u and u.piles.hand[slot]
            return card ~= nil and u == self.selected and not self:Busy() and Cards.Affordable(u, card)
        end,
        chosen = function() return self.card and self.card.slot end,
        -- Out of the way while a card is aimed or playing out.
        hidden = function() return self.card ~= nil or (self.co ~= nil and self.phase == self.me) end,
        -- The other side's phase: just the tops of the cards, to keep the field clear.
        peek = function() return self.phase ~= self.me end,
        onPlay = function(slot) self:PlayCard(slot) end,
        flights = self.flights,
    })
end

-- A placement's root by its id, looked up once.
function Battle:Hud(id)
    local e = self.hud[id]
    if not e then
        e = Widgets.Find(id)
        self.hud[id] = e
    end
    return e
end

local function WithAlpha(c, a) return { r = c.r, g = c.g, b = c.b, a = a } end

-- The HUD is laid out for the layout size; on a bigger screen each piece keeps to its edge.
-- (The hero frames keep to theirs themselves: Scripts/Components/HeroFrame.lua.)
function Battle:AnchorHud()
    Widgets.Anchor(self:Hud("TurnLabel"), "left", "bottom")
    Widgets.Anchor(self:Hud("Forecast"), "center", "bottom")
    Widgets.Anchor(self:Hud("Wait"), "right", "bottom")
    Widgets.Anchor(self:Hud("EndTurn"), "right", "bottom")
    Widgets.Anchor(self:Hud("Hand"), "center", "bottom")
    local banner = self:Hud("Banner")
    Widgets.Anchor(banner, "stretch", "middle")
    for _, name in ipairs({ "Text", "Sub" }) do Widgets.Anchor(banner and Widgets.Child(banner, name), "center") end
end

function Battle:UpdateHud()
    local playing = self.state == "battle" or self.state == "over"
    self:AnchorHud()

    local label = self:Hud("TurnLabel")
    Widgets.SetVisible(label, playing)
    if label and playing then
        local phase
        if self.phase == self.me then phase = self.net and "Your phase" or "Player phase"
        else phase = self.net and "Opponent's phase" or "Enemy phase" end
        Widgets.ChildText(label, "Turn", string.format("Turn %d  -  %s", math.max(1, self.turn or 1), phase), nil, 16)
    end

    local forecast = playing and self:Forecast() or nil
    local box = self:Hud("Forecast")
    Widgets.SetVisible(box, forecast ~= nil)
    if box and forecast then Widgets.ChildText(box, "Text", forecast) end

    -- The turn buttons step aside while a card is aimed.
    local ours = playing and self.phase == self.me and self.state == "battle" and not self.card
    Widgets.SetVisible(self:Hud("EndTurn"), ours)
    Widgets.SetVisible(self:Hud("Wait"), ours and self.selected ~= nil)

    self:UpdateBanner()
end

-- The banner: the current announcement fading in and out, or the loading notice.
function Battle:UpdateBanner()
    local root = self:Hud("Banner")
    if not root then return end
    local b = self.banner
    if self.state == "loading" then b = { text = "Reading the battlefield...", t = 1 } end

    -- The hero frames fade out while a turn's banner is up, and back after (a root's
    -- opacity fades everything under it).
    local turnBanner = b ~= nil and b.duration ~= nil
    self.framesOpacity = Widgets.Ease(self.framesOpacity or 1, turnBanner and 0 or 1, 10, self.dt or 0)
    for _, frame in ipairs(HeroFrames and HeroFrames.Frames() or {}) do
        local layer = GetComponent(frame, "Layer")
        if layer then layer.opacity = self.framesOpacity end
    end
    Widgets.SetVisible(root, b ~= nil)
    if not b then return end
    local a = b.duration and math.floor(255 * math.min(1, (b.duration - b.t) * 3, b.t * 4)) or 255
    a = math.max(0, math.min(255, a))
    Widgets.SetFill(root, {r = 8, g = 8, b = 14, a = math.floor(a * 0.8)})
    local mat = GetComponent(root, "Material")
    local fire = mat and mat:Layer("Fire")
    if fire then fire:Set("uIntensity", 0.6 * a / 255) end
    Widgets.ChildText(root, "Text", b.text, WithAlpha({r = 255, g = 215, b = 120}, a))
    Widgets.ChildText(root, "Sub", b.sub or "", WithAlpha({r = 220, g = 220, b = 230}, a))
end

return Battle
