---@type SceneScript
-- Dungeon: a floor of the run (Scripts/Battler/Run.lua), roamed freely on the navmesh. The
-- party walks the rooms and passages; each EncounterN entity in Scenes/Dungeon.xml is the door
-- to the floor's Nth encounter. Walking into an uncleared one stops the party for its intro,
-- then a click starts the fight in the Battle scene, which comes back here on a win (to where
-- the fight began, that encounter cleared) or ends the run on a loss. Winning the floor's last
-- encounter goes on to the Campfire instead.
-- Left-click a party member to select it, right-click the floor to walk there; the rest of the
-- party follows the leader. Wheel zooms, middle-drag / WASD pans, Esc abandons the run.
local Board = require("Scripts/Battler/Board")
local Run = require("Scripts/Battler/Run")
local Units = require("Scripts/Battler/Units")
local Music = require("Scripts/Menu/Music")
local Sfx = require("Scripts/Menu/Sfx")

local Dungeon = {}

local PLAYER = Units.PLAYER
local FONT = "Fonts/EnchantedLand-Regular.ttf"
local SPEED = 215          -- ground units a second, as in battle
local PICK_RADIUS = 30     -- how close a click must land to a unit's feet
local FOLLOW_GAP = 40      -- how far behind the one ahead a follower stops
local TRIGGER = 70         -- how close a party member gets to an encounter to walk into it
local DOOR_RADIUS = 60     -- the ring drawn around an encounter
local INTRO_DELAY = 0.4    -- seconds before the intro takes a click (not the one that walked in)

local CAMERA_SPEED = 300
local ZOOM_STEP = 1.15     -- per wheel notch
local ZOOM_MIN, ZOOM_MAX = 0.6, 2.5
local ZOOM_SMOOTH = 14     -- how fast zoom converges on the target, per second

-- Where the party starts a floor, in Run.PARTY order.
local START = { { -20.6, -19.5 }, { -139.7, 19.0 }, { -47.2, 63.0 } }

local TEXT = {r = 235, g = 235, b = 240, a = 255}
local DIM = {r = 160, g = 160, b = 175, a = 255}
local GOLD = {r = 255, g = 210, b = 110, a = 255}
local PANEL = {r = 14, g = 14, b = 22, a = 210}
local DOOR = {r = 255, g = 90, b = 60, a = 230}
local DOOR_CLEARED = {r = 120, g = 120, b = 135, a = 140}

local function GroundDistance(a, b) return NavGroundDistance(a.x, a.y, b.x, b.y) end
local function WithAlpha(c, a) return { r = c.r, g = c.g, b = c.b, a = a } end

-- --- Lifecycle ----------------------------------------------------------------------------

function Dungeon:Initialize()
    Music.Play(Music.MENU)
    if not Run.Active() then Run.New() end   -- opened straight from the editor
    self.time = 0
    self.party, self.units = {}, {}
    self.intro = nil   -- { door, t }: the encounter just walked into, before its fight
    self.cameraEntity, self.panAnchor, self.zoomTarget, self.zoomOffset = nil, nil, nil, nil

    -- Back from a fight, the party stands where it began; otherwise at the floor's start.
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

    -- The doors: EncounterN is the floor's Nth slot.
    self.doors = {}
    for k, slot in ipairs(Run.Slots()) do
        local e = GetEntityByName("Encounter" .. k)
        local t = e and GetComponent(e, "Transform")
        if t then
            local z = NavFloorHeight(t.localX, t.localY, 0) or 0
            self.doors[#self.doors + 1] = { slot = k, x = t.localX, y = t.localY, z = z, encounter = slot.encounter }
        else
            Log("Dungeon: no Encounter" .. k .. " in the scene")
        end
    end
end

function Dungeon:Spawn(name, x, y)
    local u = Units.Spawn(name, PLAYER, { x = x, y = y, z = NavFloorHeight(x, y, 0) or 0 })
    if not u then return end
    local wounds = Run.Wounds(u.name)
    if wounds then u.hp, u.shownHp = math.min(u.hp, wounds), math.min(u.hp, wounds) end
    u.path, u.leg = nil, 0
    self.party[#self.party + 1] = u
    self.units[#self.units + 1] = u
end

function Dungeon:Update(dt)
    self.time = self.time + dt
    if self.intro then self.intro.t = self.intro.t + dt else self:PanWithKeys(dt) end
    self:UpdateCamera(dt)

    for _, u in ipairs(self.units) do
        self:Walk(u, dt)
        Units.Update(u, dt)
        Units.DrawRing(u, u == self.selected, self.time)
    end
    if not self.intro then self:CheckDoors() end
end

-- --- Encounters ---------------------------------------------------------------------------

function Dungeon:Cleared(door) return Run.Slots()[door.slot].cleared end

-- A party member inside an uncleared door's ring walks the party into it.
function Dungeon:CheckDoors()
    for _, door in ipairs(self.doors) do
        if not self:Cleared(door) then
            for _, u in ipairs(self.party) do
                if GroundDistance(u, door) <= TRIGGER then
                    self:Enter(door)
                    return
                end
            end
        end
    end
end

-- The party stops, and the encounter's intro comes up.
function Dungeon:Enter(door)
    for _, u in ipairs(self.party) do
        if u.path then
            u.path = nil
            Units.Play(u, "Idle")
        end
    end
    Sfx.Play(Sfx.SELECT)
    self.intro = { door = door, t = 0 }
end

-- From the intro into the fight. The party comes back to where its leader stands now.
function Dungeon:Fight()
    local lead = self.selected or self.party[1]
    Run.Enter(self.intro.door.slot, { x = lead.x, y = lead.y })
    Sfx.Play(Sfx.ORDER)
    SceneReplace("Battle")
end

-- A ring on the floor around each door, burning while it waits and grey once cleared, and
-- its name over it.
function Dungeon:DrawDoors()
    for _, door in ipairs(self.doors) do
        local cleared = self:Cleared(door)
        local r = DOOR_RADIUS + (cleared and 0 or 4 * math.sin(self.time * 3))
        local color = cleared and DOOR_CLEARED or DOOR
        local px, py
        for k = 0, 40 do
            local a = k / 40 * math.pi * 2
            local qx, qy = Board.Lift(door.x + math.cos(a) * r, door.y + math.sin(a) * r * 0.5, door.z)
            if px then DrawLine(px, py, qx, qy, color, "selection") end
            px, py = qx, qy
        end
        local sx, sy = ViewProject(door.x, door.y, door.z + 90)
        local label = cleared and "Cleared" or door.encounter.title
        DrawText(label, sx - #label * 4, sy, 20, cleared and DIM or GOLD, "ui", FONT)
    end
end

function Dungeon:Render()
    self:DrawDoors()
    FillRect(0, 0, 1280, 32, PANEL, "ui")
    DrawText(string.format("Floor %d of %d", Run.Floor(), Run.FLOORS), 16, 6, 22, TEXT, "ui", FONT)
    DrawText(string.format("Encounters %d / %d", Run.Cleared(), #Run.Slots()), 170, 8, 18, GOLD, "ui", FONT)
    DrawText("Left-click: select   Right-click: move   Esc: abandon the run", 420, 9, 16, DIM, "ui", FONT)

    -- The encounter's intro, before the fight.
    local intro = self.intro
    if intro then
        local a = math.floor(255 * math.min(1, intro.t * 3))
        local e = intro.door.encounter
        FillRect(240, 250, 800, 190, {r = 8, g = 8, b = 14, a = math.floor(a * 0.85)}, "ui")
        DrawText(e.title, 280, 270, 44, WithAlpha(GOLD, a), "ui", FONT)
        DrawText(e.intro or "", 280, 334, 22, WithAlpha(TEXT, a), "ui", FONT)
        DrawText("Click to fight", 280, 396, 20, WithAlpha(DIM, a), "ui", FONT)
    end
end

function Dungeon:OnEvent(event)
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

function Dungeon:PartyMemberAt(x, y)
    local best, bestD
    for _, u in ipairs(self.party) do
        local d = GroundDistance(u, { x = x, y = y })
        if d <= PICK_RADIUS and (not bestD or d < bestD) then best, bestD = u, d end
    end
    return best
end

-- The leader walks to `to`; each other member walks to just behind the one before it.
function Dungeon:MoveParty(leader, to)
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

function Dungeon:Send(u, to)
    local path = NavFindPath(u.x, u.y, to.x, to.y, u.z)
    if #path == 0 then return end
    u.path, u.leg = path, 1
    if u.clip ~= "Walk" then Units.Play(u, "Walk") end
end

-- Advances along the path at a steady ground speed, turning to face each leg.
function Dungeon:Walk(u, dt)
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
function Dungeon:Camera()
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
function Dungeon:CenterOn(p)
    if not self:Camera() then return end
    local t = GetComponent(self.cameraEntity, "Transform")
    if t then t.localX, t.localY = p.x, p.y end
end

function Dungeon:PanWithKeys(dt)
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
function Dungeon:UpdateCamera(dt)
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

return Dungeon
