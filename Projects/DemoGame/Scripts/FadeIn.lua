---@type EntityScript
---
--- Fades the entity in from fully transparent to its authored alpha.
--- Works on TextComponent and on every MaterialComponent layer with a uColor, or both.
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
    local pos = GetComponent(entity, "Transform")
    self.elapsed  = 0.0
    self.done     = false
    self.delay    = pos and math.max(0.0, (pos.localY - 80.0) * STAGGER_SCALE) or 0.0

    -- Capture and zero the alpha on Text
    local text = GetComponent(entity, "Text")
    if text then
        local c = text.color
        self.textAlpha = c.a
        c.a = 0
        text.color = c
    end

    -- Capture and zero the alpha of every material layer's color
    local mat = GetComponent(entity, "Material")
    if mat then
        self.layerAlphas = {}
        for i = 1, mat.layerCount do
            local c = mat:LayerAt(i):Get("uColor")
            if c then self.layerAlphas[i] = c.a end
        end
        self:SetLayerAlpha(mat, 0)
    end
end

-- Sets each captured layer's color alpha to its authored alpha scaled by `k` (0..1).
function FadeIn:SetLayerAlpha(mat, k)
    for i, alpha in pairs(self.layerAlphas) do
        local layer = mat:LayerAt(i)
        local c = layer and layer:Get("uColor")
        if c then
            c.a = math.floor(alpha * k)
            layer:Set("uColor", c)
        end
    end
end

function FadeIn:Update(entity, dt)
    if self.done then return end

    self.elapsed = self.elapsed + dt
    local t = (self.elapsed - self.delay) / FADE_DURATION
    if t < 0.0 then return end

    local text = GetComponent(entity, "Text")
    local mat  = GetComponent(entity, "Material")

    if t >= 1.0 then
        -- Snap to authored values
        if text and self.textAlpha then
            local c = text.color; c.a = self.textAlpha; text.color = c
        end
        if mat and self.layerAlphas then self:SetLayerAlpha(mat, 1) end
        self.done = true
        return
    end

    local e = easeInOutQuad(t)

    if text and self.textAlpha then
        local c = text.color
        c.a = math.floor(lerp(0, self.textAlpha, e))
        text.color = c
    end

    if mat and self.layerAlphas then self:SetLayerAlpha(mat, e) end
end

return FadeIn
