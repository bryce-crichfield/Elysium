---@type EntityScript
---@class Slider
-- Prefabs/Slider.xml: a 0..1 value (Widgets.Bind with the placement id) shown by the root's
-- Meter fill; press anywhere on the track and drag to set it. The Knob child rides the fill's
-- end and the Value child shows the percentage. Hovered or dragged, the track burns (the root's
-- Fire layer).
local Widgets = require("Scripts/Components/Widgets")

local Slider = {}

local GRAB = 14         -- extra pixels above and below the track that still catch a press
local GLOW = 1.2        -- the track's glow while hovered
local GLOW_DRAG = 1.4   -- scaled up by this while dragging
local GLOW_RATE = 12

function Slider:Initialize(entity)
    self.id = Widgets.PlacementId(entity)
    self.knob = Widgets.Child(entity, "Knob")
    self.value = Widgets.Child(entity, "Value")
    self.label = Widgets.Child(entity, "Label")
    self.dragging = false
    self.glow = 0
end

function Slider:Update(entity, dt)
    local binding = self.id and Widgets.Binding(self.id)
    local x, y, w, h = Widgets.Rect(entity)
    local m = GetMousePosition()
    local hover = Widgets.Inside(m, x, y - GRAB, w, h + 2 * GRAB)

    if binding and hover and IsMouseButtonPressed(MOUSE_LEFT) then self.dragging = true end
    if not IsMouseButtonDown(MOUSE_LEFT) then self.dragging = false end
    if self.dragging and w > 0 then binding.set(math.max(0, math.min(1, (m.x - x) / w))) end
    local v = binding and binding.get() or 0

    local mat = GetComponent(entity, "Material")
    local meter = mat and mat:Layer("Meter")
    if meter then meter:Set("uProgress", v) end

    local t = GetComponent(entity, "Transform")
    local lit = hover or self.dragging
    self.glow = Widgets.Ease(self.glow, lit and (self.dragging and GLOW_DRAG or 1) or 0, GLOW_RATE, dt)

    Widgets.SetGlow(entity, self.glow * GLOW)

    -- The knob is centered on the fill's end.
    local kt = self.knob and GetComponent(self.knob, "Transform")
    local kr = self.knob and GetComponent(self.knob, "Rectangle")
    if kt and kr and t then
        kt.localX = v * w / t.worldScaleX - kr.width / 2
        local kmat = GetComponent(self.knob, "Material")
        local fill = kmat and kmat:Layer("Flat")
        if fill then fill:Set("uColor", lit and Widgets.HOVER or {r = 235, g = 235, b = 240, a = 255}) end
    end

    Widgets.AlignLeft(self.label, 0)
    local text = self.value and GetComponent(self.value, "Text")
    if text then text.content = string.format("%d%%", math.floor(v * 100 + 0.5)) end
end

return Slider
