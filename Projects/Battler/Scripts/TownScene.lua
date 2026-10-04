---@type SceneScript
-- Town: the party's home between encounters (Scripts/Battler/Run.lua), roamed freely on the
-- navmesh. The Portal (Prefabs/Portal.xml) leads to the next encounter: walking into it stops
-- the party for the encounter's intro, then a click starts the fight in the Battle scene,
-- which comes back here win or lose, to where the party stood. Later: shops and Contracts.
-- Left-click a party member to select it, right-click the floor to walk there; the rest of the
-- party follows the leader. Wheel zooms, middle-drag / WASD pans, Esc leaves for the menu.
local Board = require("Scripts/Battler/Board")
local Run = require("Scripts/Battler/Run")
local Units = require("Scripts/Battler/Units")
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

local CAMERA_SPEED = 300
local ZOOM_STEP = 1.15     -- per wheel notch
local ZOOM_MIN, ZOOM_MAX = 0.6, 2.5
local ZOOM_SMOOTH = 14     -- how fast zoom converges on the target, per second

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
    self.cameraEntity, self.panAnchor, self.zoomTarget, self.zoomOffset = nil, nil, nil, nil

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
    if at then self:CenterOn(at) end

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
    if self.intro then self.intro.t = self.intro.t + dt else self:PanWithKeys(dt) end
    self:UpdateCamera(dt)

    for _, u in ipairs(self.units) do
        self:Walk(u, dt)
        Units.Update(u, dt)
        Units.DrawRing(u, u == self.selected, self.time)
    end
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
    for _, u in ipairs(self.party) do
        if u.path then
            u.path = nil
            Units.Play(u, "Idle")
        end
    end
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
    DrawText("Left-click: select   Right-click: move   Esc: leave", 420, 9, 16, DIM, "ui", FONT)

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
    if event.type ~= "MouseButtonPressed" then return false end

    local w = ScreenToWorld(GetMousePosition())
    if event.button == MOUSE_LEFT then
        local u = self:PartyMemberAt(w.x, w.y)
        if u then
            if u ~= self.selected then Sfx.Play(Sfx.SELECT) end
            self.selected = u
        end
        return u ~= nil
    elseif event.button == MOUSE_RIGHT and self.selected then
        local p = NavPick(w.x, w.y)
        if p then self:MoveParty(self.selected, p) end
        return true
    end
    return false
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

-- The leader walks to `to`; each other member walks to just behind the one before it.
function Town:MoveParty(leader, to)
    self:Send(leader, to)
    local ahead = to
    for _, u in ipairs(self.party) do
        if u ~= leader then
            local dx, dy = u.x - ahead.x, u.y - ahead.y
            local d = math.max(1, math.sqrt(dx * dx + dy * dy))
            local spot = NavPick(ahead.x + dx / d * FOLLOW_GAP, ahead.y + dy / d * FOLLOW_GAP) or ahead
            self:Send(u, spot)
            ahead = spot
        end
    end
end

function Town:Send(u, to)
    local path = NavFindPath(u.x, u.y, to.x, to.y, u.z)
    if #path == 0 then return end
    u.path, u.leg = path, 1
    if u.clip ~= "Walk" then Units.Play(u, "Walk") end
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
            if u.leg > #u.path then
                u.path = nil
                Units.Play(u, "Idle")
            end
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

-- Puts the camera over a ground point (coming back from a fight).
function Town:CenterOn(p)
    if not self:Camera() then return end
    local t = GetComponent(self.cameraEntity, "Transform")
    if t then t.localX, t.localY = p.x, p.y end
end

function Town:PanWithKeys(dt)
    local dx, dy = 0, 0
    if IsKeyDown(KEY_W) then dy = dy - 1 end
    if IsKeyDown(KEY_S) then dy = dy + 1 end
    if IsKeyDown(KEY_A) then dx = dx - 1 end
    if IsKeyDown(KEY_D) then dx = dx + 1 end
    if (dx ~= 0 or dy ~= 0) and self:Camera() then
        local pos = GetComponent(self.cameraEntity, "Transform")
        if pos then
            pos.localX = pos.localX + dx * CAMERA_SPEED * dt
            pos.localY = pos.localY + dy * CAMERA_SPEED * dt
        end
    end
end

-- Wheel zooms about the cursor (eased toward a target) and middle-drag pans, like the editor.
function Town:UpdateCamera(dt)
    if not self:Camera() then return end
    local transform = GetComponent(self.cameraEntity, "Transform")
    local camera = GetComponent(self.cameraEntity, "Camera")
    if not transform or not camera then return end

    self.zoomTarget = self.zoomTarget or camera.zoom
    local wheel = GetMouseWheelMove()
    if wheel ~= 0 then
        self.zoomTarget = math.max(ZOOM_MIN, math.min(ZOOM_MAX, self.zoomTarget * ZOOM_STEP ^ wheel))
        local under = ScreenToWorld(GetMousePosition())
        self.zoomOffset = { x = (under.x - transform.localX) * camera.zoom,
                            y = (under.y - transform.localY) * camera.zoom }
    end

    if math.abs(self.zoomTarget - camera.zoom) > 0.0005 then
        local zoom = camera.zoom + (self.zoomTarget - camera.zoom) * (1 - math.exp(-ZOOM_SMOOTH * dt))
        local offset = self.zoomOffset
        if offset then
            -- Hold the cursor's world point still across the change.
            local worldX = transform.localX + offset.x / camera.zoom
            local worldY = transform.localY + offset.y / camera.zoom
            transform.localX = worldX - offset.x / zoom
            transform.localY = worldY - offset.y / zoom
        end
        camera.zoom = zoom
    else
        camera.zoom = self.zoomTarget
        self.zoomOffset = nil
    end

    local mouse = GetMousePosition()
    if IsMouseButtonDown(MOUSE_MIDDLE) then
        if self.panAnchor then
            transform.localX = transform.localX - (mouse.x - self.panAnchor.x) / camera.zoom
            transform.localY = transform.localY - (mouse.y - self.panAnchor.y) / camera.zoom
        end
        self.panAnchor = { x = mouse.x, y = mouse.y }
    else
        self.panAnchor = nil
    end
end

return Town
