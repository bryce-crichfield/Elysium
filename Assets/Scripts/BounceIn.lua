---@type EntityScript
---
--- Slides the entity in from the left with a springy overshoot.
--- Uses easeOutBack so it travels past the target and snaps back,
--- giving buttons a satisfying game-feel entry.
--- Stagger is automatic: higher Y = slightly later arrival.
local BounceIn = {}

local SLIDE_DIST     = 420
local SLIDE_DURATION = 1
local STAGGER_SCALE  = 0.0012

-- Overshoots by ~10% then settles.  c1 controls the overshoot amount.
local function easeOutBack(t)
    local c1 = 1.70158
    local c3 = c1 + 1
    local u  = t - 1
    return 1 + c3 * u * u * u + c1 * u * u
end

function BounceIn:Initialize(entity)
    local pos = GetComponent(entity, "Position")
    if not pos then return end

    self.targetX = pos.x
    self.elapsed = 0.0
    self.done    = false
    self.delay   = math.max(0.0, (pos.y - 80.0) * STAGGER_SCALE)

    pos.x = pos.x - SLIDE_DIST
end

function BounceIn:Update(entity, dt)
    if self.done then return end

    self.elapsed = self.elapsed + dt
    local t = (self.elapsed - self.delay) / SLIDE_DURATION
    if t < 0.0 then return end

    local pos = GetComponent(entity, "Position")
    if not pos then return end

    if t >= 1.0 then
        pos.x    = self.targetX
        self.done = true
        return
    end

    pos.x = (self.targetX - SLIDE_DIST) + easeOutBack(t) * SLIDE_DIST
end

return BounceIn
