---@type SceneScript
-- Town: the party's home between encounters (Scripts/Battler/Run.lua), roamed freely on the
-- navmesh. The Portal (Prefabs/Portal.xml) leads to the next encounter: walking into it stops
-- the party for the encounter's intro, then a click starts the fight in the Battle scene,
-- which comes back here win or lose, to where the party stood. Later: shops and Contracts.
-- The camera rides over the leader's shoulder. WASD walks the leader, relative to the camera
-- (W away from it), sliding along walls (NavSlide); the rest of the party trails behind it,
-- each pathing after the one ahead. Right-drag turns and tilts the camera, which swings back
-- behind the leader once it walks forward again; the wheel zooms. Left-click a party member to
-- lead with it. I opens the party's character sheet (Scenes/CharacterSheet.xml) over the
-- Town. Esc leaves for the menu.
local Board = require("Scripts/Battler/Board")
local Run = require("Scripts/Battler/Run")
local Units = require("Scripts/Battler/Units")
local Widgets = require("Scripts/Components/Widgets")
local Music = require("Scripts/Menu/Music")
local Sfx = require("Scripts/Menu/Sfx")

local Town = {}

local PLAYER = Units.PLAYER
local FONT = "Fonts/EnchantedLand-Regular.ttf"
local SPEED = 215          -- ground units a second, as in battle
local PICK_RADIUS = 30     -- how close a click must land to a unit's feet
local FOLLOW_GAP = 40      -- how far behind the one ahead a follower stops
local TRIGGER = 70         -- how close a party member gets to the Portal to walk into it
local PORTAL_RADIUS = 60   -- the ring drawn around the Portal
local INTRO_DELAY = 0.4    -- seconds before the intro takes a click (not the one that walked in)

local REPATH = 0.25        -- seconds between a follower's paths after the one ahead
local CLIMB = 12           -- how fast a steered unit's height eases onto the floor's, per second
-- A floor at height z is drawn z * cos(pitch) above its ground position; NavFindPath takes
-- its goal as a point in that picture.
local PICTURE_LIFT = math.cos(math.rad(30))

local ZOOM_STEP = 1.15     -- per wheel notch
local ZOOM_MIN, ZOOM_MAX = 1.0, 4.0
local ZOOM_START = 2.2
local ZOOM_SMOOTH = 14     -- how fast zoom converges on the target, per second
local PITCH_START = 28     -- degrees down from the horizon
local PITCH_MIN, PITCH_MAX = 10, 65
local ORBIT_SPEED = 0.3    -- degrees per pixel of right-drag
local FOLLOW_SMOOTH = 8    -- how fast the camera catches up with the leader, per second
local RECENTER_DELAY = 0.8 -- seconds after a drag before it swings back behind the leader
local RECENTER_SMOOTH = 3  -- how fast it swings back, per second
local LEAD = 30            -- how far ahead of the leader the camera looks

-- Where the party starts, in Run.PARTY order.
local START = { { -20.6, -19.5 }, { -139.7, 19.0 }, { -47.2, 63.0 } }

local TEXT = {r = 235, g = 235, b = 240, a = 255}
local DIM = {r = 160, g = 160, b = 175, a = 255}
local GOLD = {r = 255, g = 210, b = 110, a = 255}
local PANEL = {r = 14, g = 14, b = 22, a = 210}
local RING = {r = 170, g = 120, b = 255, a = 230}

local function GroundDistance(a, b) return NavGroundDistance(a.x, a.y, b.x, b.y) end
local function WithAlpha(c, a) return { r = c.r, g = c.g, b = c.b, a = a } end

-- --- Lifecycle ----------------------------------------------------------------------------

function Town:Initialize()
    Music.Play(Music.MENU)
    if not Run.Active() then Run.New() end   -- opened straight from the editor
    self.time = 0
    self.party, self.units = {}, {}
    self.intro = nil   -- { t }: the Portal just walked into, before the fight
    self.cameraEntity, self.orbitAnchor, self.zoomTarget = nil, nil, nil
    self.sinceOrbit, self.snapCamera, self.forward = RECENTER_DELAY, true, false

    -- Back from a fight, the party stands where it left; otherwise at the start.
    local at = Run.State().at
    for k, name in ipairs(Run.PARTY) do
        local x, y = START[k][1], START[k][2]
        if at then
            local spot = NavPick(at.x + (k - 1) * 30, at.y + (k - 1) * 18)
            x, y = spot and spot.x or at.x, spot and spot.y or at.y
        end
        self:Spawn(name, x, y)
    end
    self.selected = self.party[1]

    local e = GetEntityByName("Portal")
    local t = e and GetComponent(e, "Transform")
    if t then
        self.portal = { x = t.localX, y = t.localY, z = NavFloorHeight(t.localX, t.localY, 0) or 0 }
    else
        self.portal = nil
        Log("Town: no Portal in the scene")
    end
end

function Town:Spawn(name, x, y)
    local u = Units.Spawn(name, PLAYER, { x = x, y = y, z = NavFloorHeight(x, y, 0) or 0 })
    if not u then return end
    u.path, u.leg = nil, 0
    self.party[#self.party + 1] = u
    self.units[#self.units + 1] = u
end

function Town:Update(dt)
    self.time = self.time + dt
    if self.intro then self.intro.t = self.intro.t + dt else self:Steer(self.selected, dt) end

    local ahead = self.selected
    for _, u in ipairs(self.party) do
        if u ~= self.selected then
            if not self.intro then self:Follow(u, ahead, dt) end
            ahead = u
        end
    end
    for _, u in ipairs(self.units) do
        self:Walk(u, dt)
        Units.Update(u, dt)
        Units.DrawRing(u, u == self.selected, self.time)
    end
    self:UpdateCamera(dt)
    if not self.intro then self:CheckPortal() end
end

-- --- The Portal ---------------------------------------------------------------------------

-- A party member inside the Portal's ring walks the party into it.
function Town:CheckPortal()
    if not self.portal then return end
    for _, u in ipairs(self.party) do
        if GroundDistance(u, self.portal) <= TRIGGER then
            self:Enter()
            return
        end
    end
end

-- The party stops, and the next encounter's intro comes up.
function Town:Enter()
    for _, u in ipairs(self.party) do u.path = nil end
    Sfx.Play(Sfx.SELECT)
    self.intro = { t = 0 }
end

-- From the intro into the fight. The party comes back to where its leader stands now, a
-- step back from the Portal so it doesn't walk straight in again.
function Town:Fight()
    local lead = self.selected or self.party[1]
    local dx, dy = lead.x - self.portal.x, lead.y - self.portal.y
    local d = math.max(1, math.sqrt(dx * dx + dy * dy))
    local back = (TRIGGER + 60) / d
    Run.Enter({ x = self.portal.x + dx * back, y = self.portal.y + dy * back })
    Sfx.Play(Sfx.ORDER)
    SceneReplace("Battle")
end

-- A pulsing ring on the floor around the Portal, and the next encounter's name over it.
function Town:DrawPortal()
    local p = self.portal
    if not p then return end
    local r = PORTAL_RADIUS + 4 * math.sin(self.time * 3)
    local px, py
    for k = 0, 40 do
        local a = k / 40 * math.pi * 2
        local qx, qy = Board.Lift(p.x + math.cos(a) * r, p.y + math.sin(a) * r * 0.5, p.z)
        if px then DrawLine(px, py, qx, qy, RING, "selection") end
        px, py = qx, qy
    end
    local e = Run.Next()
    if e then
        local sx, sy = ViewProject(p.x, p.y, p.z + 130)
        DrawText(e.title, sx - #e.title * 4, sy, 20, GOLD, "ui", FONT)
    end
end

function Town:Render()
    self:DrawPortal()
    local sw, sh = GetScreenSize()
    FillRect(0, 0, sw, 32, PANEL, "ui")
    DrawText("Town", 16, 6, 22, TEXT, "ui", FONT)
    DrawText(string.format("Victories %d", Run.Wins()), 110, 8, 18, GOLD, "ui", FONT)
    DrawText("WASD: move   Right-drag: look   Wheel: zoom   Left-click: lead   I: character   Esc: leave", 420, 9, 16, DIM, "ui", FONT)

    -- The encounter's intro, before the fight.
    local intro, e = self.intro, Run.Next()
    if intro and e then
        local a = math.floor(255 * math.min(1, intro.t * 3))
        local x, y = sw / 2 - 400, sh / 2 - 110   -- an 800 x 190 panel in the middle
        FillRect(x, y, 800, 190, {r = 8, g = 8, b = 14, a = math.floor(a * 0.85)}, "ui")
        DrawText(e.title, x + 40, y + 20, 44, WithAlpha(GOLD, a), "ui", FONT)
        DrawText(e.intro or "", x + 40, y + 84, 22, WithAlpha(TEXT, a), "ui", FONT)
        DrawText("Click to fight", x + 40, y + 146, 20, WithAlpha(DIM, a), "ui", FONT)
    end
end

function Town:OnEvent(event)
    if event.type == "KeyPressed" and event.key == KEY_ESCAPE then
        Sfx.Play(Sfx.CANCEL)
        Run.Clear()
        SceneReplace("MainMenu")
        return true
    end
    if self.intro then
        local go = (event.type == "MouseButtonPressed" and event.button == MOUSE_LEFT)
            or (event.type == "KeyPressed" and (event.key == KEY_SPACE or event.key == KEY_ENTER))
        if go and self.intro.t > INTRO_DELAY then self:Fight() end
        return true
    end
    if event.type == "KeyPressed" and event.key == KEY_I then
        self:OpenCharacterSheet()
        return true
    end
    if event.type ~= "MouseButtonPressed" then return false end

    local w = ScreenToWorld(GetMousePosition())
    if event.button == MOUSE_LEFT then
        local u = self:PartyMemberAt(w.x, w.y)
        if u then
            if u ~= self.selected then Sfx.Play(Sfx.SELECT) end
            self.selected = u
        end
        return u ~= nil
    end
    return false
end

-- Opens on the leader.
function Town:OpenCharacterSheet()
    local selected = 1
    for k, u in ipairs(self.party) do
        if u == self.selected then selected = k end
    end
    Widgets.BindView("CharacterSheet", { selected = selected })
    Sfx.Play(Sfx.SELECT)
    ScenePush("CharacterSheet")
end

-- --- Movement -----------------------------------------------------------------------------

function Town:PartyMemberAt(x, y)
    local best, bestD
    for _, u in ipairs(self.party) do
        local d = GroundDistance(u, { x = x, y = y })
        if d <= PICK_RADIUS and (not bestD or d < bestD) then best, bestD = u, d end
    end
    return best
end

-- The camera's ground axes, each one ground unit long (a picture y counts twice an x):
-- forward, away from the camera, and right.
function Town:CameraAxes()
    local camera = self:Camera() and GetComponent(self.cameraEntity, "Camera")
    local yaw = math.rad(camera and camera.yaw or 0)
    return math.sin(yaw), -math.cos(yaw) / 2, math.cos(yaw), math.sin(yaw) / 2
end

-- WASD walks the leader, relative to the camera; the navmesh stops it at walls and slides it
-- along them.
function Town:Steer(u, dt)
    if not u then return end
    u.path = nil
    local f, r = 0, 0
    if IsKeyDown(KEY_W) then f = f + 1 end
    if IsKeyDown(KEY_S) then f = f - 1 end
    if IsKeyDown(KEY_D) then r = r + 1 end
    if IsKeyDown(KEY_A) then r = r - 1 end
    self.forward = f > 0
    if f == 0 and r == 0 then return end
    local fx, fy, rx, ry = self:CameraAxes()
    local dx, dy = fx * f + rx * r, fy * f + ry * r
    local step = SPEED * dt / math.sqrt(dx * dx + 4 * dy * dy)
    dx, dy = dx * step, dy * step
    Units.Face(u, u.x + dx, u.y + dy)
    local x, y, z = NavSlide(u.x, u.y, u.z, dx, dy)
    u.x, u.y = x, y
    -- The floor's height comes a cell at a time (a stair is a step); ease onto it.
    u.z = u.z + (z - u.z) * math.min(1, CLIMB * dt)
end

-- A follower paths after `ahead` (the party member in front of it), every REPATH seconds
-- while it's out of reach, and stops FOLLOW_GAP behind it.
function Town:Follow(u, ahead, dt)
    if GroundDistance(u, ahead) <= FOLLOW_GAP then
        u.path = nil
        return
    end
    u.repath = (u.repath or 0) - dt
    if u.path and u.repath > 0 then return end
    u.repath = REPATH
    local path = NavFindPath(u.x, u.y, ahead.x, ahead.y - ahead.z * PICTURE_LIFT, u.z)
    u.path, u.leg = #path > 0 and path or nil, 1
end

-- Advances along the path at a steady ground speed, turning to face each leg.
function Town:Walk(u, dt)
    if not u.path then return end
    local step = SPEED * dt
    while step > 0 and u.path do
        local p = u.path[u.leg]
        Units.Face(u, p.x, p.y)
        local d = GroundDistance(u, p)
        if d <= step then
            u.x, u.y, u.z = p.x, p.y, p.z
            step = step - d
            u.leg = u.leg + 1
            if u.leg > #u.path then u.path = nil end
        else
            local k = step / d
            u.x, u.y, u.z = u.x + (p.x - u.x) * k, u.y + (p.y - u.y) * k, u.z + (p.z - u.z) * k
            step = 0
        end
    end
end

-- --- Camera -------------------------------------------------------------------------------

-- Resolved lazily rather than cached once, so the camera can also arrive from a prefab.
function Town:Camera()
    if self.cameraEntity and HasComponent(self.cameraEntity, "Camera") then return self.cameraEntity end
    local cam = GetEntityByName("CAMERA")
    if not cam then
        local found = FindEntitiesWithComponent("Camera")
        cam = found and found[1]
    end
    self.cameraEntity = cam
    return cam
end

local function Ease(from, to, rate, dt) return from + (to - from) * (1 - math.exp(-rate * dt)) end

-- The way from `from` to `to` in degrees, the short way round: -180 to 180.
local function Turn(from, to) return (to - from + 180) % 360 - 180 end

-- Over the leader's shoulder: it follows the leader, looking a little ahead of it. Right-drag
-- turns (across) and tilts (down) it; walking forward swings it back behind the leader once
-- the drag has settled. The wheel zooms (a perspective camera's zoom dollies it).
function Town:UpdateCamera(dt)
    local lead = self.selected
    if not self:Camera() or not lead then return end
    local transform = GetComponent(self.cameraEntity, "Transform")
    local camera = GetComponent(self.cameraEntity, "Camera")
    if not transform or not camera then return end

    if self.snapCamera then
        camera.pitch, camera.zoom, self.zoomTarget = PITCH_START, ZOOM_START, ZOOM_START
        -- A unit's yaw is 180 off the yaw of a camera looking the way it faces (Units.Face).
        camera.yaw = (lead.yaw or 180) - 180
    end

    local wheel = GetMouseWheelMove()
    if wheel ~= 0 then
        self.zoomTarget = math.max(ZOOM_MIN, math.min(ZOOM_MAX, self.zoomTarget * ZOOM_STEP ^ wheel))
    end
    camera.zoom = Ease(camera.zoom, self.zoomTarget, ZOOM_SMOOTH, dt)

    local mouse = GetMousePosition()
    if IsMouseButtonDown(MOUSE_RIGHT) and not self.intro then
        if self.orbitAnchor then
            camera.yaw = camera.yaw + (mouse.x - self.orbitAnchor.x) * ORBIT_SPEED
            camera.pitch = math.max(PITCH_MIN, math.min(PITCH_MAX, camera.pitch + (mouse.y - self.orbitAnchor.y) * ORBIT_SPEED))
        end
        self.orbitAnchor = { x = mouse.x, y = mouse.y }
        self.sinceOrbit = 0
    else
        self.orbitAnchor = nil
        self.sinceOrbit = self.sinceOrbit + dt
        if self.forward and self.sinceOrbit >= RECENTER_DELAY and lead.yaw then
            camera.yaw = camera.yaw + Turn(camera.yaw, lead.yaw - 180) * (1 - math.exp(-RECENTER_SMOOTH * dt))
        end
    end
    camera.yaw = camera.yaw % 360

    -- The camera looks at a point on the ground (height 0). For a leader up on a floor, that's
    -- where its sight line through the leader comes down, further ahead.
    local fx, fy = self:CameraAxes()
    local ahead = LEAD + lead.z / math.tan(math.rad(math.max(camera.pitch, 5)))
    local x, y = lead.x + fx * ahead, lead.y + fy * ahead
    if self.snapCamera then
        transform.localX, transform.localY = x, y
        self.snapCamera = false
    else
        transform.localX = Ease(transform.localX, x, FOLLOW_SMOOTH, dt)
        transform.localY = Ease(transform.localY, y, FOLLOW_SMOOTH, dt)
    end
end

return Town
