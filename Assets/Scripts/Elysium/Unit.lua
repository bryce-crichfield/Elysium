---@type EntityScript
---@class Unit
local Unit = {}

local FLASH_DURATION = 0.5  -- seconds the lost-health flash is visible

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

-- Lerp between two colors
local function LerpColor(r1, g1, b1, r2, g2, b2, t)
    return
        math.floor(r1 + (r2 - r1) * t),
        math.floor(g1 + (g2 - g1) * t),
        math.floor(b1 + (b2 - b1) * t)
end

-- Map health fraction (0..1) to red→orange→green
local function HealthColor(fill)
    local r, g, b
    if fill < 0.5 then
        -- red (200,40,40) → orange (220,160,20)
        r, g, b = LerpColor(200, 40, 40, 220, 160, 20, fill * 2)
    else
        -- orange (220,160,20) → green (60,200,60)
        r, g, b = LerpColor(220, 160, 20, 60, 200, 60, (fill - 0.5) * 2)
    end
    return r, g, b
end

function Unit:Initialize(entity)
    self.lastDirection = "south"
    self.prevHealth    = nil   -- set on first frame
    self.flashTimer    = 0.0
    self.flashFillFrom = 0.0   -- fill fraction where the flash starts
    self.flashFillTo   = 0.0   -- fill fraction where it ends (the lost portion)
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
        spr.sequence = self.lastDirection
    end

    -- Health bar
    local health = GetComponent(entity, "Health")
    local pos    = GetComponent(entity, "Position")
    if not health or not pos then return end

    local fill = math.max(0, math.min(1, health.current / health.max))

    -- Detect health loss and start flash
    if self.prevHealth == nil then
        self.prevHealth = health.current
    elseif health.current < self.prevHealth then
        local prevFill = math.max(0, math.min(1, self.prevHealth / health.max))
        self.flashFillFrom = fill
        self.flashFillTo   = prevFill
        self.flashTimer    = FLASH_DURATION
        self.prevHealth    = health.current
    else
        self.prevHealth = health.current
    end

    -- Tick flash timer
    if self.flashTimer > 0 then
        self.flashTimer = self.flashTimer - dt
    end

    -- Layout
    local barW = 32
    local barH = 4
    local barX = pos.x - barW * 0.5
    local barY = pos.y + 10   -- near the feet

    local layer = GetComponent(entity, "Layer")
    if not layer or layer.isVisible == false then
        return
    end

    -- Background (dark red trough)
    FillRect(barX, barY, barW, barH, {r=40, g=10, b=10, a=200}, "entity")

    -- Health fill with gradient color
    if fill > 0 then
        local r, g, b = HealthColor(fill)
        FillRect(barX, barY, barW * fill, barH, {r=r, g=g, b=b, a=220}, "entity")
    end

    -- Lost-health flash (white rect over the drained portion, fades out)
    if self.flashTimer > 0 then
        local t = self.flashTimer / FLASH_DURATION          -- 1→0
        local flashA = math.floor(220 * t)
        local flashX = barX + barW * self.flashFillFrom
        local flashW = barW * (self.flashFillTo - self.flashFillFrom)
        FillRect(flashX, barY, flashW, barH, {r=255, g=255, b=255, a=flashA}, "entity")
    end
end

function Unit:OnEvent(entity, event)
end

return Unit
