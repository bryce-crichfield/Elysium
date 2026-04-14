---@type EntityScript
---
--- Fades the entity in from fully transparent to its authored alpha.
--- Works on TextComponent and RectangleComponent (background + border), or both.
--- Stagger is automatic: elements with higher Y values start slightly later.
local FadeIn = {}

local FADE_DURATION  = 1
local STAGGER_SCALE  = 0.0014

local function easeInOutQuad(t)
    if t < 0.5 then return 2 * t * t
    else return 1 - (-2 * t + 2)^2 * 0.5 end
end

local function lerp(a, b, t) return a + (b - a) * t end

function FadeIn:Initialize(entity)
    local pos = GetComponent(entity, "Position")
    self.elapsed  = 0.0
    self.done     = false
    self.delay    = pos and math.max(0.0, (pos.y - 80.0) * STAGGER_SCALE) or 0.0

    -- Capture and zero the alpha on Text
    local text = GetComponent(entity, "Text")
    if text then
        local c = text.color
        self.textAlpha = c.a
        c.a = 0
        text.color = c
    end

    -- Capture and zero the alpha on Rectangle (background and border separately)
    local rect = GetComponent(entity, "Rectangle")
    if rect then
        local bg = rect.background
        local bd = rect.border
        self.bgAlpha = bg.a
        self.bdAlpha = bd.a
        bg.a = 0
        bd.a = 0
        rect.background = bg
        rect.border     = bd
    end
end

function FadeIn:Update(entity, dt)
    if self.done then return end

    self.elapsed = self.elapsed + dt
    local t = (self.elapsed - self.delay) / FADE_DURATION
    if t < 0.0 then return end

    local text = GetComponent(entity, "Text")
    local rect = GetComponent(entity, "Rectangle")

    if t >= 1.0 then
        -- Snap to authored values
        if text and self.textAlpha then
            local c = text.color; c.a = self.textAlpha; text.color = c
        end
        if rect and self.bgAlpha then
            local bg = rect.background; bg.a = self.bgAlpha; rect.background = bg
            local bd = rect.border;     bd.a = self.bdAlpha; rect.border     = bd
        end
        self.done = true
        return
    end

    local e = easeInOutQuad(t)

    if text and self.textAlpha then
        local c = text.color
        c.a = math.floor(lerp(0, self.textAlpha, e))
        text.color = c
    end

    if rect and self.bgAlpha then
        local bg = rect.background
        local bd = rect.border
        bg.a = math.floor(lerp(0, self.bgAlpha, e))
        bd.a = math.floor(lerp(0, self.bdAlpha, e))
        rect.background = bg
        rect.border     = bd
    end
end

return FadeIn
