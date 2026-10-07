---@type EntityScript
---@class Button
-- Prefabs/Button.xml: a menu button. The scene gives it its action by the placement id
-- (Widgets.BindButton(id, onClick [, enabled])); everything else is the button's own:
--   hover:   the hover sound, the Fire glow fades in, the edge and label turn gold
--   press:   the face squashes a little about its centre
--   release: over the button, the click sound, a flare of the glow, then the action
-- A disabled button (its `enabled` says no) greys out and ignores the pointer.
local Widgets = require("Scripts/Components/Widgets")
local Sfx = require("Scripts/Menu/Sfx")

local Button = {}

local GLOW_HOVER = 1.3     -- the glow's intensity while hovered
local GLOW_FLARE = 3.0     -- what a click flares it to, settling back to GLOW_HOVER
local GLOW_RATE = 12       -- how fast the glow eases toward its target, per second
local PRESS_SCALE = 0.95   -- the face's scale while held down
local SCALE_RATE = 25

-- The face: a gradient from near-black on the left to a faint warm tint on the right, which
-- warms toward ember while lit.
local FACE_A, FACE_B = {r = 0, g = 0, b = 0, a = 235}, {r = 31, g = 20, b = 8, a = 64}
local FACE_HOVER_A, FACE_HOVER_B = {r = 46, g = 28, b = 5, a = 245}, {r = 90, g = 50, b = 10, a = 110}
local TEXT = {r = 235, g = 235, b = 240, a = 255}
local OFF = {r = 90, g = 90, b = 100, a = 255}

local Ease = Widgets.Ease

function Button:Initialize(entity)
    self.id = Widgets.PlacementId(entity)
    self.label = Widgets.Child(entity, "Label")
    self.hover, self.armed = false, false
    self.glow, self.flare, self.scale = 0, 0, 1
    -- Script instances are kept by entity id, which a later scene reuses: drop the last
    -- button's position, or this one jumps to it.
    self.home = nil
end

function Button:Update(entity, dt)
    local binding = self.id and Widgets.Binding(self.id)
    -- A hidden button (the scene hid it) takes no input and makes no sound.
    if not Widgets.Visible(entity) then
        self.hover, self.armed, self.glow, self.flare = false, false, 0, 0
        Widgets.SetHovered(self.id, false)
        return
    end
    local enabled = binding and (not binding.enabled or binding.enabled())
    local active = binding and binding.active and binding.active()
    local t = GetComponent(entity, "Transform")
    local r = GetComponent(entity, "Rectangle")
    if not t or not r then return end
    -- Where the placement put it, before any squash moved it.
    self.home = self.home or { x = t.localX, y = t.localY }

    -- Where its anchor keeps it on this screen (Widgets.Anchor).
    local dx, dy = 0, 0
    if binding and binding.anchor then dx, dy = Widgets.Shift(binding.anchor[1], binding.anchor[2]) end
    local hx, hy = self.home.x + dx, self.home.y + dy

    -- Hit-test the unsquashed face, so the edge doesn't flicker in and out while held.
    local x, y = t.worldX - (t.localX - hx), t.worldY - (t.localY - hy)
    local w, h = r.width, r.height
    local m = GetMousePosition()
    local hover = enabled and Widgets.Inside(m, x, y, w, h) or false

    if hover and not self.hover then Sfx.Play(Sfx.HOVER, 0.5) end
    self.hover = hover
    Widgets.SetHovered(self.id, hover)
    if hover and IsMouseButtonPressed(MOUSE_LEFT) then self.armed = true end
    if IsMouseButtonReleased(MOUSE_LEFT) then
        if self.armed and hover then
            Sfx.Play(Sfx.CLICK)
            self.flare = 1
            if binding.onClick then binding.onClick() end
        end
        self.armed = false
    end

    -- Glow: up to GLOW_HOVER while hovered, plus a click's flare settling away.
    self.flare = math.max(0, self.flare - dt * 3)
    local lit = hover or active
    self.glow = Ease(self.glow, lit and GLOW_HOVER or 0, GLOW_RATE, dt)
    local mat = GetComponent(entity, "Material")
    if mat then
        local fire = mat:Layer("Fire")
        if fire then fire:Set("uIntensity", self.glow + self.flare * (GLOW_FLARE - GLOW_HOVER)) end
        local face = mat:Layer("Gradient")
        if face then
            face:Set("uColorA", lit and FACE_HOVER_A or FACE_A)
            face:Set("uColorB", lit and FACE_HOVER_B or FACE_B)
        end
        local stroke = mat:Layer("Stroke")
        if stroke then stroke:Set("uColor", lit and Widgets.HOVER or Widgets.EDGE) end
    end

    -- Squash about the centre while held.
    self.scale = Ease(self.scale, (self.armed and hover) and PRESS_SCALE or 1, SCALE_RATE, dt)
    t.localScaleX, t.localScaleY = self.scale, self.scale
    t.localX = hx + w * (1 - self.scale) / 2
    t.localY = hy + h * (1 - self.scale) / 2

    -- The label, centered on the face (text draws centered on its position).
    local lt = self.label and GetComponent(self.label, "Transform")
    if lt then lt.localX, lt.localY = w / 2, h / 2 end
    local text = self.label and GetComponent(self.label, "Text")
    if text then text.color = enabled and (lit and Widgets.HOVER or TEXT) or OFF end
end

return Button
