---@type SceneScript
-- Overworld: free roam on the navmesh with the battler's 3D characters (Character.xml through
-- Units). Left-click a party member to select it, right-click the floor to walk there; the rest
-- of the party follows the leader. A band of brigands idles to the east (the battle trigger,
-- once transitions are in). Wheel zooms, middle-drag / WASD pans, Esc goes back to the menu.
local Units = require("Scripts/Battler/Units")

local Overworld = {}

local PLAYER, ENEMY = Units.PLAYER, Units.ENEMY
local FONT = "Fonts/EnchantedLand-Regular.ttf"
local SPEED = 215          -- ground units a second, as in battle
local PICK_RADIUS = 30     -- how close a click must land to a unit's feet
local FOLLOW_GAP = 40      -- how far behind the one ahead a follower stops

local CAMERA_SPEED = 300
local ZOOM_STEP = 1.15     -- per wheel notch
local ZOOM_MIN, ZOOM_MAX = 0.6, 2.5
local ZOOM_SMOOTH = 14     -- how fast zoom converges on the target, per second

-- Where everyone starts (the old Knight placements).
local PARTY = {
    { "Fighter", -20.6, -19.5 }, { "Archer", -139.7, 19.0 }, { "Mage", -47.2, 63.0 },
}
local BAND = {
    { "Grunt", 747.4, 364.5 }, { "Poacher", 628.3, 403.0 }, { "Hexer", 720.8, 447.0 },
}

local TEXT = {r = 235, g = 235, b = 240, a = 255}
local DIM = {r = 160, g = 160, b = 175, a = 255}
local PANEL = {r = 14, g = 14, b = 22, a = 210}

local function GroundDistance(a, b) return NavGroundDistance(a.x, a.y, b.x, b.y) end

-- --- Lifecycle ----------------------------------------------------------------------------

function Overworld:Initialize()
    self.time = 0
    self.party, self.band, self.units = {}, {}, {}
    for _, s in ipairs(PARTY) do self:Spawn(s, PLAYER, self.party) end
    for _, s in ipairs(BAND) do self:Spawn(s, ENEMY, self.band) end
    self.selected = self.party[1]
    self.cameraEntity, self.panAnchor, self.zoomTarget, self.zoomOffset = nil, nil, nil, nil
    Log("Overworld: left-click select, right-click move, wheel zoom, middle-drag pan, Esc menu")
end

function Overworld:Spawn(spec, team, list)
    local u = Units.Spawn(spec[1], team, { x = spec[2], y = spec[3], z = 0 })
    if not u then return end
    u.path, u.leg = nil, 0
    list[#list + 1] = u
    self.units[#self.units + 1] = u
end

function Overworld:Update(dt)
    self.time = self.time + dt
    self:PanWithKeys(dt)
    self:UpdateCamera(dt)

    for _, u in ipairs(self.units) do
        self:Walk(u, dt)
        Units.Update(u, dt)
        Units.DrawRing(u, u == self.selected, self.time)
    end
end

function Overworld:Render()
    FillRect(0, 0, 1280, 32, PANEL, "ui")
    DrawText("Overworld", 16, 6, 22, TEXT, "ui", FONT)
    DrawText("Left-click: select   Right-click: move   Esc: menu", 180, 9, 16, DIM, "ui", FONT)
end

function Overworld:OnEvent(event)
    if event.type == "KeyPressed" and event.key == KEY_ESCAPE then
        SceneReplace("MainMenu")
        return true
    end
    if event.type ~= "MouseButtonPressed" then return false end

    local w = ScreenToWorld(GetMousePosition())
    if event.button == MOUSE_LEFT then
        local u = self:PartyMemberAt(w.x, w.y)
        if u then self.selected = u end
        return u ~= nil
    elseif event.button == MOUSE_RIGHT and self.selected then
        local p = NavPick(w.x, w.y)
        if p then self:MoveParty(self.selected, p) end
        return true
    end
    return false
end

-- --- Movement -----------------------------------------------------------------------------

function Overworld:PartyMemberAt(x, y)
    local best, bestD
    for _, u in ipairs(self.party) do
        local d = GroundDistance(u, { x = x, y = y })
        if d <= PICK_RADIUS and (not bestD or d < bestD) then best, bestD = u, d end
    end
    return best
end

-- The leader walks to `to`; each other member walks to just behind the one before it.
function Overworld:MoveParty(leader, to)
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

function Overworld:Send(u, to)
    local path = NavFindPath(u.x, u.y, to.x, to.y, u.z)
    if #path == 0 then return end
    u.path, u.leg = path, 1
    if u.clip ~= "Walk" then Units.Play(u, "Walk") end
end

-- Advances along the path at a steady ground speed, turning to face each leg.
function Overworld:Walk(u, dt)
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
function Overworld:Camera()
    if self.cameraEntity and HasComponent(self.cameraEntity, "Camera") then return self.cameraEntity end
    local cam = GetEntityByName("CAMERA")
    if not cam then
        local found = FindEntitiesWithComponent("Camera")
        cam = found and found[1]
    end
    self.cameraEntity = cam
    return cam
end

function Overworld:PanWithKeys(dt)
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
function Overworld:UpdateCamera(dt)
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

return Overworld
