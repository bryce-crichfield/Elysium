---@type EntityScript
---@class Unit
local Unit = {}

-- Compute 8-direction name from velocity vector
local function GetDirectionName(vx, vy)
    local speed = math.sqrt(vx * vx + vy * vy)
    if speed < 1.0 then
        return nil  -- Not moving enough to determine direction
    end

    -- atan2 gives angle in radians, convert to degrees
    -- In screen coords: +x is right, +y is down
    local angle = math.atan(vy, vx) * (180 / math.pi)
    if angle < 0 then
        angle = angle + 360
    end

    -- Map angle to 8 directions
    -- 0° = east, 90° = south, 180° = west, 270° = north
    if angle < 22.5 or angle >= 337.5 then
        return "east"
    elseif angle < 67.5 then
        return "southeast"
    elseif angle < 112.5 then
        return "south"
    elseif angle < 157.5 then
        return "southwest"
    elseif angle < 202.5 then
        return "west"
    elseif angle < 247.5 then
        return "northwest"
    elseif angle < 292.5 then
        return "north"
    else
        return "northeast"
    end
end

function Unit:Initialize(entity)
    -- Store last known direction so we keep facing that way when idle
    self.lastDirection = "south"
end

function Unit:Update(entity, dt)
    local kin = GetComponent(entity, "Kinematics")
    local spr = GetComponent(entity, "Sprite")

    if not kin or not spr then
        return
    end

    local vx = kin.velocity.x
    local vy = kin.velocity.y
    local speed = math.sqrt(vx * vx + vy * vy)

    -- Determine sheet based on movement
    local movementThreshold = 1.0
    if speed > movementThreshold * 25 then
        spr.sheet = "Run"
        spr.duration = 0.08
    elseif speed > movementThreshold then
        spr.sheet = "Walk"
        spr.duration = 0.15
    else
        spr.sheet = "Idle"
        spr.duration = 0.2
    end

    -- Determine sequence (direction) from velocity
    local direction = GetDirectionName(vx, vy)
    if direction then
        self.lastDirection = direction
        spr.sequence = direction
    else
        -- Keep facing last direction when stopped
        spr.sequence = self.lastDirection
    end
end

function Unit:OnEvent(entity, event)
end

return Unit

