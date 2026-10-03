---@type EntityScript
---@class TextField
-- Prefabs/TextField.xml: one line of typed text (letters, digits, . - :), bound by the
-- placement id: Widgets.Bind(id, get, set) with strings. Click to focus (a caret blinks and
-- the box burns); Enter, Esc or a click outside drops focus. Keys are polled here rather
-- than taken from events, so it isn't limited to the keys the engine sends as events.
local Widgets = require("Scripts/Components/Widgets")
local Sfx = require("Scripts/Menu/Sfx")

local TextField = {}

local MAX_LEN = 32
local TEXT_LEFT = 12

-- The characters keys type (GLFW: printable keys are their ASCII codes).
local KEYS = {}
for c = 48, 57 do KEYS[c] = string.char(c) end           -- 0-9
for c = 65, 90 do KEYS[c] = string.char(c):lower() end   -- a-z
KEYS[46], KEYS[45], KEYS[59] = ".", "-", ":"

function TextField:Initialize(entity)
    self.id = Widgets.PlacementId(entity)
    self.label = Widgets.Child(entity, "Label")
    self.text = Widgets.Child(entity, "Text")
    self.focused, self.hover = false, false
    self.glow, self.time = 0, 0
end

function TextField:Update(entity, dt)
    self.time = self.time + dt
    local binding = self.id and Widgets.Binding(self.id)
    local x, y, w, h = Widgets.Rect(entity)
    local hover = Widgets.Inside(GetMousePosition(), x, y, w, h)
    if hover and not self.hover then Sfx.Play(Sfx.HOVER, 0.5) end
    self.hover = hover

    if IsMouseButtonPressed(MOUSE_LEFT) then
        if hover and not self.focused then Sfx.Play(Sfx.CLICK) end
        self.focused = hover
    end

    if self.focused and binding then
        local s = binding.get() or ""
        if IsKeyPressed(KEY_ENTER) or IsKeyPressed(KEY_ESCAPE) then
            self.focused = false
        elseif IsKeyPressed(KEY_BACKSPACE) then
            binding.set(s:sub(1, -2))
        else
            for key, ch in pairs(KEYS) do
                if IsKeyPressed(key) and #s < MAX_LEN then s = s .. ch end
            end
            if s ~= binding.get() then binding.set(s) end
        end
    end

    Widgets.SetTyping(self.id, self.focused)

    local lit = hover or self.focused
    self.glow = Widgets.Ease(self.glow, self.focused and 1.3 or (hover and 0.8 or 0), 12, dt)
    Widgets.SetGlow(entity, self.glow)
    local mat = GetComponent(entity, "Material")
    local stroke = mat and mat:Layer("Stroke")
    if stroke then stroke:Set("uColor", lit and Widgets.HOVER or Widgets.EDGE) end

    local caret = self.focused and (math.floor(self.time * 2) % 2 == 0) and "_" or ""
    Widgets.ChildText(entity, "Text", ((binding and binding.get()) or "") .. caret, nil, TEXT_LEFT)
    Widgets.AlignLeft(self.label, 0)
end

return TextField
