---@type EntityScript
---
--- Slides the entity in from the left to its authored X position.
--- Attach to any entity with a Position component.
---
--- Stagger is automatic: elements with a higher Y value arrive slightly later,
--- so a vertical stack of buttons cascades down naturally without any extra config.
local SlideIn = {}

local SLIDE_DIST     = 420    -- px off-screen to the left of final position
local SLIDE_DURATION = 2   -- seconds for the slide itself
local STAGGER_SCALE  = 0.0012 -- seconds of delay per pixel of Y below the top element

local function easeOutCubic(t)
    local u = 1.0 - t
    return 1.0 - u * u * u
end

function SlideIn:Initialize(entity)
    local pos = GetComponent(entity, "Position")
    if not pos then return end

    self.targetX = pos.x
    self.elapsed = 0.0
    self.done    = false
    -- Elements higher on screen (smaller Y) arrive first.
    -- Anchor the stagger at y=80 so the topmost element has zero delay.
    self.delay   = math.max(0.0, (pos.y - 80.0) * STAGGER_SCALE)

    pos.x = pos.x - SLIDE_DIST
end

function SlideIn:Update(entity, dt)
    if self.done then return end

    self.elapsed = self.elapsed + dt

    local t = (self.elapsed - self.delay) / SLIDE_DURATION
    if t < 0.0 then return end  -- still in stagger window

    local pos = GetComponent(entity, "Position")
    if not pos then return end

    if t >= 1.0 then
        pos.x     = self.targetX
        self.done = true
        return
    end

    pos.x = (self.targetX - SLIDE_DIST) + easeOutCubic(t) * SLIDE_DIST
end

return SlideIn
