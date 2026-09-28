---@type EntityScript
---@class Monster : Unit
-- A Unit that wanders on its own: waits a moment, picks a random waypoint near where it
-- started, walks there, and repeats. Unit handles the sprite (sheet + facing) and health bar.
--
-- It also drives two children of the Monster prefab through their materials: "Selection"
-- (a glowing ring) is on while walking, and "Cooldown" (a Meter) fills up while it waits.
local Unit = require("Scripts/Elysium/Unit")
local Monster = setmetatable({}, {__index = Unit})

local WANDER_RADIUS = 200   -- world units around the spawn point
local WAIT_MIN      = 1.0   -- seconds idle between waypoints
local WAIT_MAX      = 3.0
local MAX_SPEED     = 90    -- used only when the monster has no Kinematics of its own
local FRICTION      = 5

-- The child whose name ends in "::<name>" (prefab members are namespaced by placement).
local function FindChild(entity, name)
    for _, child in ipairs(FindEntitiesWithComponent("Parent")) do
        local parent = GetComponent(child, "Parent")
        local childName = GetComponent(child, "Name")
        if parent and parent.parent == entity and childName then
            local n = childName.name
            if n == name or n:sub(-(#name + 2)) == "::" .. name then return child end
        end
    end
    return nil
end

local function SetMaterialEnabled(entity, enabled)
    local mat = entity and GetComponent(entity, "Material")
    if mat then mat.enabled = enabled end
end

local function RandomPointNear(cx, cy, radius)
    local angle = math.random() * 2 * math.pi
    local dist  = math.sqrt(math.random()) * radius  -- sqrt: uniform over the disc
    return cx + math.cos(angle) * dist, cy + math.sin(angle) * dist
end

function Monster:Initialize(entity)
    Unit.Initialize(self, entity)

    -- Moving needs Kinematics (velocity) and Movement (the path); add whichever is missing.
    if not GetComponent(entity, "Kinematics") then
        AddComponent(entity, "Kinematics")
        local kin = GetComponent(entity, "Kinematics")
        kin.maxSpeed = MAX_SPEED
        kin.friction = FRICTION
    end
    if not GetComponent(entity, "Movement") then
        AddComponent(entity, "Movement")
    end

    local pos = GetComponent(entity, "Transform")
    self.homeX = pos and pos.worldX or 0
    self.homeY = pos and pos.worldY or 0

    self.phase     = "waiting"
    self.waitTime  = math.random() * WAIT_MAX  -- stagger, so monsters don't all set off together
    self.waitTotal = self.waitTime
end

-- Children are looked up until found: their parent links may not be resolved yet at
-- Initialize, or on the first frames.
function Monster:FindParts(entity)
    if not self.selection then self.selection = FindChild(entity, "Selection") end
    if not self.cooldown then
        self.cooldown = FindChild(entity, "Cooldown")
        local mat = self.cooldown and GetComponent(self.cooldown, "Material")
        local meter = mat and mat:Layer("Meter")
        if meter then meter:Set("uAutoSpeed", 0) end  -- the script drives it
    end
end

-- Walking: the selection ring glows. Waiting: the cooldown meter shows, filling as the
-- wait runs out.
function Monster:ShowParts()
    local moving = self.phase == "moving"
    SetMaterialEnabled(self.selection, moving)
    SetMaterialEnabled(self.cooldown, not moving)

    local mat = self.cooldown and GetComponent(self.cooldown, "Material")
    local meter = mat and mat:Layer("Meter")
    if meter and not moving then
        local progress = 1 - self.waitTime / math.max(self.waitTotal, 0.001)
        meter:Set("uProgress", math.max(0, math.min(1, progress)))
    end
end

function Monster:Update(entity, dt)
    Unit.Update(self, entity, dt)

    self:FindParts(entity)
    self:ShowParts()

    local mv = GetComponent(entity, "Movement")
    if not mv then return end

    if self.phase == "moving" then
        if mv.state == 0 then  -- Idle: arrived, or no path there
            self.phase     = "waiting"
            self.waitTime  = WAIT_MIN + math.random() * (WAIT_MAX - WAIT_MIN)
            self.waitTotal = self.waitTime
        end
    else
        self.waitTime = self.waitTime - dt
        if self.waitTime <= 0 then
            local x, y = RandomPointNear(self.homeX, self.homeY, WANDER_RADIUS)
            IssueMoveCommand(entity, x, y)
            self.phase = "moving"
        end
    end
end

return Monster
